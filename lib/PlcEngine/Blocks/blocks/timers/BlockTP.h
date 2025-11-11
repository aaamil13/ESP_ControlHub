#ifndef PLC_BLOCK_TP_H
#define PLC_BLOCK_TP_H

#include "../PlcBlock.h"
#include <string>

class BlockTP : public PlcBlock {
public:
    BlockTP();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var_q;
    std::string output_var_et;
    unsigned long pulse_time;
    unsigned long start_time;
    bool timing;
    bool last_input_state;
};

#endif // PLC_BLOCK_TP_H