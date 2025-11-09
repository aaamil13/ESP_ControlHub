#ifndef PLC_BLOCK_INT8_TO_UINT8_H
#define PLC_BLOCK_INT8_TO_UINT8_H

#include "../PlcBlock.h"
#include <string>

class BlockInt8ToUint8 : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var;
};

#endif // PLC_BLOCK_INT8_TO_UINT8_H