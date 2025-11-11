#ifndef PLC_BLOCK_TON_H
#define PLC_BLOCK_TON_H

#include "../PlcBlock.h"
#include <string>

class BlockTON : public PlcBlock {
public:
    BlockTON();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var_q;
    std::string output_var_et;
    unsigned long preset_time;
    unsigned long start_time;
    bool timing;
};

#endif // PLC_BLOCK_TON_H