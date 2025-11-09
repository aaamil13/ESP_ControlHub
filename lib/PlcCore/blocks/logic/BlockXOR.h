#ifndef PLC_BLOCK_XOR_H
#define PLC_BLOCK_XOR_H

#include "../PlcBlock.h"
#include <vector>
#include <string>

class BlockXOR : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::vector<std::string> input_vars;
    std::string output_var;
};

#endif // PLC_BLOCK_XOR_H