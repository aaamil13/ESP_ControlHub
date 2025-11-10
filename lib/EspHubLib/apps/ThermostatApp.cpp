#include "ThermostatApp.h"
#include <StreamLogger.h>

extern StreamLogger* EspHubLog;

ThermostatApp::ThermostatApp() : _setpoint(20.0f), _hysteresis(0.5f) {
}

bool ThermostatApp::configure(const JsonObject& config, PlcEngine& plcEngine) {
    _tempSensorVar = config["temp_sensor"].as<std::string>();
    _heaterOutputVar = config["heater_output"].as<std::string>();
    _setpoint = config["setpoint"] | 20.0f;
    _hysteresis = config["hysteresis"] | 0.5f;

    // Declare PLC variables needed by this app
    plcEngine.getMemory().declareVariable(_tempSensorVar, PlcValueType::REAL, false);
    plcEngine.getMemory().declareVariable(_heaterOutputVar, PlcValueType::BOOL, true); // Heater output should be retentive

    // Here, we would dynamically generate PLC logic blocks for the thermostat
    // For example, using GT and LT blocks to compare temperature with setpoint and hysteresis
    // This is a simplified example, a full implementation would add these blocks to the PlcEngine's logic_blocks
    EspHubLog->printf("ThermostatApp configured: Temp Sensor: %s, Heater Output: %s, Setpoint: %.1f, Hysteresis: %.1f\n",
                _tempSensorVar.c_str(), _heaterOutputVar.c_str(), _setpoint, _hysteresis);
    return true;
}

void ThermostatApp::setupWebServer(AsyncWebServer& server) {
    server.on("/thermostat_config", HTTP_GET, [&](AsyncWebServerRequest *request){
        String html = "<h1>Thermostat Configuration</h1>";
        html += "<p>Temp Sensor: " + String(_tempSensorVar.c_str()) + "</p>";
        html += "<p>Heater Output: " + String(_heaterOutputVar.c_str()) + "</p>";
        html += "<p>Setpoint: " + String(_setpoint) + "</p>";
        html += "<p>Hysteresis: " + String(_hysteresis) + "</p>";
        request->send(200, "text/html", html);
    });
}

void ThermostatApp::update() {
    // This update method would typically not contain direct control logic,
    // but rather interact with PLC variables that are part of the PlcEngine's evaluation cycle.
    // For demonstration, we'll simulate direct control here.

    // float currentTemp = _plcEngine->getMemory().getValue<float>(_tempSensorVar, 0.0f);
    // bool heaterState = _plcEngine->getMemory().getValue<bool>(_heaterOutputVar, false);

    // if (currentTemp < (_setpoint - _hysteresis / 2.0f) && !heaterState) {
    //     _plcEngine->getMemory().setValue<bool>(_heaterOutputVar, true); // Turn heater on
    //     EspHubLog->printf("Thermostat: Temp %.1f < Setpoint %.1f, turning heater ON\n", currentTemp, _setpoint);
    // } else if (currentTemp > (_setpoint + _hysteresis / 2.0f) && heaterState) {
    //     _plcEngine->getMemory().setValue<bool>(_heaterOutputVar, false); // Turn heater off
    //     EspHubLog->printf("Thermostat: Temp %.1f > Setpoint %.1f, turning heater OFF\n", currentTemp, _setpoint);
    // }
}