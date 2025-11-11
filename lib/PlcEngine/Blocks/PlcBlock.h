#ifndef PLC_BLOCK_H
#define PLC_BLOCK_H

#include "../Engine/PlcMemory.h"
#include <ArduinoJson.h>

class PlcBlock {
public:
    virtual ~PlcBlock() {}
    virtual bool configure(const JsonObject& config, PlcMemory& memory) = 0;
    virtual void evaluate(PlcMemory& memory) = 0;
    virtual JsonDocument getBlockSchema() = 0; // Returns a JSON schema for validation
};

#endif // PLC_BLOCK_H