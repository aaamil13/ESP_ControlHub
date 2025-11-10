#include "PlcMemory.h"
#include "Preferences.h"
#include <StreamLogger.h> // Include for EspHubLog
#include <DeviceRegistry.h> // Include for IO point integration

extern StreamLogger* EspHubLog; // Declare EspHubLog

PlcMemory::PlcMemory() : deviceRegistry(nullptr) {}

void PlcMemory::begin() {
    loadRetentiveMemory();
}

bool PlcMemory::declareVariable(const std::string& name, PlcValueType type, bool isRetentive, const String& mesh_link) {
    if (memoryMap.count(name)) {
        return false; // Variable already exists
    }
    PlcVariable newVar;
    newVar.type = type;
    newVar.isRetentive = isRetentive;
    newVar.mesh_link = mesh_link;
    newVar.valueType = type; // Set the valueType based on PlcValueType

    // Initialize with default value
    switch (type) {
        case PlcValueType::BOOL: newVar.value.bVal = false; break;
        case PlcValueType::BYTE: newVar.value.ui8Val = (uint8_t)0; break;
        case PlcValueType::INT: newVar.value.i16Val = (int16_t)0; break;
        case PlcValueType::DINT: newVar.value.ui32Val = (uint32_t)0; break;
        case PlcValueType::REAL: newVar.value.fVal = 0.0f; break;
        case PlcValueType::STRING_TYPE: strcpy(newVar.value.sVal, ""); break;
    }
    memoryMap[name] = newVar;
    return true;
}

template<typename T>
bool PlcMemory::setValue(const std::string& name, T val) {
    if (!memoryMap.count(name)) {
        return false; // Variable not declared
    }
    PlcVariable& var = memoryMap[name];
    // Check for type mismatch (simplified, as union doesn't enforce this at compile time)
    // This would ideally involve more robust type checking at runtime or design time
    switch (var.type) {
        case PlcValueType::BOOL: var.value.bVal = (bool)val; break;
        case PlcValueType::BYTE: var.value.ui8Val = static_cast<uint8_t>(val); break;
        case PlcValueType::INT: var.value.i16Val = static_cast<int16_t>(val); break;
        case PlcValueType::DINT: var.value.ui32Val = static_cast<uint32_t>(val); break;
        case PlcValueType::REAL: var.value.fVal = static_cast<float>(val); break;
        case PlcValueType::STRING_TYPE: strncpy(var.value.sVal, String(val).c_str(), sizeof(var.value.sVal) - 1); var.value.sVal[sizeof(var.value.sVal) - 1] = '\0'; break;
    }
    return true;
}

template<typename T>
T PlcMemory::getValue(const std::string& name, T defaultValue) {
    if (!memoryMap.count(name)) {
        return defaultValue;
    }
    PlcVariable& var = memoryMap[name];
    switch (var.type) {
        case PlcValueType::BOOL: return static_cast<T>(var.value.bVal);
        case PlcValueType::BYTE: return static_cast<T>(var.value.ui8Val);
        case PlcValueType::INT: return static_cast<T>(var.value.i16Val);
        case PlcValueType::DINT: return static_cast<T>(var.value.ui32Val);
        case PlcValueType::REAL: return static_cast<T>(var.value.fVal);
        case PlcValueType::STRING_TYPE: {
            // Special handling for String type to avoid invalid cast
            if constexpr (std::is_same<T, String>::value) {
                return String(var.value.sVal);
            } else {
                return defaultValue; // Can't convert string to numeric type
            }
        }
    }
    return defaultValue; // Should not reach here
}

void PlcMemory::loadRetentiveMemory() {
    Preferences preferences;
    preferences.begin("plc_memory", true); // Use a separate namespace for PLC memory

    for (auto& pair : memoryMap) {
        PlcVariable& var = pair.second;
        if (var.isRetentive) {
            String key = String(pair.first.c_str());
            switch (var.type) {
                case PlcValueType::BOOL:
                    var.value.bVal = preferences.getBool(key.c_str(), var.value.bVal);
                    break;
                case PlcValueType::BYTE:
                    var.value.ui8Val = static_cast<uint8_t>(preferences.getUChar(key.c_str(), var.value.ui8Val));
                    break;
                case PlcValueType::INT:
                    var.value.i16Val = static_cast<int16_t>(preferences.getShort(key.c_str(), var.value.i16Val));
                    break;
                case PlcValueType::DINT:
                    var.value.ui32Val = preferences.getUInt(key.c_str(), var.value.ui32Val);
                    break;
                case PlcValueType::REAL:
                    var.value.fVal = preferences.getFloat(key.c_str(), var.value.fVal);
                    break;
                case PlcValueType::STRING_TYPE: {
                    String stored_string = preferences.getString(key.c_str(), "");
                    if (!stored_string.isEmpty()) {
                        strncpy(var.value.sVal, stored_string.c_str(), sizeof(var.value.sVal) - 1);
                        var.value.sVal[sizeof(var.value.sVal) - 1] = '\0';
                    }
                    break;
                }
            }
        }
    }
    preferences.end();
    EspHubLog->println("Retentive memory loaded from NVS.");
}

void PlcMemory::saveRetentiveMemory() {
    Preferences preferences;
    preferences.begin("plc_memory", false);

    for (auto& pair : memoryMap) {
        PlcVariable& var = pair.second;
        if (var.isRetentive) {
            String key = String(pair.first.c_str());
            switch (var.type) {
                case PlcValueType::BOOL:
                    preferences.putBool(key.c_str(), var.value.bVal);
                    break;
                case PlcValueType::BYTE:
                    preferences.putUChar(key.c_str(), var.value.ui8Val);
                    break;
                case PlcValueType::INT:
                    preferences.putShort(key.c_str(), var.value.i16Val);
                    break;
                case PlcValueType::DINT:
                    preferences.putUInt(key.c_str(), var.value.ui32Val);
                    break;
                case PlcValueType::REAL:
                    preferences.putFloat(key.c_str(), var.value.fVal);
                    break;
                case PlcValueType::STRING_TYPE:
                    preferences.putString(key.c_str(), var.value.sVal);
                    break;
            }
        }
    }
    preferences.end();
    EspHubLog->println("Retentive memory saved to NVS.");
}

void PlcMemory::clear() {
    memoryMap.clear();
}

// Template specialization for String type to avoid invalid casts
template<>
bool PlcMemory::setValue<String>(const std::string& name, String val) {
    if (!memoryMap.count(name)) {
        return false;
    }
    PlcVariable& var = memoryMap[name];
    if (var.type == PlcValueType::STRING_TYPE) {
        strncpy(var.value.sVal, val.c_str(), sizeof(var.value.sVal) - 1);
        var.value.sVal[sizeof(var.value.sVal) - 1] = '\0';
        return true;
    }
    return false; // Type mismatch
}

template<>
String PlcMemory::getValue<String>(const std::string& name, String defaultValue) {
    if (!memoryMap.count(name)) {
        return defaultValue;
    }
    PlcVariable& var = memoryMap[name];
    if (var.type == PlcValueType::STRING_TYPE) {
        return String(var.value.sVal);
    }
    return defaultValue; // Type mismatch
}

// Explicit template instantiations
template bool PlcMemory::setValue<bool>(const std::string& name, bool val);
template bool PlcMemory::setValue<int8_t>(const std::string& name, int8_t val);
template bool PlcMemory::setValue<uint8_t>(const std::string& name, uint8_t val);
template bool PlcMemory::setValue<int16_t>(const std::string& name, int16_t val);
template bool PlcMemory::setValue<uint16_t>(const std::string& name, uint16_t val);
template bool PlcMemory::setValue<int32_t>(const std::string& name, int32_t val);
template bool PlcMemory::setValue<uint32_t>(const std::string& name, uint32_t val);
template bool PlcMemory::setValue<float>(const std::string& name, float val);
template bool PlcMemory::setValue<double>(const std::string& name, double val);
// String specialization defined above

template bool PlcMemory::getValue<bool>(const std::string& name, bool defaultValue);
template int8_t PlcMemory::getValue<int8_t>(const std::string& name, int8_t defaultValue);
template uint8_t PlcMemory::getValue<uint8_t>(const std::string& name, uint8_t defaultValue);
template int16_t PlcMemory::getValue<int16_t>(const std::string& name, int16_t defaultValue);
template uint16_t PlcMemory::getValue<uint16_t>(const std::string& name, uint16_t defaultValue);
template int32_t PlcMemory::getValue<int32_t>(const std::string& name, int32_t defaultValue);
template uint32_t PlcMemory::getValue<uint32_t>(const std::string& name, uint32_t defaultValue);
template float PlcMemory::getValue<float>(const std::string& name, float defaultValue);
template double PlcMemory::getValue<double>(const std::string& name, double defaultValue);
// String specialization defined above

// ========== IO Point Management Implementation ==========

void PlcMemory::setDeviceRegistry(DeviceRegistry* registry) {
    deviceRegistry = registry;
    EspHubLog->println("PlcMemory: DeviceRegistry connected");
}

bool PlcMemory::registerIOPoint(const std::string& plcVarName, const std::string& endpointName,
                                IODirection direction, bool requiresFunction,
                                const std::string& functionName, bool autoSync) {
    if (!deviceRegistry) {
        EspHubLog->println("ERROR: DeviceRegistry not set in PlcMemory");
        return false;
    }

    // Check if PLC variable exists
    if (!memoryMap.count(plcVarName)) {
        EspHubLog->printf("ERROR: PLC variable '%s' not declared\n", plcVarName.c_str());
        return false;
    }

    // Create IO point structure
    PlcIOPoint ioPoint;
    ioPoint.plcVarName = String(plcVarName.c_str());
    ioPoint.mappedEndpoint = String(endpointName.c_str());
    ioPoint.direction = direction;
    ioPoint.requiresFunction = requiresFunction;
    ioPoint.functionName = String(functionName.c_str());
    ioPoint.autoSync = autoSync;

    // Register with DeviceRegistry
    bool result = deviceRegistry->registerIOPoint(ioPoint);
    if (result) {
        EspHubLog->printf("Registered IO point: %s <-> %s (%s)\n",
                         plcVarName.c_str(),
                         endpointName.c_str(),
                         (direction == IODirection::IO_INPUT) ? "INPUT" : "OUTPUT");
    }
    return result;
}

bool PlcMemory::unregisterIOPoint(const std::string& plcVarName) {
    if (!deviceRegistry) {
        return false;
    }
    return deviceRegistry->unregisterIOPoint(String(plcVarName.c_str()));
}

PlcIOPoint* PlcMemory::getIOPoint(const std::string& plcVarName) {
    if (!deviceRegistry) {
        return nullptr;
    }
    return deviceRegistry->getIOPoint(String(plcVarName.c_str()));
}

bool PlcMemory::isEndpointOnline(const std::string& endpointName) {
    if (!deviceRegistry) {
        return false;
    }
    Endpoint* endpoint = deviceRegistry->getEndpoint(String(endpointName.c_str()));
    return endpoint ? endpoint->isOnline : false;
}

PlcValue PlcMemory::getValueAsPlcValue(const std::string& name) {
    PlcValue result;
    if (!memoryMap.count(name)) {
        return result; // Return default bool value
    }

    PlcVariable& var = memoryMap[name];
    result.type = var.type;
    result.value = var.value;
    return result;
}

void PlcMemory::syncIOPoints() {
    if (!deviceRegistry) {
        return;
    }

    auto allIOPoints = deviceRegistry->getAllIOPoints();

    for (PlcIOPoint* ioPoint : allIOPoints) {
        if (!ioPoint || !ioPoint->autoSync) {
            continue;
        }

        std::string plcVarName = ioPoint->plcVarName.c_str();
        String endpointName = ioPoint->mappedEndpoint;

        // Check if endpoint exists and is online
        Endpoint* endpoint = deviceRegistry->getEndpoint(endpointName);
        if (!endpoint || !endpoint->isOnline) {
            continue; // Skip offline endpoints
        }

        if (ioPoint->direction == IODirection::IO_INPUT) {
            // INPUT: Copy from endpoint to PLC variable
            if (memoryMap.count(plcVarName)) {
                PlcVariable& var = memoryMap[plcVarName];
                const PlcValue& endpointValue = endpoint->currentValue;

                // Verify type compatibility
                if (var.type != endpointValue.type) {
                    EspHubLog->printf("WARNING: Type mismatch for %s: PLC=%d, Endpoint=%d\n",
                                     plcVarName.c_str(),
                                     (int)var.type,
                                     (int)endpointValue.type);
                    continue;
                }

                // Copy value from endpoint to PLC
                var.value = endpointValue.value;
            }
        } else {
            // OUTPUT: Copy from PLC variable to endpoint
            // Note: For outputs with requiresFunction=true, this should only be
            // called through dedicated function blocks, not here
            if (ioPoint->requiresFunction) {
                continue; // Skip outputs that require function calls
            }

            if (memoryMap.count(plcVarName)) {
                PlcValue plcValue = getValueAsPlcValue(plcVarName);
                deviceRegistry->updateEndpointValue(endpointName, plcValue);
            }
        }
    }
}