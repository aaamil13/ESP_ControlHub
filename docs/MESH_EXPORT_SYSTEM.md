# Mesh Variable Export System

## Overview

The Mesh Variable Export System enables seamless variable sharing between multiple ESP hubs in a painlessMesh network. It provides a publish/subscribe model where hubs can share their PLC variables with other hubs and consume variables from remote hubs.

### Key Features

- **Publish local variables** to mesh network with configurable sync intervals
- **Subscribe to remote variables** from other hubs with timeout detection
- **Automatic synchronization** on value change with throttling
- **Namespace support** using `hub_id.program.variable` format
- **Stale detection** for offline or unresponsive remote hubs
- **Type-safe** variable synchronization using PlcValue system
- **Minimal overhead** with efficient change detection and throttling

## Architecture

### Components

1. **MeshExportManager** - Core mesh variable sharing engine
2. **VariableRegistry** - Unified variable access layer (integration point)
3. **PlcEngine** - PLC program memory backend
4. **painlessMesh** - ESP32 mesh networking library

### Message Flow

```
Hub A (Publisher)                    Mesh Network                    Hub B (Subscriber)
     |                                    |                                |
     | 1. Variable changes                |                                |
     | (PLC memory update)                |                                |
     |                                    |                                |
     | 2. MeshExportManager               |                                |
     |    detects change via              |                                |
     |    change callback                 |                                |
     |                                    |                                |
     | 3. Check if should                 |                                |
     |    publish (threshold,             |                                |
     |    interval)                       |                                |
     |                                    |                                |
     | 4. Serialize variable              |                                |
     |    value to JSON                   |                                |
     |                                    |                                |
     | 5. Send VARIABLE_SYNC -----------> |                                |
     |    message to mesh                 |                                |
     |                                    |                                |
     |                                    | 6. Mesh broadcasts ----------> |
     |                                    |    message to all nodes        |
     |                                    |                                |
     |                                    |                  7. MeshExportManager
     |                                    |                     receives message
     |                                    |                                |
     |                                    |                  8. Parse hub_id,
     |                                    |                     check if subscribed
     |                                    |                                |
     |                                    |                  9. Update local PLC
     |                                    |                     variable (alias)
     |                                    |                                |
     |                                    |                 10. Reset timeout timer
     |                                    |                                |
     |                                    | <---------- VARIABLE_REQUEST   |
     |                                    |              (optional, on demand)
     |                                    |                                |
```

## Configuration

### JSON Structure

```json
{
  "mesh_export": {
    "publish": {
      "variable_name": {
        "sync_interval_ms": 10000,
        "sync_on_change": true,
        "min_change_threshold": 0.5
      }
    },
    "subscribe": {
      "hub_id.program.variable": {
        "local_alias": "local_variable_name",
        "timeout_ms": 30000
      }
    }
  }
}
```

### Configuration Fields

#### Publish Rules

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sync_interval_ms` | number | 10000 | Minimum time between publishes (heartbeat) |
| `sync_on_change` | boolean | true | Publish immediately on value change |
| `min_change_threshold` | number | 0.0 | Minimum change for numeric values to trigger sync |

#### Subscribe Rules

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `local_alias` | string | required | Local PLC variable name for storing remote value |
| `timeout_ms` | number | 30000 | Mark as stale if no update within this time |

### Namespace Format

Variables are identified using the format: `hub_id.program_name.variable_name`

- **hub_id**: Automatically generated as `hub_` + MAC address (e.g., `hub_a1b2c3d4`)
- **program_name**: PLC program name (e.g., `main`)
- **variable_name**: Variable name within the program (e.g., `temperature`)

Example: `hub_a1b2c3d4.main.temperature`

## API Usage

### Initialization

```cpp
// In EspHub::begin()
meshExportManager.begin();
meshExportManager.setMesh(&mesh);
meshExportManager.setVariableRegistry(&variableRegistry);
meshExportManager.setPlcEngine(&plcEngine);
meshExportManager.setLocalHubId("hub_" + String(ESP.getEfuseMac(), HEX));
```

### Adding Publish Rules

```cpp
// Publish temperature every 10s or on 0.5°C change
meshExportManager.addPublishRule("main.temperature", 10000, true);

// Get the rule and set threshold
MeshPublishRule* rule = meshExportManager.getPublishRule("main.temperature");
if (rule) {
    rule->minChangeThreshold = 0.5;
}
```

### Adding Subscribe Rules

```cpp
// Subscribe to outdoor temperature from remote hub
meshExportManager.addSubscribeRule(
    "hub_a1b2c3d4.main.outdoor_temp",  // Remote variable
    "main.remote_outdoor_temp",         // Local alias
    30000                               // Timeout (30s)
);
```

### Manual Publishing

```cpp
// Force publish immediately
meshExportManager.publishVariable("main.temperature", true);

// Publish all configured variables
meshExportManager.publishAllVariables(false);
```

### Requesting Remote Variables

```cpp
// Request latest value from remote hub
meshExportManager.requestRemoteVariable("hub_a1b2c3d4.main.pump_status");
```

### Checking Subscription Status

```cpp
// Check if subscription is stale (timeout exceeded)
bool isStale = meshExportManager.isSubscriptionStale("hub_a1b2c3d4.main.outdoor_temp");
if (isStale) {
    EspHubLog->println("WARNING: Remote outdoor temperature not updating");
}
```

### Statistics

```cpp
MeshExportManager::MeshExportStats stats = meshExportManager.getStatistics();
EspHubLog->printf("Publish rules: %d\n", stats.totalPublishRules);
EspHubLog->printf("Subscribe rules: %d\n", stats.totalSubscribeRules);
EspHubLog->printf("Total publishes: %lu\n", stats.totalPublishes);
EspHubLog->printf("Total received: %lu\n", stats.totalReceived);
EspHubLog->printf("Stale subscriptions: %d\n", stats.staleSubscriptions);
```

### Loading Configuration from JSON

```cpp
// Load from JsonObject
StaticJsonDocument<4096> doc;
deserializeJson(doc, jsonString);
meshExportManager.loadConfig(doc["mesh_export"].as<JsonObject>());

// Load from file
meshExportManager.loadConfigFromFile("/config/mesh_export.json");

// Save to file
meshExportManager.saveConfig("/config/mesh_export.json");
```

## Integration with PLC

### Using Remote Variables in PLC Programs

Once subscribed, remote variables can be used in PLC programs just like local variables:

```cpp
// Subscribe to remote temperature
meshExportManager.addSubscribeRule(
    "hub_sensor.main.temperature",
    "main.remote_temp",
    30000
);

// Now use in PLC ladder logic
// Rung: Start heater when remote_temp < 20.0
// Contacts: main.remote_temp < 20.0, main.heater_enabled
// Coils: main.heater_on
```

### Variable Registration

When a subscribe rule is added, the local alias variable is automatically registered in the VariableRegistry and created in PLC memory if it doesn't exist.

### Change Detection

Published variables automatically trigger sync when their value changes in PLC memory, thanks to the VariableRegistry change callback system.

## Use Cases

### 1. Distributed Sensor Network

**Scenario**: Multiple sensor hubs collecting data, one main controller

**Hub Configuration**:

```json
// Sensor Hub 1
{
  "mesh_export": {
    "publish": {
      "main.temperature": {
        "sync_interval_ms": 10000,
        "sync_on_change": true,
        "min_change_threshold": 0.5
      },
      "main.humidity": {
        "sync_interval_ms": 15000,
        "sync_on_change": true,
        "min_change_threshold": 2.0
      }
    }
  }
}

// Controller Hub
{
  "mesh_export": {
    "subscribe": {
      "hub_sensor1.main.temperature": {
        "local_alias": "main.zone1_temp",
        "timeout_ms": 30000
      },
      "hub_sensor1.main.humidity": {
        "local_alias": "main.zone1_humidity",
        "timeout_ms": 45000
      },
      "hub_sensor2.main.temperature": {
        "local_alias": "main.zone2_temp",
        "timeout_ms": 30000
      }
    }
  }
}
```

### 2. Redundant Pump Control

**Scenario**: Two pumps with interlock - don't run both simultaneously

**Hub Configuration**:

```json
// Pump A Hub
{
  "mesh_export": {
    "publish": {
      "main.pump_a_running": {
        "sync_interval_ms": 2000,
        "sync_on_change": true
      }
    },
    "subscribe": {
      "hub_pump_b.main.pump_b_running": {
        "local_alias": "main.other_pump_running",
        "timeout_ms": 10000
      }
    }
  }
}

// PLC Logic:
// Rung: Start pump A only if pump B not running
// Contacts: main.start_button, !main.other_pump_running
// Coils: main.pump_a_running
```

### 3. Alarm Propagation

**Scenario**: Critical alarms broadcast to all hubs

**Hub Configuration**:

```json
// Any Hub
{
  "mesh_export": {
    "publish": {
      "main.critical_alarm": {
        "sync_interval_ms": 1000,
        "sync_on_change": true
      }
    },
    "subscribe": {
      "hub_*.main.critical_alarm": {
        "local_alias": "main.remote_alarm",
        "timeout_ms": 5000
      }
    }
  }
}
```

## Performance Tuning

### Sync Interval Guidelines

| Update Speed | sync_interval_ms | Use Case |
|-------------|------------------|----------|
| Critical | 1000-2000 | Safety alarms, emergency stops |
| Fast | 5000-10000 | Control status, actuator feedback |
| Normal | 10000-30000 | Sensor readings, environmental data |
| Slow | 60000+ | Configuration, infrequent status |

### Timeout Guidelines

**Rule of thumb**: `timeout_ms = 2-3 × sync_interval_ms`

This allows for:
- One missed message due to mesh congestion
- Retry attempts
- Grace period for mesh re-routing

### Change Threshold Guidelines

For numeric values, set `min_change_threshold` to avoid unnecessary syncs:

| Sensor Type | Threshold | Reason |
|------------|-----------|---------|
| Temperature | 0.5°C | Noise filtering |
| Humidity | 2% | Natural fluctuation |
| Pressure | 5 hPa | Sensor precision |
| Voltage | 0.1V | ADC resolution |
| Current | 0.05A | Measurement accuracy |

### Reducing Mesh Traffic

**Problem**: Too many mesh messages causing congestion

**Solutions**:
1. Increase `sync_interval_ms` for non-critical variables
2. Disable `sync_on_change` for frequently changing values
3. Increase `min_change_threshold` to filter noise
4. Group related values into single message (future enhancement)

## Mesh Protocol

### VARIABLE_SYNC Message

Broadcast to all mesh nodes when a variable is published:

```json
{
  "type": 13,
  "hub_id": "hub_a1b2c3d4",
  "var_name": "main.temperature",
  "value": 23.5,
  "value_type": 2,
  "timestamp": 1234567890
}
```

### VARIABLE_REQUEST Message

Sent to specific node to request latest value:

```json
{
  "type": 14,
  "hub_id": "hub_a1b2c3d4",
  "var_name": "main.temperature"
}
```

Response is a VARIABLE_SYNC message.

## Troubleshooting

### Subscription Marked as Stale

**Symptoms**:
- `isSubscriptionStale()` returns true
- Remote variable not updating in PLC

**Causes**:
1. Remote hub offline or out of mesh range
2. Remote hub not publishing the variable
3. `timeout_ms` too short for `sync_interval_ms`
4. Mesh network congestion

**Solutions**:
1. Check mesh connectivity: `mesh.getNodeList()`
2. Verify remote hub configuration
3. Increase `timeout_ms` (should be 2-3× sync_interval)
4. Check mesh topology with `mesh.subConnectionJson()`

### Variable Not Updating

**Symptoms**:
- Subscribed variable never receives updates
- Local alias remains at default value

**Causes**:
1. Incorrect hub_id in subscription
2. Variable name mismatch (spelling, case)
3. Local alias not created in PLC memory
4. Variable type mismatch

**Solutions**:
1. Verify hub_id with `getLocalHubId()`
2. Check exact variable names on both hubs
3. Ensure local alias exists in PLC program
4. Match PlcValueType on both sides

### High Mesh Traffic

**Symptoms**:
- Mesh messages delayed
- Mesh reconnections
- High CPU usage

**Causes**:
1. Too many variables with `sync_on_change=true`
2. Very short `sync_interval_ms` values
3. Noisy sensors triggering frequent syncs

**Solutions**:
1. Disable `sync_on_change` for non-critical variables
2. Increase `sync_interval_ms` to reasonable values
3. Use `min_change_threshold` to filter sensor noise
4. Optimize mesh topology (reduce hops)

### Variables Out of Sync

**Symptoms**:
- Values different from expected
- Delayed updates

**Causes**:
1. Clock skew between hubs
2. Message reordering in mesh
3. Update race conditions

**Solutions**:
1. Enable time sync: `mesh.onNodeTimeAdjusted()`
2. Use timestamps to detect stale updates
3. Implement message sequence numbers (future enhancement)

## Best Practices

### 1. Design for Mesh Topology

- **Minimize hops**: Keep critical subscribers close to publishers
- **Hub placement**: Position hubs to ensure reliable connectivity
- **Root node**: Mesh root typically has best connectivity

### 2. Configure Timeouts Properly

```cpp
// Good: timeout is 3× sync interval
meshExportManager.addSubscribeRule("hub_x.main.temp", "main.remote_temp", 30000);  // 30s timeout
// Publish interval on remote: 10s

// Bad: timeout too short
meshExportManager.addSubscribeRule("hub_x.main.temp", "main.remote_temp", 5000);   // 5s timeout
// Publish interval on remote: 10s - will frequently mark as stale!
```

### 3. Use Appropriate Change Thresholds

```cpp
// Good: Filter noise
MeshPublishRule rule;
rule.variableName = "main.temperature";
rule.syncOnChange = true;
rule.minChangeThreshold = 0.5;  // Only sync on 0.5°C change

// Bad: Sync on every tiny change
rule.minChangeThreshold = 0.0;  // Will sync on 0.01°C noise
```

### 4. Monitor Stale Subscriptions

```cpp
void checkMeshHealth() {
    auto stats = meshExportManager.getStatistics();
    if (stats.staleSubscriptions > 0) {
        EspHubLog->printf("WARNING: %d stale subscriptions\n", stats.staleSubscriptions);

        // Check each subscription
        for (const auto& varName : meshExportManager.getSubscribedVariables()) {
            if (meshExportManager.isSubscriptionStale(varName)) {
                EspHubLog->printf("  - %s is stale\n", varName.c_str());
            }
        }
    }
}
```

### 5. Handle Offline Hubs Gracefully

```cpp
// In PLC logic, check if subscription is stale
bool remoteTempValid = !meshExportManager.isSubscriptionStale("hub_sensor.main.temperature");

if (remoteTempValid) {
    // Use remote temperature
    float temp = plcEngine.getProgram("main")->getMemory().getValue<float>("remote_temp");
    // ... control logic
} else {
    // Fallback: use local sensor or safe default
    EspHubLog->println("Remote sensor offline, using local temperature");
    float temp = plcEngine.getProgram("main")->getMemory().getValue<float>("local_temp");
}
```

## Comparison with MQTT Export

| Feature | Mesh Export | MQTT Export |
|---------|------------|-------------|
| **Range** | Mesh network (100m per hop) | WiFi + Internet (unlimited) |
| **Latency** | Low (direct mesh) | Medium (via broker) |
| **Dependency** | No external broker | Requires MQTT broker |
| **Root node** | Any node | Only mesh root |
| **Bandwidth** | Limited (mesh capacity) | High (WiFi/Ethernet) |
| **Use case** | Local hub-to-hub | Cloud integration, Home Assistant |

**Recommendation**: Use Mesh Export for local inter-hub communication, MQTT Export for cloud/external integration.

## Future Enhancements

- **Message batching**: Group multiple variable updates in single mesh message
- **Compression**: Compress JSON payloads for large messages
- **QoS levels**: Guaranteed delivery for critical variables
- **Subscriptions patterns**: Wildcard subscriptions (e.g., `hub_*.main.alarm`)
- **Delta encoding**: Send only changed values for large data structures
- **Time synchronization**: Built-in timestamp validation
- **Mesh routing hints**: Prefer specific paths for critical variables

## Related Documentation

- [Variable Registry System](VARIABLE_REGISTRY.md)
- [MQTT Export System](MQTT_EXPORT_SYSTEM.md)
- [PLC Programming Guide](PLC_PROGRAMMING.md)
- [Mesh Network Configuration](MESH_NETWORK.md)

## Example Configuration Files

See [mesh_export_example.json](mesh_export_example.json) for comprehensive configuration examples and use cases.
