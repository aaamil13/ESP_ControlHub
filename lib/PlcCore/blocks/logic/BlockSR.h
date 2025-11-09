#ifndef PLC_BLOCK_SR_H
#define PLC_BLOCK_SR_H

#include "../PlcBlock.h"
#include <string>

class BlockSR : public PlcBlock {
public:
    BlockSR();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string set_var;
    std::string reset_var;
    std::string output_var;
};

#endif // PLC_BLOCK_SR_H