#include "../PlcEngine/Engine/PlcMemory.h"
#include "Preferences.h"
#include <StreamLogger.h> // Include for EspHubLog
#include <DeviceRegistry.h> // Include for IO point integration
#include <type_traits>

void PlcMemory::clear() {}

// Template specialization for String type to avoid invalid casts
template<>
bool PlcMemory::setValue<String>(const std::string& name, String val) {
    return true;
}

template<>
String PlcMemory::getValue<String>(const std::string& name, String defaultValue) {
    return defaultValue;
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
// std::string handled by template specialization above

template bool PlcMemory::getValue<bool>(const std::string& name, bool defaultValue);
template int8_t PlcMemory::getValue<int8_t>(const std::string& name, int8_t defaultValue);
template uint8_t PlcMemory::getValue<uint8_t>(const std::string& name, uint8_t defaultValue);
template int16_t PlcMemory::getValue<int16_t>(const std::string& name, int16_t defaultValue);
template uint16_t PlcMemory::getValue<uint16_t>(const std::string& name, uint16_t defaultValue);
template int32_t PlcMemory::getValue<int32_t>(const std::string& name, int32_t defaultValue);
template uint32_t PlcMemory::getValue<uint32_t>(const std::string& name, uint32_t defaultValue);
template float PlcMemory::getValue<float>(const std::string& name, float defaultValue);
template double PlcMemory::getValue<double>(const std::string& name, double defaultValue);
// std::string handled by template specialization above

// ========== IO Point Management Implementation ==========

void PlcMemory::setDeviceRegistry(DeviceRegistry* registry) {
    deviceRegistry = registry;
}

bool PlcMemory::registerIOPoint(const std::string& plcVarName, const std::string& endpointName,
                                IODirection direction, const std::string& ownerProgram,
                                bool requiresFunction, const std::string& functionName,
                                bool autoSync) {
    return true;
}

bool PlcMemory::unregisterIOPoint(const std::string& plcVarName) {
    return true;
}

PlcIOPoint* PlcMemory::getIOPoint(const std::string& plcVarName) {
    return nullptr;
}

bool PlcMemory::isEndpointOnline(const std::string& endpointName) {
    return false;
}

PlcValue PlcMemory::getValueAsPlcValue(const std::string& name) {
    PlcValue result;
    return result;
}

void PlcMemory::syncIOPoints(IODirection* filterDirection) {}

size_t PlcMemory::getMemoryUsage() const {
    return 0;
}