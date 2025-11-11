#ifndef PLC_BLOCK_CTU_H
#define PLC_BLOCK_CTU_H

#include "../PlcBlock.h"
#include <string>

class BlockCTU : public PlcBlock {
public:
    BlockCTU();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string cu_var;      // Count Up input
    std::string reset_var;   // Reset input
    std::string pv_var;      // Preset Value input
    std::string q_var;       // Output
    std::string cv_var;      // Current Value output

    bool last_cu_state;
};

#endif // PLC_BLOCK_CTU_H