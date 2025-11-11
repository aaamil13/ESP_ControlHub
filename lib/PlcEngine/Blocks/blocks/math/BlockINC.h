#ifndef PLC_BLOCK_INC_H
#define PLC_BLOCK_INC_H

#include "../PlcBlock.h"
#include <string>

class BlockINC : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_output_var; // Variable to increment
};

#endif // PLC_BLOCK_INC_H