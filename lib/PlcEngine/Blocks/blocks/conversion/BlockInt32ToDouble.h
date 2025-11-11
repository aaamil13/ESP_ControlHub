#ifndef PLC_BLOCK_INT32_TO_DOUBLE_H
#define PLC_BLOCK_INT32_TO_DOUBLE_H

#include "../PlcBlock.h"
#include <string>

class BlockInt32ToDouble : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var;
};

#endif // PLC_BLOCK_INT32_TO_DOUBLE_H