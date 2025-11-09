#ifndef APP_MODULE_H
#define APP_MODULE_H

#include <ArduinoJson.h>
#include "../../PlcCore/PlcEngine.h"

// Base class for all high-level application modules
class AppModule {
public:
    virtual ~AppModule() {}

    // Called once to pass the JSON configuration for this specific app instance.
    // The app module can use the plcEngine to create its underlying logic.
    virtual void configure(const JsonObject& config, PlcEngine& plcEngine) = 0;

    // Called repeatedly to execute the app's logic.
    virtual void update() = 0;
};

#endif // APP_MODULE_H