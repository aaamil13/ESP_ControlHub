# Changelog

All notable changes to EspHub will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-12

### Added - Zone Mesh Network
- **Custom zone-based mesh protocol** replacing painlessMesh
- **ZoneManager** - Zone membership and coordinator election
  - Automatic coordinator election based on RAM, uptime, CPU load, power source
  - Max 30 devices per zone (configurable)
  - Device discovery via beacon protocol
  - Zone statistics and monitoring
- **ZoneRouter** - Inter-zone routing protocol
  - Route discovery with hop-count metrics
  - Route timeout and auto-refresh (5 minutes)
  - Routing table (~50 bytes per route)
  - Support for 400+ devices across multiple zones
- **Binary protocol header** - Efficient packet format with checksum
- **Memory optimization** - ~1-2KB RAM per zone (vs 8KB for painlessMesh DHT)

### Added - Event-Driven System
- **IOEventManager** - Event-driven PLC trigger system
  - **I/O event triggers**: INPUT_CHANGED, INPUT_OFFLINE, INPUT_ONLINE, OUTPUT_ERROR, VALUE_THRESHOLD
  - **Scheduled triggers**: Time-based program execution (hour, minute, day, month)
  - **Event priorities**: NORMAL vs CRITICAL processing
  - **Event history**: 100 events circular buffer with MQTT export
  - **Debounce support**: Configurable debounce time per trigger
  - **Memory**: ~7.7KB RAM, ~5KB Flash
- **TimeManager.getCurrentTime()** - Added for scheduled trigger support

### Added - PLC Engine Improvements
- **Dynamic block structure** - Parse JSON once, execute many (performance optimization)
- **Memory validation** - Check available RAM before loading PLC programs
- **Standard I/O scan cycle** - READ → EXECUTE → WRITE phases
- **Output ownership tracking** - Prevents multiple programs from controlling same output
- **Export blocking** - PLC-controlled outputs cannot be exported to MQTT
- **PlcIOPoint.ownerProgram** field - Track which program owns each output

### Added - Documentation
- **docs/ZoneMesh_Guide.md** - Complete zone mesh documentation
  - Architecture overview and topology diagrams
  - API reference with examples
  - Configuration guide
  - Performance metrics and memory usage
  - Troubleshooting section
  - Migration guide from painlessMesh
- **docs/IOEventManager_Guide.md** - Event-driven system guide
  - Event types and triggers
  - JSON configuration format
  - Usage examples (temperature alarm, scheduled routines, etc.)
  - Performance and limitations
  - API reference
- **README.md** - Completely rewritten
  - Feature overview
  - Architecture diagrams
  - Quick start guide
  - Configuration examples
  - Performance benchmarks
- **data/config/events_example.json** - Example event configuration

### Changed - MeshDeviceManager
- **Breaking**: `begin()` now requires `deviceName` and `zoneName` parameters
- Integrated with ZoneManager and ZoneRouter
- Added zone-aware device management API
- Added subscription management API
- Maintained backward compatibility with legacy painlessMesh API
- Auto-generates device name from MAC address in EspHub.begin()

### Changed - EspHub Core
- `EspHub::begin()` now initializes zone mesh automatically
- Device name: `esphub_XXXXXX` (from MAC address last 3 bytes)
- Default zone: "main" (configurable)

### Performance
- **RAM usage**: 18.3% (59,952 / 327,680 bytes)
- **Flash usage**: 44.3% (1,392,177 / 3,145,728 bytes)
- **Zone mesh memory**: ~2KB per zone (vs 8KB for painlessMesh)
- **Max devices**: 400+ (vs ~50 for painlessMesh)
- **Event checking**: <1ms for all triggers
- **Route discovery**: <2s for multi-zone networks

### Fixed
- PlcValue field access in MeshDeviceManager (use `value.bVal`, `value.fVal`, etc.)
- TimeManager missing `getCurrentTime()` method

## [0.9.0] - 2024-12-XX (Previous releases not documented)

### Features (Existing before v1.0.0)
- PLC Engine with 50+ function blocks
- MQTT integration with Home Assistant discovery
- Zigbee device support
- WiFi device support
- RF433 device support
- Web interface for configuration
- OTA firmware updates
- User management
- Device configuration manager
- Variable registry
- MQTT/Mesh export managers
- painlessMesh networking (deprecated in v1.0.0)

---

## Migration Guide: painlessMesh → Zone Mesh

### Breaking Changes

**MeshDeviceManager.begin()**
```cpp
// Before (v0.9.0)
meshDeviceManager.begin();

// After (v1.0.0)
meshDeviceManager.begin("my.device", "kitchen"); // device name, zone name
```

### Backward Compatibility

Legacy painlessMesh API still works:
```cpp
meshDeviceManager.addDevice(nodeId, "name");
meshDeviceManager.updateDeviceLastSeen(nodeId);
MeshDevice* dev = meshDeviceManager.getDevice(nodeId);
```

But new zone mesh API is recommended:
```cpp
meshDeviceManager.subscribeToEndpoint("kitchen.temp.value.real", "my.device");
meshDeviceManager.publishToSubscribers("kitchen.temp.value.real", value);
```

---

## Roadmap

### v1.1.0 (Planned)
- [ ] Web UI for zone mesh configuration
- [ ] REST API for zone management
- [ ] Persistent subscription storage (NVS)
- [ ] Advanced routing algorithms (shortest path, load balancing)
- [ ] Zone mesh encryption (ESP-NOW encrypted mode)

### v1.2.0 (Planned)
- [ ] Zone merging/splitting support
- [ ] Inter-coordinator direct links (mesh-of-meshes)
- [ ] PLC debugging tools (breakpoints, variable watch)
- [ ] Event history persistent storage

### v2.0.0 (Future)
- [ ] Cloud synchronization
- [ ] Mobile app
- [ ] Advanced analytics
- [ ] Machine learning integration

---

[1.0.0]: https://github.com/yourusername/esphub/releases/tag/v1.0.0
[0.9.0]: https://github.com/yourusername/esphub/releases/tag/v0.9.0
