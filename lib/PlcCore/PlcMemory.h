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

struct PlcVariable {
    PlcValueUnion value;
    PlcValueType valueType; // To track which member of the union is active
    PlcValueType type; // Original PlcDataType enum
    bool isRetentive;
    String mesh_link; // Identifier for mesh-linked variables
};

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

private:
    std::map<std::string, PlcVariable> memoryMap;
    void loadRetentiveMemory();
};

#endif // PLC_MEMORY_H