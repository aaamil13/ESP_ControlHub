#ifndef PLC_BLOCK_NOT_H
#define PLC_BLOCK_NOT_H

#include "../PlcBlock.h"
#include <string>

class BlockNOT : public PlcBlock {
public:
    void configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;

private:
    std::string input_var;
    std::string output_var;
};

#endif // PLC_BLOCK_NOT_H