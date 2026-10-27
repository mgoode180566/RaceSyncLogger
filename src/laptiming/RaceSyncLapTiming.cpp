#include "RaceSyncLapTiming.h"

void RaceSyncLapTiming::begin(RaceSyncStorage& storage) {
    _database.begin(storage);
    _writer.begin(storage);
}

void RaceSyncLapTiming::startSession(const String& vboFilename) {
    stopSession();
    _selected=-1;
    _selection="waiting_for_circuit";
    _timer=RaceSyncLaps::LapTimer();
    const bool loaded=_database.load();
    if(!_writer.startSession(vboFilename,_database.snapshot(),loaded,_database.error())) return;
    if(!loaded) {
        _selection="invalid_configuration";
        _writer.close();
        Serial.println("[LAPS] "+_database.error());
    }
}

void RaceSyncLapTiming::selectCircuit(const RaceSyncLaps::Point& position) {
    const auto result=RaceSyncLaps::CircuitSelector::select(
        position,_database.locations(),_database.preferredIndex());
    if(result.state==RaceSyncLaps::CircuitSelection::State::Ambiguous) {
        _selection="ambiguous_circuits: set selectedCircuitId";
        return;
    }
    if(result.state!=RaceSyncLaps::CircuitSelection::State::Selected) return;
    _selected=result.index;
    const CircuitDefinition& circuit=_database.circuit(_selected);
    _timer.configure(circuit.settings);
    _selection="selected";
    Serial.println("[LAPS] Circuit selected: "+circuit.id);
}

void RaceSyncLapTiming::process(const Telemetry& telemetry) {
    if(!_writer.active()) return;
    const auto sample=GpsSampleAdapter::convert(telemetry);
    if(!sample.valid || !RaceSyncLaps::validPoint(sample.position)) {
        _timer.invalidate();
        return;
    }
    if(_selected<0) selectCircuit(sample.position);
    if(_selected<0) return;
    const auto crossing=_timer.process(sample);
    if(!_writer.append(_database.circuit(_selected).id,crossing)) _timer.invalidate();
}

void RaceSyncLapTiming::stopSession() {
    _writer.close();
    _timer.invalidate();
}

void RaceSyncLapTiming::addStatus(JsonObject status) const {
    status["circuitId"]=_selected>=0 ? _database.circuit(_selected).id : String("");
    status["selection"]=_selection;
    status["lapFile"]=_writer.filename();
    status["completedLaps"]=_timer.completedLaps();
    status["lapRunning"]=_timer.lapRunning();
    status["writeErrors"]=_writer.writeErrors();
    status["error"]=_writer.error().length() ? _writer.error() : _database.error();
}
