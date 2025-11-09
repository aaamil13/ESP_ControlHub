#ifndef PLC_BLOCK_SUB_H
#define PLC_BLOCK_SUB_H

#include "../PlcBlock.h"
#include <string>
#include <vector>

class BlockSUB : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::vector<std::string> input_vars; // First input is subtracted by subsequent inputs
    std::string output_var;
};

#endif // PLC_BLOCK_SUB_H