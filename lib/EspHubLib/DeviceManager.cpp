#include "DeviceManager.h"
#include "Preferences.h"

DeviceManager::DeviceManager() {
}

void DeviceManager::begin() {
    // Load devices from NVS
    loadDevices();
}

bool DeviceManager::addDevice(const uint8_t* mac_addr, uint8_t id) {
    if (devices.size() >= MAX_DEVICES) {
        return false; // Too many devices
    }

    // Check if device already exists
    for (const auto& device : devices) {
        if (memcmp(device.mac_addr, mac_addr, 6) == 0) {
            return true; // Already exists
        }
    }

    managed_device_t new_device;
    memcpy(new_device.mac_addr, mac_addr, 6);
    new_device.id = id;
    devices.push_back(new_device);

    // Add peer to ESP-NOW
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, mac_addr, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Log->println("Failed to add peer");
        return false;
    }

    saveDevices();
    return true;
}

void DeviceManager::loadDevices() {
    Preferences preferences;
    preferences.begin("esp-hub", false);
    
    devices.clear();
    int device_count = preferences.getInt("dev_count", 0);

    for (int i = 0; i < device_count; i++) {
        String key = "dev_" + String(i);
        managed_device_t device;
        preferences.getBytes(key.c_str(), &device, sizeof(managed_device_t));
        devices.push_back(device);

        // Re-add peer to ESP-NOW
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, device.mac_addr, 6);
        peerInfo.channel = 0;
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) != ESP_OK){
            Log->printf("Failed to re-add peer %d\n", device.id);
        }
    }
    
    preferences.end();
    Log->printf("Loaded %d devices from NVS\n", devices.size());
}

void DeviceManager::saveDevices() {
    Preferences preferences;
    preferences.begin("esp-hub", false);
    
    preferences.putInt("dev_count", devices.size());
    for (int i = 0; i < devices.size(); i++) {
        String key = "dev_" + String(i);
        preferences.putBytes(key.c_str(), &devices[i], sizeof(managed_device_t));
    }
    
    preferences.end();
    Log->printf("Saved %d devices to NVS\n", devices.size());
}