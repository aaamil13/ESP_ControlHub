#ifndef PLC_BLOCK_CTUD_H
#define PLC_BLOCK_CTUD_H

#include "../PlcBlock.h"
#include <string>

class BlockCTUD : public PlcBlock {
public:
    BlockCTUD();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string cu_var;      // Count Up input
    std::string cd_var;      // Count Down input
    std::string reset_var;   // Reset input
    std::string load_var;    // Load input
    std::string pv_var;      // Preset Value input
    std::string qu_var;      // Count Up Output
    std::string qd_var;      // Count Down Output
    std::string cv_var;      // Current Value output

    bool last_cu_state;
    bool last_cd_state;
};

#endif // PLC_BLOCK_CTUD_H