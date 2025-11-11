# Code Restructuring Plan

## Current Structure Analysis

### lib/EspHubLib/ (43 files)
**Core:**
- EspHubLib.cpp/h - Main library
- StreamLogger.cpp/h - Logging system
- TimeManager.cpp/h - Time management
- Logger.h - Logger interface

**Protocols:**
- MqttManager.cpp/h - MQTT protocol
- WiFiDeviceManager.cpp/h - WiFi devices
- RF433Manager.cpp/h - RF 433MHz devices
- ZigbeeManager.cpp/h - Zigbee devices
- MeshDeviceManager.cpp/h - Mesh device tracking
- mesh_protocol.cpp/h - Mesh protocol definitions
- ProtocolManagerInterface.h - Protocol interface

**Device Management:**
- DeviceConfigManager.cpp/h - Device configuration
- DeviceManager.cpp/h - Generic device manager
- DeviceRegistry.cpp/h - Device registry

**Export Systems:**
- VariableRegistry.cpp/h - Unified variable access
- MqttExportManager.cpp/h - MQTT variable export
- MeshExportManager.cpp/h - Mesh variable export
- MqttDiscoveryManager.cpp/h - Home Assistant discovery

**Storage/Config:**
- UserManager.cpp/h - User authentication
- OtaManager.cpp/h - OTA updates

**UI:**
- WebManager.cpp/h - Web server and UI

**Apps:**
- AppManager.cpp/h - Application manager
- apps/ - Application directory

### lib/PlcCore/ (7 files + blocks/)
- PlcEngine.cpp/h - PLC execution engine
- PlcMemory.cpp/h - PLC memory system
- PlcProgram.cpp/h - PLC program container
- blocks/ - PLC function blocks
  - comparison/ - Comparison blocks
  - conversion/ - Type conversion
  - counters/ - Counter blocks
  - events/ - Event handlers
  - logic/ - Logic gates
  - math/ - Math operations
  - scheduler/ - Scheduling
  - string/ - String operations
  - timers/ - Timer blocks
  - PlcBlock.h - Base block interface

## Proposed New Structure

```
lib/
├── Core/                           - Core system components
│   ├── EspHub.cpp
│   ├── EspHub.h
│   ├── StreamLogger.cpp
│   ├── StreamLogger.h
│   ├── Logger.h
│   ├── TimeManager.cpp
│   ├── TimeManager.h
│   └── library.json
│
├── Protocols/                      - Communication protocols
│   ├── Mqtt/
│   │   ├── MqttManager.cpp
│   │   └── MqttManager.h
│   ├── Mesh/
│   │   ├── MeshDeviceManager.cpp
│   │   ├── MeshDeviceManager.h
│   │   ├── mesh_protocol.cpp
│   │   └── mesh_protocol.h
│   ├── WiFi/
│   │   ├── WiFiDeviceManager.cpp
│   │   └── WiFiDeviceManager.h
│   ├── RF433/
│   │   ├── RF433Manager.cpp
│   │   └── RF433Manager.h
│   ├── Zigbee/
│   │   ├── ZigbeeManager.cpp
│   │   └── ZigbeeManager.h
│   ├── ProtocolManagerInterface.h
│   └── library.json
│
├── PlcEngine/                      - PLC system
│   ├── Engine/
│   │   ├── PlcEngine.cpp
│   │   ├── PlcEngine.h
│   │   ├── PlcMemory.cpp
│   │   ├── PlcMemory.h
│   │   ├── PlcProgram.cpp
│   │   └── PlcProgram.h
│   ├── Blocks/
│   │   ├── PlcBlock.h
│   │   ├── comparison/
│   │   ├── conversion/
│   │   ├── counters/
│   │   ├── events/
│   │   ├── logic/
│   │   ├── math/
│   │   ├── scheduler/
│   │   ├── string/
│   │   └── timers/
│   └── library.json
│
├── Export/                         - Variable export systems
│   ├── VariableRegistry.cpp
│   ├── VariableRegistry.h
│   ├── MqttExportManager.cpp
│   ├── MqttExportManager.h
│   ├── MeshExportManager.cpp
│   ├── MeshExportManager.h
│   ├── MqttDiscoveryManager.cpp
│   ├── MqttDiscoveryManager.h
│   └── library.json
│
├── Devices/                        - Device management
│   ├── DeviceConfigManager.cpp
│   ├── DeviceConfigManager.h
│   ├── DeviceManager.cpp
│   ├── DeviceManager.h
│   ├── DeviceRegistry.cpp
│   ├── DeviceRegistry.h
│   └── library.json
│
├── Storage/                        - Configuration & persistence
│   ├── UserManager.cpp
│   ├── UserManager.h
│   ├── OtaManager.cpp
│   ├── OtaManager.h
│   └── library.json
│
├── UI/                            - Web interface
│   ├── WebManager.cpp
│   ├── WebManager.h
│   └── library.json
│
└── Apps/                          - Application framework
    ├── AppManager.cpp
    ├── AppManager.h
    ├── apps/
    └── library.json
```

## Migration Steps

### Phase 1: Create New Directory Structure
1. Create all new directories
2. Create library.json for each module
3. Keep originals intact during migration

### Phase 2: Move Core Files
1. Move EspHubLib.cpp/h → Core/EspHub.cpp/h
2. Move StreamLogger.cpp/h → Core/
3. Move Logger.h → Core/
4. Move TimeManager.cpp/h → Core/

### Phase 3: Move Protocol Files
1. Create Protocols/Mqtt/ and move MqttManager files
2. Create Protocols/Mesh/ and move MeshDeviceManager + mesh_protocol files
3. Create Protocols/WiFi/ and move WiFiDeviceManager files
4. Create Protocols/RF433/ and move RF433Manager files
5. Create Protocols/Zigbee/ and move ZigbeeManager files
6. Move ProtocolManagerInterface.h to Protocols/

### Phase 4: Move PlcEngine Files
1. Create PlcEngine/Engine/ and move core PLC files
2. Move blocks/ directory to PlcEngine/Blocks/

### Phase 5: Move Export System Files
1. Create Export/ directory
2. Move VariableRegistry files
3. Move MqttExportManager files
4. Move MeshExportManager files
5. Move MqttDiscoveryManager files

### Phase 6: Move Device Management Files
1. Create Devices/ directory
2. Move DeviceConfigManager files
3. Move DeviceManager files
4. Move DeviceRegistry files

### Phase 7: Move Storage Files
1. Create Storage/ directory
2. Move UserManager files
3. Move OtaManager files

### Phase 8: Move UI Files
1. Create UI/ directory
2. Move WebManager files

### Phase 9: Move Apps Files
1. Create Apps/ directory
2. Move AppManager files
3. Move apps/ subdirectory

### Phase 10: Update Include Paths
1. Update all #include statements in .cpp/.h files
2. Update EspHub.h master include file
3. Update any cross-module dependencies

### Phase 11: Update Build Configuration
1. Update platformio.ini library dependencies
2. Add build flags for new structure
3. Configure include directories

### Phase 12: Test & Validate
1. Compile and fix errors
2. Verify all modules link correctly
3. Test functionality

## Include Path Changes

### Old → New Mapping

```
EspHubLib.h                    → Core/EspHub.h
StreamLogger.h                 → Core/StreamLogger.h
Logger.h                       → Core/Logger.h
TimeManager.h                  → Core/TimeManager.h

MqttManager.h                  → Protocols/Mqtt/MqttManager.h
MeshDeviceManager.h            → Protocols/Mesh/MeshDeviceManager.h
mesh_protocol.h                → Protocols/Mesh/mesh_protocol.h
WiFiDeviceManager.h            → Protocols/WiFi/WiFiDeviceManager.h
RF433Manager.h                 → Protocols/RF433/RF433Manager.h
ZigbeeManager.h                → Protocols/Zigbee/ZigbeeManager.h
ProtocolManagerInterface.h     → Protocols/ProtocolManagerInterface.h

../PlcCore/PlcEngine.h         → PlcEngine/Engine/PlcEngine.h
../PlcCore/PlcMemory.h         → PlcEngine/Engine/PlcMemory.h
../PlcCore/PlcProgram.h        → PlcEngine/Engine/PlcProgram.h
../PlcCore/blocks/PlcBlock.h   → PlcEngine/Blocks/PlcBlock.h

VariableRegistry.h             → Export/VariableRegistry.h
MqttExportManager.h            → Export/MqttExportManager.h
MeshExportManager.h            → Export/MeshExportManager.h
MqttDiscoveryManager.h         → Export/MqttDiscoveryManager.h

DeviceConfigManager.h          → Devices/DeviceConfigManager.h
DeviceManager.h                → Devices/DeviceManager.h
DeviceRegistry.h               → Devices/DeviceRegistry.h

UserManager.h                  → Storage/UserManager.h
OtaManager.h                   → Storage/OtaManager.h

WebManager.h                   → UI/WebManager.h

AppManager.h                   → Apps/AppManager.h
```

## Benefits

1. **Clear Separation of Concerns** - Each directory has a single responsibility
2. **Better Discoverability** - Easy to find related files
3. **Modular Build** - Can build/link modules independently
4. **Easier Testing** - Test individual modules
5. **Better IDE Support** - IDEs work better with organized structure
6. **Future Extensibility** - Easy to add new protocols/features
7. **Documentation** - Structure itself documents architecture

## Risks & Mitigation

### Risk: Breaking existing includes
**Mitigation:**
- Keep backup of original structure
- Update includes systematically
- Test compilation after each phase

### Risk: Build system issues
**Mitigation:**
- Update platformio.ini carefully
- Use explicit include paths
- Test incremental builds

### Risk: Cross-module dependencies
**Mitigation:**
- Map all dependencies first
- Update related files together
- Use forward declarations where possible

## Timeline Estimate

- Phase 1-2: 10 minutes (structure + core)
- Phase 3-9: 30 minutes (move files)
- Phase 10: 45 minutes (update includes)
- Phase 11: 15 minutes (build config)
- Phase 12: 30 minutes (testing)

**Total: ~2.5 hours**

## Success Criteria

- ✅ All files in new structure
- ✅ Project compiles without errors
- ✅ All tests pass
- ✅ Documentation updated
- ✅ No broken includes
- ✅ Clean git history
