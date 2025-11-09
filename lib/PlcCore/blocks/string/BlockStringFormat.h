#ifndef PLC_BLOCK_STRING_FORMAT_H
#define PLC_BLOCK_STRING_FORMAT_H

#include "../PlcBlock.h"
#include <string>
#include <vector>

class BlockStringFormat : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string format_string_var;
    std::vector<std::string> input_vars; // Variables to insert into format string
    std::string output_var;
};

#endif // PLC_BLOCK_STRING_FORMAT_H