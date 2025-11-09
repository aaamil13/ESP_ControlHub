#ifndef PLC_MEMORY_H
#define PLC_MEMORY_H

#include <Arduino.h>
#include <variant>
#include <map>
#include <string>

// Supported data types for our PLC
using PlcValue = std::variant<bool, uint8_t, int16_t, uint32_t, float, String>;

// Enum to map JSON types to our variant
enum class PlcDataType {
    BOOL, BYTE, INT, DINT, REAL, STRING
};

struct PlcVariable {
    PlcValue value;
    PlcDataType type;
    bool isRetentive;
    String mesh_link; // Identifier for mesh-linked variables
};

class PlcMemory {
public:
    PlcMemory();
    void begin(); // Load retentive memory from NVS

    bool declareVariable(const std::string& name, PlcDataType type, bool isRetentive = false);

    template<typename T>
    bool setValue(const std::string& name, T val);

    template<typename T>
    T getValue(const std::string& name, T defaultValue = T{});

    void saveRetentiveMemory();
    void clear();

private:
    std::map<std::string, PlcVariable> memoryMap;
    void loadRetentiveMemory();
};

#endif // PLC_MEMORY_H