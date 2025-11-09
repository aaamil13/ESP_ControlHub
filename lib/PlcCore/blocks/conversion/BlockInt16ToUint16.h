#ifndef PLC_BLOCK_INT16_TO_UINT16_H
#define PLC_BLOCK_INT16_TO_UINT16_H

#include "../PlcBlock.h"
#include <string>

class BlockInt16ToUint16 : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var;
};

#endif // PLC_BLOCK_INT16_TO_UINT16_H