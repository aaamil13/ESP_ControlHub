#ifndef BLOCK_STATUS_HANDLER_H
#define BLOCK_STATUS_HANDLER_H

#include "../PlcBlock.h"
#include <string>

// Forward declarations
class DeviceRegistry;

/**
 * BlockStatusHandler - PLC block for handling device status changes
 *
 * This block monitors an endpoint's online/offline status and provides
 * trigger outputs for event-driven PLC logic.
 *
 * Inputs:
 *   - endpoint_name (STRING): Full endpoint name to monitor
 *
 * Outputs:
 *   - is_online (BOOL): Current online status
 *   - on_online (BOOL): Trigger set to true when device goes online
 *   - on_offline (BOOL): Trigger set to true when device goes offline
 *
 * Usage in JSON:
 * {
 *   "type": "StatusHandler",
 *   "inputs": {
 *     "endpoint_name": "endpoint_var"
 *   },
 *   "outputs": {
 *     "is_online": "status_var",
 *     "on_online": "online_trigger",
 *     "on_offline": "offline_trigger"
 *   }
 * }
 */
class BlockStatusHandler : public PlcBlock {
public:
    BlockStatusHandler();
    ~BlockStatusHandler();

    bool configure(const JsonObject& config, PlcMemory& memory) override;
    void evaluate(PlcMemory& memory) override;
    JsonDocument getBlockSchema() override;

    // Set DeviceRegistry for status monitoring
    void setDeviceRegistry(DeviceRegistry* registry);

private:
    std::string endpoint_name_var;  // PLC variable containing endpoint name
    std::string is_online_var;      // Output: current online status
    std::string on_online_var;      // Output: online trigger
    std::string on_offline_var;     // Output: offline trigger

    DeviceRegistry* deviceRegistry;
    String monitoredEndpoint;       // Currently monitored endpoint
    bool lastKnownStatus;           // Last known online status
    bool initialized;

    // Update status from registry
    void updateStatus(PlcMemory& memory);
};

#endif // BLOCK_STATUS_HANDLER_H
