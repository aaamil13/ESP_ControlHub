#ifndef PLC_BLOCK_STRING_COPY_H
#define PLC_BLOCK_STRING_COPY_H

#include "../PlcBlock.h"
#include <string>

class BlockStringCopy : public PlcBlock {
public:
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::string source_var;
    std::string destination_var;
    int start_index;
    int length;
};

#endif // PLC_BLOCK_STRING_COPY_H