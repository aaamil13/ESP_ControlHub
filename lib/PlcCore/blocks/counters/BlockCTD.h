#ifndef PLC_BLOCK_CTD_H
#define PLC_BLOCK_CTD_H

#include "../PlcBlock.h"
#include <string>

class BlockCTD : public PlcBlock {
public:
    BlockCTD();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string cd_var;      // Count Down input
    std::string load_var;    // Load input
    std::string pv_var;      // Preset Value input
    std::string q_var;       // Output
    std::string cv_var;      // Current Value output

    bool last_cd_state;
};

#endif // PLC_BLOCK_CTD_H