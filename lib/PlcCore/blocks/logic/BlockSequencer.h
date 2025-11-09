#ifndef PLC_BLOCK_SEQUENCER_H
#define PLC_BLOCK_SEQUENCER_H

#include "../PlcBlock.h"
#include <string>
#include <vector>

struct SequencerStep {
    JsonArray actions; // Actions to perform in this step
    std::string transition_condition_var; // Variable to check for transition
    unsigned long timeout_ms; // Timeout for this step
    unsigned long start_time; // Internal: when this step started
};

class BlockSequencer : public PlcBlock {
public:
    BlockSequencer();
    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

private:
    std::vector<SequencerStep> steps;
    int current_step;
    std::string output_done_var; // Output when sequence is complete
    std::string output_active_var; // Output when sequence is active

    void executeActions(const JsonArray& actions, PlcMemory& memory);
};

#endif // PLC_BLOCK_SEQUENCER_H