#ifndef MESH_DEVICE_MANAGER_H
#define MESH_DEVICE_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <map>

struct MeshDevice {
    uint32_t nodeId;
    String name;
    unsigned long lastSeen;
    bool isOnline; // New: Track online/offline status
    // Add more device-specific info here later (e.g., associated PLC variables)
};

class MeshDeviceManager {
public:
    MeshDeviceManager();
    void begin();
    void addDevice(uint32_t nodeId, const String& name);
    void updateDeviceLastSeen(uint32_t nodeId);
    MeshDevice* getDevice(uint32_t nodeId);
    std::vector<MeshDevice> getAllDevices();
    void checkOfflineDevices(unsigned long offlineTimeoutMs); // New: Check for offline devices

private:
    std::map<uint32_t, MeshDevice> devices;
    // For persistence, we would use NVS here
};

#endif // MESH_DEVICE_MANAGER_H