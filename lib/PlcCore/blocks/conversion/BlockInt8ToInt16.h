#ifndef PLC_BLOCK_INT8_TO_INT16_H
#define PLC_BLOCK_INT8_TO_INT16_H

#include "../PlcBlock.h"
#include <string>

class BlockInt8ToInt16 : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var;
};

#endif // PLC_BLOCK_INT8_TO_INT16_H