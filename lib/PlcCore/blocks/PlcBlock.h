#ifndef PLC_BLOCK_H
#define PLC_BLOCK_H

#include "../PlcMemory.h"
#include <ArduinoJson.h>

class PlcBlock {
public:
    virtual ~PlcBlock() {}
    virtual void configure(const JsonObject& config, PlcMemory& memory) = 0;
    virtual void evaluate(PlcMemory& memory) = 0;
};

#endif // PLC_BLOCK_H