#ifndef PLC_BLOCK_DEC_H
#define PLC_BLOCK_DEC_H

#include "../PlcBlock.h"
#include <string>

class BlockDEC : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_output_var; // Variable to decrement
};

#endif // PLC_BLOCK_DEC_H