#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include "apps/AppModule.h"
#include "../PlcCore/PlcEngine.h"
#include "apps/ThermostatApp.h" // Include new app module

class AppManager {
public:
    AppManager();
    void begin(PlcEngine& plcEngine, AsyncWebServer& server); // Pass server to AppManager
    void loadApplications(const JsonObject& config);
    void updateAll();

private:
    PlcEngine* _plcEngine;
    AsyncWebServer* _server; // Store server pointer
    std::vector<std::unique_ptr<AppModule>> app_instances;
};

#endif // APP_MANAGER_H