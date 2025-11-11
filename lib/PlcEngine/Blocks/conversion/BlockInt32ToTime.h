#ifndef PLC_BLOCK_INT32_TO_TIME_H
#define PLC_BLOCK_INT32_TO_TIME_H

#include "../PlcBlock.h"
#include <string>

class BlockInt32ToTime : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string input_var;
    std::string output_var_hour;
    std::string output_var_minute;
    std::string output_var_second;
};

#endif // PLC_BLOCK_INT32_TO_TIME_H