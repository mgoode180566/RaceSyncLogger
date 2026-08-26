#include <Arduino.h>
#include "app/RaceSyncController.h"

RaceSyncController app;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    app.begin();
}

void loop()
{
    app.update();
}
