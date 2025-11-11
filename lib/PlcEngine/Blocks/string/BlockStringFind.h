#ifndef PLC_BLOCK_STRING_FIND_H
#define PLC_BLOCK_STRING_FIND_H

#include "../PlcBlock.h"
#include <string>

class BlockStringFind : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_string_var;
    std::string substring_var;
    std::string output_index_var;
};

#endif // PLC_BLOCK_STRING_FIND_H