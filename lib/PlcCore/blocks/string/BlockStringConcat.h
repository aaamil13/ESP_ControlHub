#ifndef PLC_BLOCK_STRING_CONCAT_H
#define PLC_BLOCK_STRING_CONCAT_H

#include "../PlcBlock.h"
#include <string>
#include <vector>

class BlockStringConcat : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::vector<std::string> input_vars;
    std::string output_var;
};

#endif // PLC_BLOCK_STRING_CONCAT_H