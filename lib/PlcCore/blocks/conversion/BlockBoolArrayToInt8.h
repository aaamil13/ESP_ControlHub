#ifndef PLC_BLOCK_BOOL_ARRAY_TO_INT8_H
#define PLC_BLOCK_BOOL_ARRAY_TO_INT8_H

#include "../PlcBlock.h"
#include <string>
#include <vector>

class BlockBoolArrayToInt8 : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::vector<std::string> input_vars; // Array of boolean variable names
    std::string output_var;
};

#endif // PLC_BLOCK_BOOL_ARRAY_TO_INT8_H