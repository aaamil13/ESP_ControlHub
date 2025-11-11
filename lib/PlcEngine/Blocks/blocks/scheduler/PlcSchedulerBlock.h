#ifndef PLC_SCHEDULER_BLOCK_H
#define PLC_SCHEDULER_BLOCK_H

#include "../PlcBlock.h"
#include <TimeManager.h> // Assuming TimeManager is accessible

class PlcSchedulerBlock : public PlcBlock {
public:
    virtual ~PlcSchedulerBlock() {}
    // Common methods for scheduler blocks can go here
protected:
    TimeManager* _timeManager; // Pointer to the global TimeManager instance
};

#endif // PLC_SCHEDULER_BLOCK_H