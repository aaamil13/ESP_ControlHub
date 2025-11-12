#ifndef PLC_MEMORY_H
#define PLC_MEMORY_H

#include <Arduino.h>
#include <map>
#include <string>

// Supported data types for our PLC
enum class PlcValueType {
    BOOL, BYTE, INT, DINT, REAL, STRING_TYPE // Renamed to avoid conflict with String class
};

union PlcValueUnion {
    bool bVal;
    uint8_t ui8Val;
    int16_t i16Val;
    uint32_t ui32Val;
    float fVal;
    char sVal[64]; // Assuming a max string length for union

    // Constructor to initialize union members
    PlcValueUnion() : bVal(false) {}
};

// PlcValue - type-safe value container for endpoint integration
struct PlcValue {
    PlcValueUnion value;
    PlcValueType type;

    PlcValue() : type(PlcValueType::BOOL) {
        value.bVal = false;
    }

    PlcValue(PlcValueType t) : type(t) {
        value.bVal = false; // Initialize to zero
    }
};

struct PlcVariable {
    PlcValueUnion value;
    PlcValueType valueType; // To track which member of the union is active
    PlcValueType type; // Original PlcDataType enum
    bool isRetentive;
    String mesh_link; // Identifier for mesh-linked variables
};

// Forward declarations
class DeviceRegistry;
enum class IODirection;
struct PlcIOPoint;

class PlcMemory {
public:
    PlcMemory();
    void begin(); // Load retentive memory from NVS

    bool declareVariable(const std::string& name, PlcValueType type, bool isRetentive = false, const String& mesh_link = "");

    template<typename T>
    bool setValue(const std::string& name, T val);

    template<typename T>
    T getValue(const std::string& name, T defaultValue = T{});

    void saveRetentiveMemory();
    void clear(); // New method

    // IO Point Management - Integration with DeviceRegistry
    void setDeviceRegistry(DeviceRegistry* registry);
    bool registerIOPoint(const std::string& plcVarName, const std::string& endpointName,
                        IODirection direction, bool requiresFunction = false,
                        const std::string& functionName = "", bool autoSync = true);
    bool unregisterIOPoint(const std::string& plcVarName);
    PlcIOPoint* getIOPoint(const std::string& plcVarName);
    bool isEndpointOnline(const std::string& endpointName);
    void syncIOPoints(); // Sync between PLC variables and endpoints
    PlcValue getValueAsPlcValue(const std::string& name); // Get value as PlcValue struct

    // Memory usage
    size_t getMemoryUsage() const;

private:
    std::map<std::string, PlcVariable> memoryMap;
    DeviceRegistry* deviceRegistry;
    void loadRetentiveMemory();
};

#endif // PLC_MEMORY_H