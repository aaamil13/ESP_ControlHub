#include "AppManager.h"
// We will include specific app modules here later
// #include "apps/ThermostatApp.h"

AppManager::AppManager() : _plcEngine(nullptr) {
}

void AppManager::begin(PlcEngine& plcEngine) {
    _plcEngine = &plcEngine;
}

void AppManager::loadApplications(const JsonObject& config) {
    if (!_plcEngine) return;

    app_instances.clear();

    if (config.containsKey("applications")) {
        JsonArray apps = config["applications"].as<JsonArray>();
        for (JsonObject app_config : apps) {
            const char* type = app_config["type"];
            
            // This is where we will create instances of our app modules
            // For example:
            // if (strcmp(type, "thermostat") == 0) {
            //     auto app = std::make_unique<ThermostatApp>();
            //     app->configure(app_config, *_plcEngine);
            //     app_instances.push_back(std::move(app));
            // }

            Log->printf("Loading application of type: %s\n", type);
        }
    }
}

void AppManager::updateAll() {
    for (auto& app : app_instances) {
        app->update();
    }
}