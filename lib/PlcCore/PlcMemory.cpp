#include "PlcMemory.h"
#include "Preferences.h"

PlcMemory::PlcMemory() {}

void PlcMemory::begin() {
    loadRetentiveMemory();
}

bool PlcMemory::declareVariable(const std::string& name, PlcDataType type, bool isRetentive, const String& mesh_link) {
    if (memoryMap.count(name)) {
        return false; // Variable already exists
    }
    PlcVariable newVar;
    newVar.type = type;
    newVar.isRetentive = isRetentive;
    newVar.mesh_link = mesh_link;
    // Initialize with default value
    switch (type) {
        case PlcDataType::BOOL: newVar.value = false; break;
        case PlcDataType::BYTE: newVar.value = (uint8_t)0; break;
        case PlcDataType::INT: newVar.value = (int16_t)0; break;
        case PlcDataType::DINT: newVar.value = (uint32_t)0; break;
        case PlcDataType::REAL: newVar.value = 0.0f; break;
        case PlcDataType::STRING: newVar.value = String(""); break;
    }
    memoryMap[name] = newVar;
    return true;
}

template<typename T>
bool PlcMemory::setValue(const std::string& name, T val) {
    if (!memoryMap.count(name)) {
        return false; // Variable not declared
    }
    try {
        memoryMap[name].value = val;
        return true;
    } catch (const std::bad_variant_access&) {
        return false; // Type mismatch
    }
}

template<typename T>
T PlcMemory::getValue(const std::string& name, T defaultValue) {
    if (!memoryMap.count(name)) {
        return defaultValue;
    }
    try {
        return std::get<T>(memoryMap[name].value);
    } catch (const std::bad_variant_access&) {
        return defaultValue; // Type mismatch
    }
}

void PlcMemory::loadRetentiveMemory() {
    Preferences preferences;
    preferences.begin("plc_memory", true); // Use a separate namespace for PLC memory

    for (auto& pair : memoryMap) {
        PlcVariable& var = pair.second;
        if (var.isRetentive) {
            String key = String(pair.first.c_str());
            switch (var.type) {
                case PlcDataType::BOOL:
                    var.value = preferences.getBool(key.c_str(), std::get<bool>(var.value));
                    break;
                case PlcDataType::BYTE:
                    var.value = static_cast<uint8_t>(preferences.getUChar(key.c_str(), std::get<uint8_t>(var.value)));
                    break;
                case PlcDataType::INT:
                    var.value = static_cast<int16_t>(preferences.getShort(key.c_str(), std::get<int16_t>(var.value)));
                    break;
                case PlcDataType::DINT:
                    var.value = preferences.getUInt(key.c_str(), std::get<uint32_t>(var.value));
                    break;
                case PlcDataType::REAL:
                    var.value = preferences.getFloat(key.c_str(), std::get<float>(var.value));
                    break;
                case PlcDataType::STRING:
                    // String serialization is more complex, requires dynamic allocation or fixed size buffer
                    // For now, just log a warning
                    Log->printf("WARNING: String variable '%s' is retentive but not fully supported for NVS persistence yet.\n", key.c_str());
                    break;
            }
        }
    }
    preferences.end();
    Log->println("Retentive memory loaded from NVS.");
}

void PlcMemory::saveRetentiveMemory() {
    Preferences preferences;
    preferences.begin("plc_memory", false);

    for (auto& pair : memoryMap) {
        PlcVariable& var = pair.second;
        if (var.isRetentive) {
            String key = String(pair.first.c_str());
            switch (var.type) {
                case PlcDataType::BOOL:
                    preferences.putBool(key.c_str(), std::get<bool>(var.value));
                    break;
                case PlcDataType::BYTE:
                    preferences.putUChar(key.c_str(), std::get<uint8_t>(var.value));
                    break;
                case PlcDataType::INT:
                    preferences.putShort(key.c_str(), std::get<int16_t>(var.value));
                    break;
                case PlcDataType::DINT:
                    preferences.putUInt(key.c_str(), std::get<uint32_t>(var.value));
                    break;
                case PlcDataType::REAL:
                    preferences.putFloat(key.c_str(), std::get<float>(var.value));
                    break;
                case PlcDataType::STRING:
                    // String serialization is more complex, requires dynamic allocation or fixed size buffer
                    // For now, just log a warning
                    Log->printf("WARNING: String variable '%s' is retentive but not fully supported for NVS persistence yet.\n", key.c_str());
                    break;
            }
        }
    }
    preferences.end();
    Log->println("Retentive memory saved to NVS.");
}

void PlcMemory::clear() {
    memoryMap.clear();
}

// Explicit template instantiations
template bool PlcMemory::setValue<bool>(const std::string& name, bool val);
template bool PlcMemory::setValue<uint8_t>(const std::string& name, uint8_t val);
template bool PlcMemory::setValue<int16_t>(const std::string& name, int16_t val);
template bool PlcMemory::setValue<uint32_t>(const std::string& name, uint32_t val);
template bool PlcMemory::setValue<float>(const std::string& name, float val);
template bool PlcMemory::setValue<String>(const std::string& name, String val);

template bool PlcMemory::getValue<bool>(const std::string& name, bool defaultValue);
template uint8_t PlcMemory::getValue<uint8_t>(const std::string& name, uint8_t defaultValue);
template int16_t PlcMemory::getValue<int16_t>(const std::string& name, int16_t defaultValue);
template uint32_t PlcMemory::getValue<uint32_t>(const std::string& name, uint32_t defaultValue);
template float PlcMemory::getValue<float>(const std::string& name, float defaultValue);
template String PlcMemory::getValue<String>(const std::string& name, String defaultValue);