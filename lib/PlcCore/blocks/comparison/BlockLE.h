#ifndef PLC_BLOCK_LE_H
#define PLC_BLOCK_LE_H

#include "../PlcBlock.h"
#include <string>

class BlockLE : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input1_var;
    std::string input2_var;
    std::string output_var;
};

#endif // PLC_BLOCK_LE_H