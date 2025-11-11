#ifndef RF433_MANAGER_H
#define RF433_MANAGER_H

#include "../Protocols/ProtocolManagerInterface.h"
#include <RCSwitch.h>
#include <ArduinoJson.h>
#include <map>

/**
 * RF433Manager - Protocol manager for 433MHz RF devices
 *
 * Implements the ProtocolManagerInterface to provide transport layer
 * for 433MHz RF communication.
 *
 * Responsibilities:
 * - RF transmission via 433MHz transmitter
 * - RF reception and code learning
 * - Protocol-specific RF encoding/decoding
 *
 * Does NOT:
 * - Store device configurations (done by DeviceConfigManager)
 * - Register IO points in DeviceRegistry (done by DeviceConfigManager)
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

class RF433Manager : public ProtocolManagerInterface {
public:
    RF433Manager(int rxPin, int txPin);
    ~RF433Manager();

    // ProtocolManagerInterface implementation
    void begin() override;
    void loop() override;

    bool initializeDevice(const String& deviceId, const JsonObject& connectionConfig) override;
    bool removeDevice(const String& deviceId) override;

    bool readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) override;
    bool writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) override;

    bool testConnection(const JsonObject& connectionConfig) override;
    bool testEndpoint(const String& deviceId, const JsonObject& endpointConfig) override;

    String getProtocolName() const override { return "rf433"; }
    bool isDeviceOnline(const String& deviceId) override;

    // Learning mode
    void enableLearningMode(uint32_t duration_ms = 60000);
    void disableLearningMode();
    bool isLearningMode() const { return learningEnabled; }

    // Auto-registration callback
    typedef std::function<void(unsigned long code, int protocol, int bitLength, int pulseLength)> LearnCallback;
    void setLearnCallback(LearnCallback callback) { learnCallback = callback; }

private:
    RCSwitch rcSwitch;
    int rxPin;
    int txPin;

    bool learningEnabled;
    unsigned long learningEndTime;
    LearnCallback learnCallback;

    // Device storage
    std::map<String, RF433Device> devices;       // deviceId -> device
    std::map<unsigned long, String> codeToDevice; // code -> deviceId

    // Internal device management
    RF433Device* getDeviceByCode(unsigned long code);
    RF433Device* getDeviceById(const String& deviceId);

    // RF transmission
    bool sendRawCode(unsigned long code, int protocol, int pulseLength, int bitLength);

    // RX handler
    void handleReceivedCode();

    // Parsing helpers
    bool parseRF433Config(const JsonObject& endpointConfig, unsigned long& onCode, unsigned long& offCode,
                         int& protocol, int& pulseLength, int& bitLength);
};

#endif // RF433_MANAGER_H
