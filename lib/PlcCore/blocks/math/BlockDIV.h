#ifndef PLC_BLOCK_DIV_H
#define PLC_BLOCK_DIV_H

#include "../PlcBlock.h"
#include <string>
#include <vector>

class BlockDIV : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::vector<std::string> input_vars; // First input is divided by subsequent inputs
    std::string output_var;
};

#endif // PLC_BLOCK_DIV_H