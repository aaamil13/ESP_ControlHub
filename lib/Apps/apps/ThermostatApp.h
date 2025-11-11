#ifndef THERMOSTAT_APP_H
#define THERMOSTAT_APP_H

#include "AppModule.h"
#include <string>

class ThermostatApp : public AppModule {
public:
    ThermostatApp();
    bool configure(const JsonObject& config, PlcEngine& plcEngine) override;
    void setupWebServer(AsyncWebServer& server) override;
    void update() override;

private:
    std::string _tempSensorVar;
    std::string _heaterOutputVar;
    float _setpoint;
    float _hysteresis;
};

#endif // THERMOSTAT_APP_H