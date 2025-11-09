#ifndef PLC_BLOCK_RS_H
#define PLC_BLOCK_RS_H

#include "../PlcBlock.h"
#include <string>

class BlockRS : public PlcBlock {
public:
    BlockRS();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string set_var;
    std::string reset_var;
    std::string output_var;
};

#endif // PLC_BLOCK_RS_H