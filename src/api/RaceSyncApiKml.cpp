#include "RaceSyncApi.h"

#include <math.h>

namespace
{
bool findVBoxDataSection(File& file)
{
    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line == "[data]")
        {
            return true;
        }
    }
    return false;
}

bool parseVBoxCoordinate(const String& line, double& longitude, double& latitude, double& height)
{
    unsigned int satellites = 0;
    double time = 0.0;
    double rawLatitude = 0.0;
    double rawLongitude = 0.0;
    double velocity = 0.0;
    double heading = 0.0;

    const int fields = sscanf(
        line.c_str(),
        "%u %lf %lf %lf %lf %lf %lf",
        &satellites,
        &time,
        &rawLatitude,
        &rawLongitude,
        &velocity,
        &heading,
        &height
    );

    if (fields != 7)
    {
        return false;
    }

    // RaceSync VBO stores latitude as decimal degrees * 60 and longitude
    // as decimal degrees * -60 to retain VBOX compatibility.
    latitude = rawLatitude / 60.0;
    longitude = rawLongitude / -60.0;

    return
        isfinite(latitude) &&
        isfinite(longitude) &&
        isfinite(height) &&
        latitude >= -90.0 && latitude <= 90.0 &&
        longitude >= -180.0 && longitude <= 180.0;
}
}

void RaceSyncApi::handleSessionKmlDownloadById(uint32_t sessionId)
{
    String vboFilename;
    if (!_storage.findSessionById(sessionId, vboFilename))
    {
        sendJson(404, "{\"error\":\"Session not found\"}");
        return;
    }

    if (_logger.recording() && _logger.currentFilename() == vboFilename)
    {
        sendJson(409, "{\"error\":\"KML cannot be generated while the session is recording\"}");
        return;
    }

    File vbo = _storage.openRead(vboFilename);
    if (!vbo)
    {
        sendJson(404, "{\"error\":\"VBO session file not found\"}");
        return;
    }

    if (!findVBoxDataSection(vbo))
    {
        vbo.close();
        sendJson(422, "{\"error\":\"VBO data section not found\"}");
        return;
    }

    String kmlFilename = vboFilename.substring(0, vboFilename.length() - 4) + ".kml";

    addCorsHeaders();
    _server.sendHeader("Content-Disposition", "attachment; filename=\"" + kmlFilename + "\"");
    _server.sendHeader("X-RaceSync-Session-Id", String(sessionId));
    _server.sendHeader("X-RaceSync-KML-Source", "ON_DEMAND_VBO");
    _server.sendHeader("Cache-Control", "no-store");
    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "application/vnd.google-earth.kml+xml", "");

    String header;
    header.reserve(512);
    header += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    header += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n<Document>\n<name>";
    header += kmlFilename;
    header += "</name>\n";
    header += "<Style id=\"RaceSyncTrack\"><LineStyle><width>4</width></LineStyle></Style>\n";
    header += "<Placemark><name>RaceSync GPS Track</name><styleUrl>#RaceSyncTrack</styleUrl>\n";
    header += "<LineString><tessellate>1</tessellate><altitudeMode>absolute</altitudeMode><coordinates>\n";
    _server.sendContent(header);

    String chunk;
    chunk.reserve(1024);
    uint32_t points = 0;

    while (vbo.available())
    {
        String line = vbo.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
        {
            continue;
        }

        double longitude;
        double latitude;
        double height;
        if (!parseVBoxCoordinate(line, longitude, latitude, height))
        {
            continue;
        }

        char coordinate[80];
        const int length = snprintf(
            coordinate,
            sizeof(coordinate),
            "%.7f,%.7f,%.2f\n",
            longitude,
            latitude,
            height
        );

        if (length <= 0 || length >= static_cast<int>(sizeof(coordinate)))
        {
            continue;
        }

        if (chunk.length() + static_cast<size_t>(length) > 960)
        {
            _server.sendContent(chunk);
            chunk = "";
            delay(0);
        }

        chunk += coordinate;
        ++points;
    }

    if (chunk.length())
    {
        _server.sendContent(chunk);
    }

    vbo.close();

    String footer;
    footer.reserve(160);
    footer += "</coordinates></LineString></Placemark>\n";
    footer += "<ExtendedData><Data name=\"points\"><value>";
    footer += String(points);
    footer += "</value></Data></ExtendedData>\n";
    footer += "</Document>\n</kml>\n";
    _server.sendContent(footer);
    _server.sendContent("");

    Serial.printf(
        "[KML] Generated %s on demand from %s (%lu points)\n",
        kmlFilename.c_str(),
        vboFilename.c_str(),
        static_cast<unsigned long>(points)
    );
}

void RaceSyncApi::beginKmlDownloadRoute()
{
    _server.on(
        "/api/session-kml",
        HTTP_GET,
        [this]()
        {
            if (!_server.hasArg("id"))
            {
                sendJson(400, "{\"error\":\"Missing session id\"}");
                return;
            }

            String idValue = _server.arg("id");
            if (idValue.length() == 0)
            {
                sendJson(400, "{\"error\":\"Invalid session id\"}");
                return;
            }

            for (size_t i = 0; i < idValue.length(); ++i)
            {
                if (idValue[i] < '0' || idValue[i] > '9')
                {
                    sendJson(400, "{\"error\":\"Invalid session id\"}");
                    return;
                }
            }

            uint32_t sessionId = static_cast<uint32_t>(strtoul(idValue.c_str(), nullptr, 10));
            if (sessionId == 0)
            {
                sendJson(400, "{\"error\":\"Invalid session id\"}");
                return;
            }

            handleSessionKmlDownloadById(sessionId);
        }
    );
}
