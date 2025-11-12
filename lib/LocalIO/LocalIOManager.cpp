#include "LocalIOManager.h"
#include "../PlcEngine/Engine/PlcMemory.h"
#include <LittleFS.h>

// ============================================================================
// Initialization
// ============================================================================

LocalIOManager::LocalIOManager()
    : plcMemory(nullptr), autoSyncEnabled(false), lastUpdateTime(0), enabled(true), errorCount(0) {
}

LocalIOManager::~LocalIOManager() {
    // Delete all pins
    for (auto& pair : ioPins) {
        delete pair.second;
    }
    ioPins.clear();
}

void LocalIOManager::begin() {
    EspHubLog->println("LocalIOManager: Initializing...");

    // Initialize LEDC fade service
    ledc_fade_func_install(0);

    lastUpdateTime = millis();

    EspHubLog->println("LocalIOManager: Initialized");
}

void LocalIOManager::loop() {
    if (!enabled) return;

    // Update all pins
    for (auto& pair : ioPins) {
        if (pair.second->isEnabled()) {
            pair.second->update();
        }
    }

    // Sync with PLC if enabled
    if (autoSyncEnabled && plcMemory != nullptr) {
        syncWithPLC();
    }

    lastUpdateTime = millis();
}

// ============================================================================
// Configuration Loading
// ============================================================================

bool LocalIOManager::loadConfig(const String& jsonConfig) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonConfig);

    if (error) {
        EspHubLog->printf("ERROR: Failed to parse IO config: %s\n", error.c_str());
        return false;
    }

    JsonObject root = doc.as<JsonObject>();

    // Clear existing pins
    for (auto& pair : ioPins) {
        delete pair.second;
    }
    ioPins.clear();

    int pinCount = 0;

    // Load digital inputs
    if (root.containsKey("digital_inputs")) {
        JsonArray arr = root["digital_inputs"].as<JsonArray>();
        for (JsonObject obj : arr) {
            if (loadDigitalInput(obj)) {
                pinCount++;
            }
        }
    }

    // Load digital outputs
    if (root.containsKey("digital_outputs")) {
        JsonArray arr = root["digital_outputs"].as<JsonArray>();
        for (JsonObject obj : arr) {
            if (loadDigitalOutput(obj)) {
                pinCount++;
            }
        }
    }

    // Load analog inputs
    if (root.containsKey("analog_inputs")) {
        JsonArray arr = root["analog_inputs"].as<JsonArray>();
        for (JsonObject obj : arr) {
            if (loadAnalogInput(obj)) {
                pinCount++;
            }
        }
    }

    // Load PWM outputs
    if (root.containsKey("pwm_outputs")) {
        JsonArray arr = root["pwm_outputs"].as<JsonArray>();
        for (JsonObject obj : arr) {
            if (loadPWMOutput(obj)) {
                pinCount++;
            }
        }
    }

    // Load pulse counters
    if (root.containsKey("pulse_counters")) {
        JsonArray arr = root["pulse_counters"].as<JsonArray>();
        for (JsonObject obj : arr) {
            if (loadPulseCounter(obj)) {
                pinCount++;
            }
        }
    }

    // Load PLC mapping
    if (root.containsKey("plc_mapping")) {
        JsonObject plcMapping = root["plc_mapping"].as<JsonObject>();
        loadPLCMapping(plcMapping);
    }

    EspHubLog->printf("LocalIOManager: Loaded %d IO pins\n", pinCount);
    return pinCount > 0;
}

bool LocalIOManager::loadConfigFromFile(const String& filename) {
    if (!LittleFS.exists(filename)) {
        EspHubLog->printf("ERROR: Config file not found: %s\n", filename.c_str());
        return false;
    }

    File file = LittleFS.open(filename, "r");
    if (!file) {
        EspHubLog->printf("ERROR: Failed to open config file: %s\n", filename.c_str());
        return false;
    }

    String config = file.readString();
    file.close();

    return loadConfig(config);
}

// ============================================================================
// Configuration Helpers
// ============================================================================

bool LocalIOManager::loadDigitalInput(const JsonObject& config) {
    String name = config["name"] | "";
    if (name.isEmpty()) {
        EspHubLog->println("ERROR: Digital input missing 'name'");
        return false;
    }

    DigitalInputConfig diConfig;
    diConfig.pin = config["pin"] | 0;
    diConfig.invert = config["invert"] | false;
    diConfig.debounceMs = config["debounce_ms"] | 50;
    diConfig.filterSamples = config["filter_samples"] | 0;

    String mode = config["mode"] | "INPUT";
    if (mode == "INPUT_PULLUP") {
        diConfig.mode = DigitalInputMode::INPUT_PULLUP;
    } else if (mode == "INPUT_PULLDOWN") {
        diConfig.mode = DigitalInputMode::INPUT_PULLDOWN;
    } else {
        diConfig.mode = DigitalInputMode::INPUT;
    }

    auto* pin = new DigitalInputPin(name, diConfig);
    return addPin(name, pin);
}

bool LocalIOManager::loadDigitalOutput(const JsonObject& config) {
    String name = config["name"] | "";
    if (name.isEmpty()) {
        EspHubLog->println("ERROR: Digital output missing 'name'");
        return false;
    }

    DigitalOutputConfig doConfig;
    doConfig.pin = config["pin"] | 0;
    doConfig.invert = config["invert"] | false;
    doConfig.initialState = config["initial_state"] | false;
    doConfig.pulseWidthMs = config["pulse_width_ms"] | 100;
    doConfig.safeState = config["safe_state"] | false;

    auto* pin = new DigitalOutputPin(name, doConfig);
    return addPin(name, pin);
}

bool LocalIOManager::loadAnalogInput(const JsonObject& config) {
    String name = config["name"] | "";
    if (name.isEmpty()) {
        EspHubLog->println("ERROR: Analog input missing 'name'");
        return false;
    }

    AnalogInputConfig aiConfig;
    aiConfig.pin = config["pin"] | 0;
    aiConfig.resolution = config["resolution"] | 12;
    aiConfig.sampleRate = config["sample_rate"] | 100;
    aiConfig.filterSamples = config["filter_samples"] | 10;
    aiConfig.calibrationOffset = config["calib_offset"] | 0.0f;
    aiConfig.calibrationScale = config["calib_scale"] | 1.0f;
    aiConfig.minValue = config["min_value"] | 0.0f;
    aiConfig.maxValue = config["max_value"] | 100.0f;

    String range = config["range"] | "0-3.3V";
    if (range == "0-1.1V") {
        aiConfig.range = AnalogInputRange::RANGE_0_1V;
    } else if (range == "0-2.2V") {
        aiConfig.range = AnalogInputRange::RANGE_0_2V;
    } else if (range == "0-6V") {
        aiConfig.range = AnalogInputRange::RANGE_0_6V;
    } else {
        aiConfig.range = AnalogInputRange::RANGE_0_3V3;
    }

    auto* pin = new AnalogInputPin(name, aiConfig);
    return addPin(name, pin);
}

bool LocalIOManager::loadPWMOutput(const JsonObject& config) {
    String name = config["name"] | "";
    if (name.isEmpty()) {
        EspHubLog->println("ERROR: PWM output missing 'name'");
        return false;
    }

    PWMOutputConfig pwmConfig;
    pwmConfig.pin = config["pin"] | 0;
    pwmConfig.frequency = config["frequency"] | 1000;
    pwmConfig.resolution = config["resolution"] | 10;
    pwmConfig.channel = config["channel"] | 0;
    pwmConfig.initialDutyCycle = config["initial_duty"] | 0.0f;

    auto* pin = new PWMOutputPin(name, pwmConfig);
    return addPin(name, pin);
}

bool LocalIOManager::loadPulseCounter(const JsonObject& config) {
    String name = config["name"] | "";
    if (name.isEmpty()) {
        EspHubLog->println("ERROR: Pulse counter missing 'name'");
        return false;
    }

    PulseCounterConfig pcConfig;
    pcConfig.pin = config["pin"] | 0;
    pcConfig.sampleWindowMs = config["sample_window_ms"] | 1000;
    pcConfig.enableFilter = config["enable_filter"] | true;
    pcConfig.filterThresholdNs = config["filter_threshold_ns"] | 1000;

    String edge = config["edge"] | "RISING";
    if (edge == "FALLING") {
        pcConfig.edge = PulseCounterEdge::FALLING;
    } else if (edge == "BOTH") {
        pcConfig.edge = PulseCounterEdge::BOTH;
    } else {
        pcConfig.edge = PulseCounterEdge::RISING;
    }

    String mode = config["mode"] | "COUNTER";
    if (mode == "FREQUENCY") {
        pcConfig.mode = PulseCounterMode::FREQUENCY;
    } else if (mode == "PERIOD") {
        pcConfig.mode = PulseCounterMode::PERIOD;
    } else {
        pcConfig.mode = PulseCounterMode::COUNTER;
    }

    auto* pin = new PulseCounterPin(name, pcConfig);
    return addPin(name, pin);
}

bool LocalIOManager::loadPLCMapping(const JsonObject& config) {
    // Clear existing mappings
    inputMappings.clear();
    outputMappings.clear();

    int inputCount = 0;
    int outputCount = 0;

    // Load input mappings
    if (config.containsKey("inputs")) {
        JsonArray inputs = config["inputs"].as<JsonArray>();
        for (JsonObject mapping : inputs) {
            PLCMapping m;
            m.ioPinName = mapping["io_pin"] | "";
            m.plcVarName = mapping["plc_var"] | "";
            m.type = mapping["type"] | "bool";

            if (!m.ioPinName.isEmpty() && !m.plcVarName.isEmpty()) {
                inputMappings.push_back(m);
                inputCount++;
            }
        }
    }

    // Load output mappings
    if (config.containsKey("outputs")) {
        JsonArray outputs = config["outputs"].as<JsonArray>();
        for (JsonObject mapping : outputs) {
            PLCMapping m;
            m.ioPinName = mapping["io_pin"] | "";
            m.plcVarName = mapping["plc_var"] | "";
            m.type = mapping["type"] | "bool";

            if (!m.ioPinName.isEmpty() && !m.plcVarName.isEmpty()) {
                outputMappings.push_back(m);
                outputCount++;
            }
        }
    }

    EspHubLog->printf("LocalIOManager: Loaded %d input mappings, %d output mappings\n",
                     inputCount, outputCount);
    return (inputCount + outputCount) > 0;
}

// ============================================================================
// Digital I/O
// ============================================================================

bool LocalIOManager::readDigital(const String& name) {
    IOPinBase* pin = getPin(name);
    if (!pin) return false;

    return pin->getState().boolValue;
}

bool LocalIOManager::writeDigital(const String& name, bool value) {
    IOPinBase* pin = getPin(name);
    if (!pin) return false;

    IOPinState state;
    state.boolValue = value;
    state.floatValue = value ? 1.0f : 0.0f;
    state.intValue = value ? 1 : 0;

    return pin->setValue(state);
}

// ============================================================================
// Analog I/O
// ============================================================================

float LocalIOManager::readAnalog(const String& name) {
    IOPinBase* pin = getPin(name);
    if (!pin) return 0.0f;

    return pin->getState().floatValue;
}

bool LocalIOManager::writeAnalog(const String& name, float value) {
    IOPinBase* pin = getPin(name);
    if (!pin) return false;

    IOPinState state;
    state.floatValue = value;
    state.boolValue = (value > 0.0f);
    state.intValue = (int32_t)value;

    return pin->setValue(state);
}

// ============================================================================
// PWM
// ============================================================================

bool LocalIOManager::setPWMDutyCycle(const String& name, float dutyCycle) {
    PWMOutputPin* pin = dynamic_cast<PWMOutputPin*>(getPin(name));
    if (!pin) return false;

    pin->setDutyCycle(dutyCycle);
    return true;
}

bool LocalIOManager::setPWMFrequency(const String& name, uint16_t frequency) {
    PWMOutputPin* pin = dynamic_cast<PWMOutputPin*>(getPin(name));
    if (!pin) return false;

    pin->setFrequency(frequency);
    return true;
}

// ============================================================================
// Pulse Counter
// ============================================================================

int32_t LocalIOManager::getPulseCount(const String& name) {
    PulseCounterPin* pin = dynamic_cast<PulseCounterPin*>(getPin(name));
    if (!pin) return 0;

    return pin->getTotalCount();
}

float LocalIOManager::getPulseFrequency(const String& name) {
    PulseCounterPin* pin = dynamic_cast<PulseCounterPin*>(getPin(name));
    if (!pin) return 0.0f;

    return pin->getFrequency();
}

bool LocalIOManager::resetPulseCounter(const String& name) {
    PulseCounterPin* pin = dynamic_cast<PulseCounterPin*>(getPin(name));
    if (!pin) return false;

    pin->resetCounter();
    return true;
}

// ============================================================================
// Pin Management
// ============================================================================

IOPinBase* LocalIOManager::getPin(const String& name) {
    auto it = ioPins.find(name);
    if (it != ioPins.end()) {
        return it->second;
    }
    return nullptr;
}

// ============================================================================
// Status and Diagnostics
// ============================================================================

String LocalIOManager::getStatusJson() const {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();

    root["enabled"] = enabled;
    root["pin_count"] = ioPins.size();
    root["error_count"] = errorCount;
    root["last_update"] = lastUpdateTime;
    root["memory_usage"] = getMemoryUsage();

    JsonArray pinsArray = root["pins"].to<JsonArray>();
    for (const auto& pair : ioPins) {
        JsonObject pinObj = pinsArray.add<JsonObject>();
        pinObj["name"] = pair.first;
        pinObj["type"] = (int)pair.second->getType();
        pinObj["enabled"] = pair.second->isEnabled();

        IOPinState state = pair.second->getState();
        pinObj["value"] = state.floatValue;
        pinObj["valid"] = state.isValid;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

String LocalIOManager::getPinStatusJson(const String& name) const {
    auto it = ioPins.find(name);
    if (it == ioPins.end()) {
        return "{\"error\":\"Pin not found\"}";
    }

    return it->second->getConfigJson();
}

void LocalIOManager::setEnabled(bool enable) {
    enabled = enable;

    for (auto& pair : ioPins) {
        pair.second->setEnabled(enable);
    }
}

size_t LocalIOManager::getMemoryUsage() const {
    return ioPins.size() * (sizeof(IOPinBase*) + 100); // Approximate
}

// ============================================================================
// Safety
// ============================================================================

void LocalIOManager::setSafeState() {
    EspHubLog->println("LocalIOManager: Setting safe state for all outputs");

    for (auto& pair : ioPins) {
        DigitalOutputPin* doPin = dynamic_cast<DigitalOutputPin*>(pair.second);
        if (doPin) {
            doPin->setSafeState();
        }

        PWMOutputPin* pwmPin = dynamic_cast<PWMOutputPin*>(pair.second);
        if (pwmPin) {
            pwmPin->setDutyCycle(0.0f); // Safe state = 0% duty
        }
    }
}

// ============================================================================
// PLC Integration
// ============================================================================

void LocalIOManager::setPlcMemory(PlcMemory* memory) {
    plcMemory = memory;

    if (plcMemory == nullptr) {
        EspHubLog->println("LocalIOManager: PLC memory cleared");
        return;
    }

    // Declare all PLC variables based on mappings
    int declaredCount = 0;

    // Declare input variables
    for (const auto& mapping : inputMappings) {
        PlcValueType varType = PlcValueType::BOOL;
        if (mapping.type == "real") {
            varType = PlcValueType::REAL;
        } else if (mapping.type == "int") {
            varType = PlcValueType::INT;
        }

        std::string varName = mapping.plcVarName.c_str();
        if (plcMemory->declareVariable(varName, varType, false)) {
            declaredCount++;
        }
    }

    // Declare output variables
    for (const auto& mapping : outputMappings) {
        PlcValueType varType = PlcValueType::BOOL;
        if (mapping.type == "real") {
            varType = PlcValueType::REAL;
        } else if (mapping.type == "int") {
            varType = PlcValueType::INT;
        }

        std::string varName = mapping.plcVarName.c_str();
        if (plcMemory->declareVariable(varName, varType, false)) {
            declaredCount++;
        }
    }

    EspHubLog->printf("LocalIOManager: Declared %d PLC variables\n", declaredCount);

    // Enable auto-sync
    autoSyncEnabled = true;
}

void LocalIOManager::syncWithPLC() {
    if (plcMemory == nullptr) return;

    // Sync inputs: IO -> PLC
    for (const auto& mapping : inputMappings) {
        IOPinBase* pin = getPin(mapping.ioPinName);
        if (!pin) continue;

        IOPinState state = pin->getState();
        if (!state.isValid) continue;

        std::string varName = mapping.plcVarName.c_str();

        if (mapping.type == "bool") {
            plcMemory->setValue(varName, state.boolValue);
        } else if (mapping.type == "real") {
            plcMemory->setValue(varName, state.floatValue);
        } else if (mapping.type == "int") {
            plcMemory->setValue(varName, state.intValue);
        }
    }

    // Sync outputs: PLC -> IO
    for (const auto& mapping : outputMappings) {
        std::string varName = mapping.plcVarName.c_str();

        if (mapping.type == "bool") {
            bool value = plcMemory->getValue<bool>(varName, false);
            writeDigital(mapping.ioPinName, value);
        } else if (mapping.type == "real") {
            float value = plcMemory->getValue<float>(varName, 0.0f);

            // Try PWM output first
            PWMOutputPin* pwmPin = dynamic_cast<PWMOutputPin*>(getPin(mapping.ioPinName));
            if (pwmPin) {
                pwmPin->setDutyCycle(value);
            } else {
                // Otherwise try analog output
                writeAnalog(mapping.ioPinName, value);
            }
        }
    }
}
