#include "RF433Manager.h"
#include "Logger.h"

RF433Manager::RF433Manager(int rxPin, int txPin)
    : rxPin(rxPin),
      txPin(txPin),
      learningEnabled(false),
      learningEndTime(0),
      learnCallback(nullptr) {
}

RF433Manager::~RF433Manager() {
}

void RF433Manager::begin() {
    LOG_INFO("RF433Manager", "Initializing (RX: GPIO" + String(rxPin) + ", TX: GPIO" + String(txPin) + ")...");

    // Initialize receiver
    if (rxPin >= 0) {
        rcSwitch.enableReceive(digitalPinToInterrupt(rxPin));
        LOG_INFO("RF433Manager", "Receiver enabled on GPIO" + String(rxPin));
    }

    // Initialize transmitter
    if (txPin >= 0) {
        rcSwitch.enableTransmit(txPin);
        LOG_INFO("RF433Manager", "Transmitter enabled on GPIO" + String(txPin));
    }

    LOG_INFO("RF433Manager", "Initialized");
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
}

// ============================================================================
// ProtocolManagerInterface Implementation
// ============================================================================

bool RF433Manager::initializeDevice(const String& deviceId, const JsonObject& connectionConfig) {
    LOG_INFO("RF433Manager", "Initializing device: " + deviceId);

    // For RF433, connection config contains RX/TX pins (global, not per-device)
    // Device-specific RF codes are in endpoint configs
    // Just create empty device entry
    RF433Device device;
    device.deviceId = deviceId;
    device.code = 0;
    device.offCode = 0;
    device.protocol = 1;
    device.pulseLength = 0;
    device.bitLength = 24;
    device.isToggle = false;
    device.currentState = false;
    device.location = "";

    devices[deviceId] = device;

    LOG_INFO("RF433Manager", "Device initialized: " + deviceId);
    return true;
}

bool RF433Manager::removeDevice(const String& deviceId) {
    auto it = devices.find(deviceId);
    if (it == devices.end()) {
        return false;
    }

    // Remove from code mapping
    RF433Device& device = it->second;
    if (device.code != 0) {
        codeToDevice.erase(device.code);
    }
    if (device.offCode != 0) {
        codeToDevice.erase(device.offCode);
    }

    devices.erase(it);
    LOG_INFO("RF433Manager", "Removed device: " + deviceId);
    return true;
}

bool RF433Manager::readEndpoint(const String& deviceId, const JsonObject& endpointConfig, PlcValue& value) {
    // RF433 is primarily transmit-only for most devices
    // Reading is based on received codes, which update state asynchronously

    RF433Device* device = getDeviceById(deviceId);
    if (!device) {
        LOG_ERROR("RF433Manager", "Device not found: " + deviceId);
        return false;
    }

    // Return current known state
    value = PlcValue(PlcValueType::BOOL);
    value.value.bVal = device->currentState;

    return true;
}

bool RF433Manager::writeEndpoint(const String& deviceId, const JsonObject& endpointConfig, const PlcValue& value) {
    RF433Device* device = getDeviceById(deviceId);
    if (!device) {
        LOG_ERROR("RF433Manager", "Device not found: " + deviceId);
        return false;
    }

    // Parse RF config from endpoint
    unsigned long onCode, offCode;
    int protocol, pulseLength, bitLength;

    if (!parseRF433Config(endpointConfig, onCode, offCode, protocol, pulseLength, bitLength)) {
        LOG_ERROR("RF433Manager", "Invalid RF433 configuration");
        return false;
    }

    // Update device config if codes changed
    if (device->code != onCode || device->offCode != offCode) {
        // Remove old code mappings
        if (device->code != 0) codeToDevice.erase(device->code);
        if (device->offCode != 0) codeToDevice.erase(device->offCode);

        // Update device
        device->code = onCode;
        device->offCode = offCode;
        device->protocol = protocol;
        device->pulseLength = pulseLength;
        device->bitLength = bitLength;
        device->isToggle = (offCode == 0 || offCode == onCode);

        // Add new code mappings
        if (onCode != 0) codeToDevice[onCode] = deviceId;
        if (offCode != 0 && offCode != onCode) codeToDevice[offCode] = deviceId;
    }

    // Determine which code to send
    unsigned long codeToSend;
    bool targetState;

    if (value.type == PlcValueType::BOOL) {
        targetState = value.value.bVal;
    } else {
        LOG_ERROR("RF433Manager", "RF433 write requires bool value");
        return false;
    }

    if (device->isToggle) {
        // For toggle devices, always send the same code
        codeToSend = onCode;
    } else {
        // For separate ON/OFF codes
        codeToSend = targetState ? onCode : offCode;
    }

    // Send RF code
    if (sendRawCode(codeToSend, protocol, pulseLength, bitLength)) {
        device->currentState = targetState;
        LOG_INFO("RF433Manager", "Sent RF code " + String(codeToSend) + " to " + deviceId);
        return true;
    }

    return false;
}

bool RF433Manager::testConnection(const JsonObject& connectionConfig) {
    // For RF433, test if transmitter is initialized
    // RX/TX pins should match the manager's pins
    int testRxPin = connectionConfig["rx_pin"] | -1;
    int testTxPin = connectionConfig["tx_pin"] | -1;

    if (testRxPin != rxPin || testTxPin != txPin) {
        LOG_ERROR("RF433Manager", "Connection config pins don't match manager pins");
        return false;
    }

    // Check if transmitter is enabled
    if (txPin >= 0) {
        LOG_INFO("RF433Manager", "RF433 transmitter test OK");
        return true;
    }

    return false;
}

bool RF433Manager::testEndpoint(const String& deviceId, const JsonObject& endpointConfig) {
    RF433Device* device = getDeviceById(deviceId);
    if (!device) {
        LOG_ERROR("RF433Manager", "Device not found: " + deviceId);
        return false;
    }

    // Parse RF config
    unsigned long onCode, offCode;
    int protocol, pulseLength, bitLength;

    if (!parseRF433Config(endpointConfig, onCode, offCode, protocol, pulseLength, bitLength)) {
        LOG_ERROR("RF433Manager", "Invalid RF433 configuration");
        return false;
    }

    // Try to send test code (ON code)
    if (sendRawCode(onCode, protocol, pulseLength, bitLength)) {
        LOG_INFO("RF433Manager", "Test code sent successfully: " + String(onCode));
        return true;
    }

    return false;
}

bool RF433Manager::isDeviceOnline(const String& deviceId) {
    // RF433 devices are always considered "online" if registered
    // since we can't verify if they actually receive the signal
    return (devices.find(deviceId) != devices.end());
}

// ============================================================================
// Learning Mode
// ============================================================================

void RF433Manager::enableLearningMode(uint32_t duration_ms) {
    learningEnabled = true;
    learningEndTime = millis() + duration_ms;
    LOG_INFO("RF433Manager", "Learning mode enabled for " + String(duration_ms / 1000) + " seconds");
}

void RF433Manager::disableLearningMode() {
    if (learningEnabled) {
        learningEnabled = false;
        learningEndTime = 0;
        LOG_INFO("RF433Manager", "Learning mode disabled");
    }
}

// ============================================================================
// Private Methods
// ============================================================================

RF433Device* RF433Manager::getDeviceByCode(unsigned long code) {
    auto it = codeToDevice.find(code);
    if (it != codeToDevice.end()) {
        return getDeviceById(it->second);
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

bool RF433Manager::sendRawCode(unsigned long code, int protocol, int pulseLength, int bitLength) {
    if (txPin < 0) {
        LOG_ERROR("RF433Manager", "Transmitter not initialized");
        return false;
    }

    if (code == 0) {
        LOG_ERROR("RF433Manager", "Invalid code: 0");
        return false;
    }

    // Set protocol
    rcSwitch.setProtocol(protocol);

    // Set pulse length if specified
    if (pulseLength > 0) {
        rcSwitch.setPulseLength(pulseLength);
    }

    // Send code
    if (bitLength > 0) {
        rcSwitch.send(code, bitLength);
    } else {
        rcSwitch.send(code, 24); // Default 24 bits
    }

    LOG_INFO("RF433Manager", "Sent RF code: " + String(code) +
             " (protocol: " + String(protocol) +
             ", pulse: " + String(pulseLength) +
             ", bits: " + String(bitLength) + ")");

    return true;
}

void RF433Manager::handleReceivedCode() {
    if (!rcSwitch.available()) {
        return;
    }

    unsigned long receivedCode = rcSwitch.getReceivedValue();
    int protocol = rcSwitch.getReceivedProtocol();
    int bitLength = rcSwitch.getReceivedBitlength();
    int pulseLength = rcSwitch.getReceivedDelay();

    if (receivedCode == 0) {
        LOG_WARN("RF433Manager", "Received unknown encoding");
        return;
    }

    LOG_INFO("RF433Manager", "Received RF code: " + String(receivedCode) +
             " (protocol: " + String(protocol) +
             ", bits: " + String(bitLength) +
             ", pulse: " + String(pulseLength) + ")");

    // Check if code belongs to registered device
    RF433Device* device = getDeviceByCode(receivedCode);

    if (device) {
        // Update device state
        if (device->isToggle) {
            device->currentState = !device->currentState;
        } else {
            if (receivedCode == device->code) {
                device->currentState = true;
            } else if (receivedCode == device->offCode) {
                device->currentState = false;
            }
        }

        LOG_INFO("RF433Manager", "Device " + device->deviceId + " state: " +
                String(device->currentState ? "ON" : "OFF"));
    } else if (learningEnabled && learnCallback) {
        // Learning mode - notify callback
        LOG_INFO("RF433Manager", "Learning mode: new code detected");
        learnCallback(receivedCode, protocol, bitLength, pulseLength);
    }
}

bool RF433Manager::parseRF433Config(const JsonObject& endpointConfig, unsigned long& onCode, unsigned long& offCode,
                                    int& protocol, int& pulseLength, int& bitLength) {
    // Check for RF codes object
    JsonObject rfCodes = endpointConfig["rf_codes"];
    if (rfCodes.isNull()) {
        LOG_ERROR("RF433Manager", "Missing rf_codes in endpoint config");
        return false;
    }

    // Parse ON code (required)
    onCode = rfCodes["on"] | 0UL;
    if (onCode == 0) {
        LOG_ERROR("RF433Manager", "Missing or invalid 'on' code");
        return false;
    }

    // Parse OFF code (optional for toggle devices)
    offCode = rfCodes["off"] | 0UL;

    // Parse protocol parameters
    protocol = endpointConfig["protocol"] | 1;
    pulseLength = endpointConfig["pulse_length"] | 0;
    bitLength = endpointConfig["bit_length"] | 24;

    return true;
}
