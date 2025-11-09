#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <esp_now.h>
#include <vector>

#define MAX_DEVICES 20

typedef struct {
    uint8_t mac_addr[6];
    uint8_t id;
    // We can add more device-specific info here later
} managed_device_t;

class DeviceManager {
public:
    DeviceManager();
    void begin();
    bool addDevice(const uint8_t* mac_addr, uint8_t id);
    // More methods will be added here

private:
    std::vector<managed_device_t> devices;
    void loadDevices();
    void saveDevices();
};

#endif // DEVICE_MANAGER_H