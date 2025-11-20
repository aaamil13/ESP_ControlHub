# EspHub Unit Tests

## Test Files

1. **test_device_registry.cpp** - Tests for DeviceRegistry
   - Singleton pattern
   - Endpoint registration and retrieval
   - Device registration
   - Status updates
   - Value updates
   - IO point management
   - Protocol and location filtering

2. **test_plc_io_points.cpp** - Tests for PlcMemory IO Points
   - Basic PlcMemory operations
   - IO point registration via PlcMemory
   - INPUT synchronization (endpoint → PLC)
   - OUTPUT synchronization (PLC → endpoint)
   - Function-protected outputs
   - Offline endpoint handling
   - Endpoint online status check

3. **test_integration.cpp** - Integration tests
   - Complete workflow with StatusHandlerBlock
   - Multi-device scenario (Zigbee, BLE, WiFi)
   - End-to-end data flow
   - Event triggers on status changes

## Running Tests

### Using PlatformIO

To compile and run a specific test:

```bash
# Test DeviceRegistry
pio test -f test_device_registry

# Test PlcMemory IO Points
pio test -f test_plc_io_points

# Test Integration
pio test -f test_integration

# Run all tests
pio test
```

### Manual Upload

Alternatively, rename the test file to `main.cpp` temporarily:

```bash
# Backup current main
mv src/main.cpp src/main.cpp.bak

# Copy test to main
cp test/test_device_registry.cpp src/main.cpp

# Build and upload
platformio run --target upload

# Monitor serial output
platformio device monitor
```

## Expected Output

Each test will print:
- Individual test results: `[PASS]` or `[FAIL]`
- Final summary with counts
- Overall result: ✅ or ❌

Example:
```
========================================
  DeviceRegistry Unit Tests
========================================

=== Test: DeviceRegistry Singleton ===
[PASS] Singleton returns same instance

=== Test: Endpoint Registration ===
[PASS] Endpoint registered successfully
[PASS] Endpoint can be retrieved
[PASS] Endpoint location correct
...

========================================
  Test Results: 25 passed, 0 failed
========================================

✅ All tests PASSED!
```

## Test Strategy

- **Unit Tests**: Test individual components in isolation
- **Integration Tests**: Test components working together
- **Embedded Tests**: Run on actual ESP32 hardware (not mocked)

## Adding New Tests

1. Create new file in `test/` directory
2. Follow the TEST_ASSERT macro pattern
3. Include `run_all_tests()` in setup()
4. Keep loop() empty or minimal

## Notes

- Tests are designed for ESP32 embedded environment
- Serial monitor at 115200 baud
- Each test resets the registry/memory for clean state
- Tests can run on hardware or in native simulator
