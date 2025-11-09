#include "AppManager.h"

AppManager::AppManager() : _plcEngine(nullptr), _server(nullptr) {
}

void AppManager::begin(PlcEngine& plcEngine, AsyncWebServer& server) {
    _plcEngine = &plcEngine;
    _server = &server;
}

void AppManager::loadApplications(const JsonObject& config) {
    if (!_plcEngine || !_server) return;

    app_instances.clear();

    if (config.containsKey("applications")) {
        JsonArray apps = config["applications"].as<JsonArray>();
        for (JsonObject app_config : apps) {
            const char* type = app_config["type"];
            
            if (strcmp(type, "thermostat") == 0) {
                auto app = std::make_unique<ThermostatApp>();
                if (app->configure(app_config, *_plcEngine)) {
                    app->setupWebServer(*_server); // Setup web server routes for this app
                    app_instances.push_back(std::move(app));
                    Log->printf("Loaded ThermostatApp.\n");
                } else {
                    Log->printf("ERROR: Failed to configure ThermostatApp.\n");
                }
            }
            // Add other app types here with else if

            Log->printf("Loading application of type: %s\n", type);
        }
    }
}

void AppManager::updateAll() {
    for (auto& app : app_instances) {
        app->update();
    }
}