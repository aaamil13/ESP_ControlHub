#ifndef PLC_BLOCK_TIME_COMPARE_H
#define PLC_BLOCK_TIME_COMPARE_H

#include "PlcSchedulerBlock.h"
#include <string>

class BlockTimeCompare : public PlcSchedulerBlock {
public:
    BlockTimeCompare(TimeManager* timeManager);
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string output_var;
    int hour;
    int minute;
    int second;
};

#endif // PLC_BLOCK_TIME_COMPARE_H