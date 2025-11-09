#ifndef PLC_BLOCK_ABS_H
#define PLC_BLOCK_ABS_H

#include "../PlcBlock.h"
#include <string>

class BlockABS : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var;
};

#endif // PLC_BLOCK_ABS_H