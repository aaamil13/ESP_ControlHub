#ifndef PLC_BLOCK_TOF_H
#define PLC_BLOCK_TOF_H

#include "../PlcBlock.h"
#include <string>

class BlockTOF : public PlcBlock {
public:
    BlockTOF();
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
    bool last_input_state;
};

#endif // PLC_BLOCK_TOF_H