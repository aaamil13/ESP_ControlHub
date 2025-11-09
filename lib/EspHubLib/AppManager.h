#ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <memory>
#include "apps/AppModule.h"
#include "../PlcCore/PlcEngine.h"

class AppManager {
public:
    AppManager();
    void begin(PlcEngine& plcEngine);
    void loadApplications(const JsonObject& config);
    void updateAll();

private:
    PlcEngine* _plcEngine;
    std::vector<std::unique_ptr<AppModule>> app_instances;
};

#endif // APP_MANAGER_H