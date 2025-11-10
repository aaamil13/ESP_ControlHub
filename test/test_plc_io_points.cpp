#include <Arduino.h>
#include <PlcMemory.h>
#include <DeviceRegistry.h>

// Simple test framework
int tests_passed = 0;
int tests_failed = 0;

#define TEST_ASSERT(condition, message) \
    if (condition) { \
        tests_passed++; \
        Serial.printf("[PASS] %s\n", message); \
    } else { \
        tests_failed++; \
        Serial.printf("[FAIL] %s\n", message); \
    }

void test_plc_memory_basic() {
    Serial.println("\n=== Test: PlcMemory Basic Operations ===");

    PlcMemory memory;
    memory.begin();

    // Declare variables
    bool declared1 = memory.declareVariable("test_bool", PlcValueType::BOOL);
    bool declared2 = memory.declareVariable("test_int", PlcValueType::INT);
    bool declared3 = memory.declareVariable("test_real", PlcValueType::REAL);

    TEST_ASSERT(declared1, "BOOL variable declared");
    TEST_ASSERT(declared2, "INT variable declared");
    TEST_ASSERT(declared3, "REAL variable declared");

    // Set values
    memory.setValue<bool>("test_bool", true);
    memory.setValue<int16_t>("test_int", 42);
    memory.setValue<float>("test_real", 3.14f);

    // Get values
    bool boolVal = memory.getValue<bool>("test_bool", false);
    int16_t intVal = memory.getValue<int16_t>("test_int", 0);
    float realVal = memory.getValue<float>("test_real", 0.0f);

    TEST_ASSERT(boolVal == true, "BOOL value correct");
    TEST_ASSERT(intVal == 42, "INT value correct");
    TEST_ASSERT(abs(realVal - 3.14f) < 0.01f, "REAL value correct");
}

void test_io_point_registration() {
    Serial.println("\n=== Test: IO Point Registration via PlcMemory ===");

    PlcMemory memory;
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();
    memory.clear();
    memory.begin();

    // Connect PlcMemory to DeviceRegistry
    memory.setDeviceRegistry(&registry);

    // Declare PLC variable
    memory.declareVariable("input_gpio", PlcValueType::BOOL);

    // Register endpoint in DeviceRegistry
    Endpoint testEndpoint;
    testEndpoint.fullName = "garage.mesh.node1.gpio.bool";
    testEndpoint.protocol = ProtocolType::MESH;
    testEndpoint.datatype = PlcValueType::BOOL;
    testEndpoint.isOnline = true;
    testEndpoint.currentValue.type = PlcValueType::BOOL;
    testEndpoint.currentValue.value.bVal = false;
    registry.registerEndpoint(testEndpoint);

    // Register IO point
    bool registered = memory.registerIOPoint(
        "input_gpio",
        "garage.mesh.node1.gpio.bool",
        IODirection::IO_INPUT,
        false,
        "",
        true
    );

    TEST_ASSERT(registered, "IO point registered via PlcMemory");

    // Verify IO point exists
    PlcIOPoint* ioPoint = memory.getIOPoint("input_gpio");
    TEST_ASSERT(ioPoint != nullptr, "IO point can be retrieved");
    TEST_ASSERT(ioPoint->direction == IODirection::IO_INPUT, "IO point direction correct");
}

void test_input_sync() {
    Serial.println("\n=== Test: INPUT Synchronization ===");

    PlcMemory memory;
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();
    memory.clear();
    memory.begin();

    memory.setDeviceRegistry(&registry);

    // Declare PLC variable for input
    memory.declareVariable("sensor_value", PlcValueType::BOOL);
    memory.setValue<bool>("sensor_value", false); // Initial value

    // Register endpoint
    Endpoint sensorEndpoint;
    sensorEndpoint.fullName = "bedroom.ble.motion.state.bool";
    sensorEndpoint.protocol = ProtocolType::BLE;
    sensorEndpoint.datatype = PlcValueType::BOOL;
    sensorEndpoint.isOnline = true;
    sensorEndpoint.currentValue.type = PlcValueType::BOOL;
    sensorEndpoint.currentValue.value.bVal = true; // Endpoint has true value
    registry.registerEndpoint(sensorEndpoint);

    // Register IO point as INPUT
    memory.registerIOPoint(
        "sensor_value",
        "bedroom.ble.motion.state.bool",
        IODirection::IO_INPUT,
        false,
        "",
        true
    );

    // Sync IO points (should copy endpoint value to PLC)
    memory.syncIOPoints();

    // Check PLC variable was updated from endpoint
    bool plcValue = memory.getValue<bool>("sensor_value", false);
    TEST_ASSERT(plcValue == true, "INPUT synced from endpoint to PLC");

    // Change endpoint value to false
    PlcValue newValue(PlcValueType::BOOL);
    newValue.value.bVal = false;
    registry.updateEndpointValue("bedroom.ble.motion.state.bool", newValue);

    // Sync again
    memory.syncIOPoints();

    // Check PLC variable updated
    plcValue = memory.getValue<bool>("sensor_value", true);
    TEST_ASSERT(plcValue == false, "INPUT synced after endpoint change");
}

void test_output_sync() {
    Serial.println("\n=== Test: OUTPUT Synchronization ===");

    PlcMemory memory;
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();
    memory.clear();
    memory.begin();

    memory.setDeviceRegistry(&registry);

    // Declare PLC variable for output
    memory.declareVariable("relay_state", PlcValueType::BOOL);
    memory.setValue<bool>("relay_state", true); // Set to true

    // Register endpoint
    Endpoint relayEndpoint;
    relayEndpoint.fullName = "kitchen.wifi.relay.switch1.bool";
    relayEndpoint.protocol = ProtocolType::WIFI;
    relayEndpoint.datatype = PlcValueType::BOOL;
    relayEndpoint.isOnline = true;
    relayEndpoint.isWritable = true;
    relayEndpoint.currentValue.type = PlcValueType::BOOL;
    relayEndpoint.currentValue.value.bVal = false; // Initial endpoint value
    registry.registerEndpoint(relayEndpoint);

    // Register IO point as OUTPUT (without function protection)
    memory.registerIOPoint(
        "relay_state",
        "kitchen.wifi.relay.switch1.bool",
        IODirection::IO_OUTPUT,
        false, // No function required
        "",
        true
    );

    // Sync IO points (should copy PLC value to endpoint)
    memory.syncIOPoints();

    // Check endpoint was updated from PLC
    Endpoint* updated = registry.getEndpoint("kitchen.wifi.relay.switch1.bool");
    TEST_ASSERT(updated != nullptr, "Endpoint exists");
    TEST_ASSERT(updated->currentValue.value.bVal == true, "OUTPUT synced from PLC to endpoint");
}

void test_function_protected_output() {
    Serial.println("\n=== Test: Function-Protected OUTPUT ===");

    PlcMemory memory;
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();
    memory.clear();
    memory.begin();

    memory.setDeviceRegistry(&registry);

    // Declare PLC variable for protected output
    memory.declareVariable("critical_relay", PlcValueType::BOOL);
    memory.setValue<bool>("critical_relay", true);

    // Register endpoint
    Endpoint criticalEndpoint;
    criticalEndpoint.fullName = "garage.wifi.door.relay.bool";
    criticalEndpoint.protocol = ProtocolType::WIFI;
    criticalEndpoint.datatype = PlcValueType::BOOL;
    criticalEndpoint.isOnline = true;
    criticalEndpoint.isWritable = true;
    criticalEndpoint.currentValue.type = PlcValueType::BOOL;
    criticalEndpoint.currentValue.value.bVal = false;
    registry.registerEndpoint(criticalEndpoint);

    // Register IO point as OUTPUT with function protection
    memory.registerIOPoint(
        "critical_relay",
        "garage.wifi.door.relay.bool",
        IODirection::IO_OUTPUT,
        true, // Requires function
        "door_control_function",
        true
    );

    // Sync IO points - should NOT update because function is required
    memory.syncIOPoints();

    // Check endpoint was NOT updated (still false)
    Endpoint* updated = registry.getEndpoint("garage.wifi.door.relay.bool");
    TEST_ASSERT(updated != nullptr, "Endpoint exists");
    TEST_ASSERT(updated->currentValue.value.bVal == false, "Function-protected OUTPUT not auto-synced");
}

void test_offline_endpoint_skip() {
    Serial.println("\n=== Test: Skip Offline Endpoints ===");

    PlcMemory memory;
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();
    memory.clear();
    memory.begin();

    memory.setDeviceRegistry(&registry);

    // Declare PLC variable
    memory.declareVariable("offline_input", PlcValueType::BOOL);
    memory.setValue<bool>("offline_input", false);

    // Register OFFLINE endpoint
    Endpoint offlineEndpoint;
    offlineEndpoint.fullName = "outdoor.zigbee.sensor.temp.real";
    offlineEndpoint.protocol = ProtocolType::ZIGBEE;
    offlineEndpoint.datatype = PlcValueType::BOOL;
    offlineEndpoint.isOnline = false; // OFFLINE
    offlineEndpoint.currentValue.type = PlcValueType::BOOL;
    offlineEndpoint.currentValue.value.bVal = true;
    registry.registerEndpoint(offlineEndpoint);

    // Register IO point
    memory.registerIOPoint(
        "offline_input",
        "outdoor.zigbee.sensor.temp.real",
        IODirection::IO_INPUT,
        false,
        "",
        true
    );

    // Sync - should skip offline endpoint
    memory.syncIOPoints();

    // PLC value should NOT change (still false)
    bool plcValue = memory.getValue<bool>("offline_input", true);
    TEST_ASSERT(plcValue == false, "Offline endpoint skipped during sync");
}

void test_endpoint_online_check() {
    Serial.println("\n=== Test: Endpoint Online Check ===");

    PlcMemory memory;
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();
    memory.clear();
    memory.begin();

    memory.setDeviceRegistry(&registry);

    // Register online endpoint
    Endpoint onlineEndpoint;
    onlineEndpoint.fullName = "kitchen.wifi.plug.state.bool";
    onlineEndpoint.protocol = ProtocolType::WIFI;
    onlineEndpoint.isOnline = true;
    registry.registerEndpoint(onlineEndpoint);

    // Register offline endpoint
    Endpoint offlineEndpoint;
    offlineEndpoint.fullName = "garage.mesh.sensor.state.bool";
    offlineEndpoint.protocol = ProtocolType::MESH;
    offlineEndpoint.isOnline = false;
    registry.registerEndpoint(offlineEndpoint);

    // Check online status
    bool online1 = memory.isEndpointOnline("kitchen.wifi.plug.state.bool");
    bool online2 = memory.isEndpointOnline("garage.mesh.sensor.state.bool");
    bool online3 = memory.isEndpointOnline("nonexistent.endpoint");

    TEST_ASSERT(online1 == true, "Online endpoint detected");
    TEST_ASSERT(online2 == false, "Offline endpoint detected");
    TEST_ASSERT(online3 == false, "Nonexistent endpoint returns false");
}

void run_all_tests() {
    Serial.println("\n");
    Serial.println("========================================");
    Serial.println("  PlcMemory IO Points Unit Tests");
    Serial.println("========================================");

    test_plc_memory_basic();
    test_io_point_registration();
    test_input_sync();
    test_output_sync();
    test_function_protected_output();
    test_offline_endpoint_skip();
    test_endpoint_online_check();

    Serial.println("\n========================================");
    Serial.printf("  Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    Serial.println("========================================\n");

    if (tests_failed == 0) {
        Serial.println("✅ All tests PASSED!");
    } else {
        Serial.printf("❌ %d tests FAILED\n", tests_failed);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\nStarting PlcMemory IO Points tests...\n");
    run_all_tests();
}

void loop() {
    delay(1000);
}
