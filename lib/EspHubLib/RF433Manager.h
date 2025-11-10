#ifndef RF433_MANAGER_H
#define RF433_MANAGER_H

#include "DeviceManager.h"
#include <RCSwitch.h>
#include <ArduinoJson.h>
#include <map>

/**
 * RF433Manager - Manages 433MHz RF devices
 *
 * Supports sending and receiving 433MHz RF signals for:
 * - RF switches and relays
 * - RF remote controls
 * - RF sensors (door/window, motion, etc.)
 * - RF outlets/sockets
 *
 * Hardware Requirements:
 * - 433MHz receiver module (RX pin)
 * - 433MHz transmitter module (TX pin)
 *
 * Hierarchical naming:
 *   {location}.rf433.{device_id}.{endpoint}.{datatype}
 *   Example: living_room.rf433.switch_1.state.bool
 *
 * Supported Protocols:
 * - PT2262, EV1527, RT1527, FP1527, HS1527
 * - HT6P20B, HT12E
 * - SC5262, SC5272
 * - And more via RCSwitch library
 */

struct RF433Device {
    unsigned long code;        // RF code for ON state
    unsigned long offCode;     // RF code for OFF state (0 if toggle)
    int protocol;              // RF protocol number
    int pulseLength;           // Pulse length in microseconds
    int bitLength;             // Number of bits
    String deviceId;           // Device ID in registry
    String location;           // Device location
    bool isToggle;             // True if same code toggles state
    bool currentState;         // Current known state
};

class RF433Manager : public DeviceManager {
public:
    RF433Manager(int rxPin, int txPin, const String& defaultLocation = "");
    ~RF433Manager();

    // DeviceManager interface
    void begin() override;
    void loop() override;

    // Configuration
    void setDefaultLocation(const String& location);
    String getDefaultLocation() const { return defaultLocation; }

    // Device registration
    bool registerDevice(unsigned long onCode, unsigned long offCode,
                       const String& deviceId, const String& location,
                       int protocol = 1, int pulseLength = 0, int bitLength = 24);

    bool registerToggleDevice(unsigned long code, const String& deviceId,
                            const String& location,
                            int protocol = 1, int pulseLength = 0, int bitLength = 24);

    // RF transmission
    bool sendCommand(const String& deviceId, bool state);
    bool sendRawCode(unsigned long code, int protocol = 1, int pulseLength = 0, int bitLength = 24);

    // Learning mode
    void enableLearningMode(uint32_t duration_ms = 60000);
    void disableLearningMode();
    bool isLearningMode() const { return learningEnabled; }

    // Auto-registration callback
    typedef std::function<void(unsigned long code, int protocol, int bitLength, int pulseLength)> LearnCallback;
    void setLearnCallback(LearnCallback callback) { learnCallback = callback; }

    // Device lookup
    RF433Device* getDeviceByCode(unsigned long code);
    RF433Device* getDeviceById(const String& deviceId);

private:
    RCSwitch rcSwitch;
    int rxPin;
    int txPin;
    String defaultLocation;

    bool learningEnabled;
    unsigned long learningEndTime;
    LearnCallback learnCallback;

    // Device storage
    std::map<String, RF433Device> devices;       // deviceId -> device
    std::map<unsigned long, String> codeToDevice; // code -> deviceId

    // RX handler
    void handleReceivedCode();
    void registerEndpointForDevice(const RF433Device& device);
};

#endif // RF433_MANAGER_H
