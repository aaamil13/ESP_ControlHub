#include "RF433Manager.h"
#include "StreamLogger.h"
#include "DeviceRegistry.h"

extern StreamLogger* EspHubLog;

RF433Manager::RF433Manager(int rxPin, int txPin, const String& defaultLocation)
    : DeviceManager("rf433", ProtocolType::RF433),
      rxPin(rxPin),
      txPin(txPin),
      defaultLocation(defaultLocation),
      learningEnabled(false),
      learningEndTime(0),
      learnCallback(nullptr) {
}

RF433Manager::~RF433Manager() {
}

void RF433Manager::begin() {
    EspHubLog->printf("RF433Manager: Initializing (RX: GPIO%d, TX: GPIO%d)...\\n", rxPin, txPin);

    // Initialize receiver
    if (rxPin >= 0) {
        rcSwitch.enableReceive(digitalPinToInterrupt(rxPin));
        EspHubLog->printf("RF433 receiver enabled on GPIO%d\\n", rxPin);
    }

    // Initialize transmitter
    if (txPin >= 0) {
        rcSwitch.enableTransmit(txPin);
        EspHubLog->printf("RF433 transmitter enabled on GPIO%d\\n", txPin);
    }

    EspHubLog->println("RF433Manager initialized");
}

void RF433Manager::loop() {
    // Check for received RF codes
    if (rcSwitch.available()) {
        handleReceivedCode();
        rcSwitch.resetAvailable();
    }

    // Check if learning mode should be disabled
    if (learningEnabled && millis() > learningEndTime) {
        disableLearningMode();
    }

    // Check offline devices
    checkOfflineDevices(300000); // 5 minute timeout for RF devices
}

void RF433Manager::setDefaultLocation(const String& location) {
    defaultLocation = location;
}

bool RF433Manager::registerDevice(unsigned long onCode, unsigned long offCode,
                                  const String& deviceId, const String& location,
                                  int protocol, int pulseLength, int bitLength) {
    // Build full device ID
    String fullDeviceId = buildDeviceId(location, deviceId);

    // Check if device already exists
    if (devices.find(fullDeviceId) != devices.end()) {
        EspHubLog->printf("RF433 device %s already registered\\n", fullDeviceId.c_str());
        return false;
    }

    // Create device structure
    RF433Device device;
    device.code = onCode;
    device.offCode = offCode;
    device.protocol = protocol;
    device.pulseLength = pulseLength;
    device.bitLength = bitLength;
    device.deviceId = fullDeviceId;
    device.location = location;
    device.isToggle = false;
    device.currentState = false;

    // Register in DeviceRegistry
    JsonDocument config;
    config["code_on"] = onCode;
    config["code_off"] = offCode;
    config["protocol"] = protocol;
    config["pulse_length"] = pulseLength;
    config["bit_length"] = bitLength;

    bool registered = registerDevice(fullDeviceId, config.as<JsonObject>());
    if (!registered) {
        EspHubLog->printf("ERROR: Failed to register RF433 device %s\\n", fullDeviceId.c_str());
        return false;
    }

    // Store device
    devices[fullDeviceId] = device;
    codeToDevice[onCode] = fullDeviceId;
    if (offCode != 0) {
        codeToDevice[offCode] = fullDeviceId;
    }

    // Register endpoint
    registerEndpointForDevice(device);

    EspHubLog->printf("Registered RF433 device: %s (ON: %lu, OFF: %lu)\\n",
                     fullDeviceId.c_str(), onCode, offCode);

    return true;
}

bool RF433Manager::registerToggleDevice(unsigned long code, const String& deviceId,
                                       const String& location,
                                       int protocol, int pulseLength, int bitLength) {
    // Build full device ID
    String fullDeviceId = buildDeviceId(location, deviceId);

    // Check if device already exists
    if (devices.find(fullDeviceId) != devices.end()) {
        EspHubLog->printf("RF433 device %s already registered\\n", fullDeviceId.c_str());
        return false;
    }

    // Create device structure
    RF433Device device;
    device.code = code;
    device.offCode = 0;
    device.protocol = protocol;
    device.pulseLength = pulseLength;
    device.bitLength = bitLength;
    device.deviceId = fullDeviceId;
    device.location = location;
    device.isToggle = true;
    device.currentState = false;

    // Register in DeviceRegistry
    JsonDocument config;
    config["code"] = code;
    config["protocol"] = protocol;
    config["pulse_length"] = pulseLength;
    config["bit_length"] = bitLength;
    config["toggle"] = true;

    bool registered = registerDevice(fullDeviceId, config.as<JsonObject>());
    if (!registered) {
        EspHubLog->printf("ERROR: Failed to register RF433 toggle device %s\\n", fullDeviceId.c_str());
        return false;
    }

    // Store device
    devices[fullDeviceId] = device;
    codeToDevice[code] = fullDeviceId;

    // Register endpoint
    registerEndpointForDevice(device);

    EspHubLog->printf("Registered RF433 toggle device: %s (Code: %lu)\\n",
                     fullDeviceId.c_str(), code);

    return true;
}

bool RF433Manager::sendCommand(const String& deviceId, bool state) {
    // Find device
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        EspHubLog->printf("ERROR: RF433 device %s not found\\n", deviceId.c_str());
        return false;
    }

    RF433Device& device = it->second;

    // Determine code to send
    unsigned long codeToSend;
    if (device.isToggle) {
        codeToSend = device.code;
        // Toggle state
        device.currentState = !device.currentState;
    } else {
        codeToSend = state ? device.code : device.offCode;
        device.currentState = state;
    }

    // Send RF code
    bool success = sendRawCode(codeToSend, device.protocol, device.pulseLength, device.bitLength);

    if (success) {
        // Update endpoint value in registry
        String endpointName = device.deviceId + ".state.bool";
        Endpoint* endpoint = registry->getEndpoint(endpointName);
        if (endpoint) {
            PlcValue newValue(PlcValueType::BOOL);
            newValue.value.bVal = device.currentState;
            registry->updateEndpointValue(endpointName, newValue);
        }

        EspHubLog->printf("RF433 command sent to %s: %s (code: %lu)\\n",
                         deviceId.c_str(), device.currentState ? "ON" : "OFF", codeToSend);
    }

    return success;
}

bool RF433Manager::sendRawCode(unsigned long code, int protocol, int pulseLength, int bitLength) {
    if (txPin < 0) {
        EspHubLog->println("ERROR: RF433 transmitter not configured");
        return false;
    }

    // Set protocol
    rcSwitch.setProtocol(protocol);

    // Set pulse length if specified
    if (pulseLength > 0) {
        rcSwitch.setPulseLength(pulseLength);
    }

    // Send code
    rcSwitch.send(code, bitLength);

    EspHubLog->printf("RF433 sent: code=%lu, protocol=%d, bits=%d\\n",
                     code, protocol, bitLength);

    return true;
}

void RF433Manager::enableLearningMode(uint32_t duration_ms) {
    learningEnabled = true;
    learningEndTime = millis() + duration_ms;
    EspHubLog->printf("RF433 learning mode enabled for %u seconds\\n", duration_ms / 1000);
}

void RF433Manager::disableLearningMode() {
    if (!learningEnabled) return;

    learningEnabled = false;
    EspHubLog->println("RF433 learning mode disabled");
}

RF433Device* RF433Manager::getDeviceByCode(unsigned long code) {
    auto it = codeToDevice.find(code);
    if (it != codeToDevice.end()) {
        return &devices[it->second];
    }
    return nullptr;
}

RF433Device* RF433Manager::getDeviceById(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it != devices.end()) {
        return &it->second;
    }
    return nullptr;
}

void RF433Manager::handleReceivedCode() {
    unsigned long receivedCode = rcSwitch.getReceivedValue();
    int protocol = rcSwitch.getReceivedProtocol();
    int bitLength = rcSwitch.getReceivedBitlength();
    int pulseLength = rcSwitch.getReceivedDelay();

    if (receivedCode == 0) {
        EspHubLog->println("RF433 received unknown encoding");
        return;
    }

    EspHubLog->printf("RF433 received: code=%lu, protocol=%d, bits=%d, pulse=%d\\n",
                     receivedCode, protocol, bitLength, pulseLength);

    // Check if code belongs to registered device
    RF433Device* device = getDeviceByCode(receivedCode);

    if (device) {
        // Update device state
        if (device->isToggle) {
            device->currentState = !device->currentState;
        } else {
            device->currentState = (receivedCode == device->code);
        }

        // Update endpoint in registry
        String endpointName = device->deviceId + ".state.bool";
        Endpoint* endpoint = registry->getEndpoint(endpointName);
        if (endpoint) {
            PlcValue newValue(PlcValueType::BOOL);
            newValue.value.bVal = device->currentState;
            registry->updateEndpointValue(endpointName, newValue);
            updateDeviceStatus(device->deviceId, true);
        }

        EspHubLog->printf("RF433 device %s state: %s\\n",
                         device->deviceId.c_str(), device->currentState ? "ON" : "OFF");
    } else if (learningEnabled) {
        // Learning mode - notify callback
        EspHubLog->printf("RF433 learning: New code detected: %lu\\n", receivedCode);

        if (learnCallback) {
            learnCallback(receivedCode, protocol, bitLength, pulseLength);
        }
    } else {
        EspHubLog->printf("RF433: Unknown code %lu (not registered)\\n", receivedCode);
    }
}

void RF433Manager::registerEndpointForDevice(const RF433Device& device) {
    // Create endpoint for device state
    Endpoint endpoint;
    endpoint.fullName = device.deviceId + ".state.bool";
    endpoint.location = device.location;
    endpoint.protocol = ProtocolType::RF433;
    endpoint.deviceId = device.deviceId;
    endpoint.endpoint = "state";
    endpoint.datatype = PlcValueType::BOOL;
    endpoint.isOnline = true;
    endpoint.lastSeen = millis();
    endpoint.isWritable = true;
    endpoint.currentValue.type = PlcValueType::BOOL;
    endpoint.currentValue.value.bVal = device.currentState;

    bool registered = registerEndpointHelper(endpoint);
    if (registered) {
        EspHubLog->printf("  Registered endpoint: %s (RW ONLINE)\\n", endpoint.fullName.c_str());
    }
}
