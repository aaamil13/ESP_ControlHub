#include <Arduino.h>
#include "../../lib/PlcEngine/Engine/PlcMemory.h"
#include "../../lib/Devices/DeviceRegistry.h"
#include "../../lib/LocalIO/LocalIOTypes.h" // For IODirection
#include <unity.h>

// Global instance of DeviceRegistry and PlcMemory for testing
// Note: PlcMemory needs a DeviceRegistry instance to function with IO points
DeviceRegistry& registry = DeviceRegistry::getInstance();
PlcMemory memory; // Global instance for tests

void setUp(void) {
    // set up runs before each test
    registry.clear(); // Clear registry before each test
    memory.clear();   // Clear PLC memory before each test
    memory.begin();   // Initialize PlcMemory
    memory.setDeviceRegistry(&registry); // Connect PlcMemory to DeviceRegistry
}

void tearDown(void) {
    // tear down runs after each test
}

void test_plc_memory_basic() {
    // Declare variables
    bool declared1 = memory.declareVariable("test_bool", PlcValueType::BOOL);
    bool declared2 = memory.declareVariable("test_int", PlcValueType::INT);
    bool declared3 = memory.declareVariable("test_real", PlcValueType::REAL);

    TEST_ASSERT_TRUE(declared1);
    TEST_ASSERT_TRUE(declared2);
    TEST_ASSERT_TRUE(declared3);

    // Set values
    memory.setValue<bool>("test_bool", true);
    memory.setValue<int16_t>("test_int", 42);
    memory.setValue<float>("test_real", 3.14f);

    // Get values
    bool boolVal = memory.getValue<bool>("test_bool", false);
    int16_t intVal = memory.getValue<int16_t>("test_int", 0);
    float realVal = memory.getValue<float>("test_real", 0.0f);

    TEST_ASSERT_TRUE(boolVal);
    TEST_ASSERT_EQUAL_INT16(42, intVal);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, realVal);
}

void test_io_point_registration() {
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
        "test_program", // Owner program
        false,
        "",
        true
    );

    TEST_ASSERT_TRUE(registered);

    // Verify IO point exists
    PlcIOPoint* ioPoint = registry.getIOPoint("input_gpio"); // Access from registry
    TEST_ASSERT_NOT_NULL(ioPoint);
    TEST_ASSERT_EQUAL(IODirection::IO_INPUT, ioPoint->direction);
}

void test_input_sync() {
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
        "test_program",
        false,
        "",
        true
    );

    // Sync IO points (should copy endpoint value to PLC)
    memory.syncIOPoints();

    // Check PLC variable was updated from endpoint
    bool plcValue = memory.getValue<bool>("sensor_value", false);
    TEST_ASSERT_TRUE(plcValue);

    // Change endpoint value to false
    PlcValue newValue(PlcValueType::BOOL);
    newValue.value.bVal = false;
    registry.updateEndpointValue("bedroom.ble.motion.state.bool", newValue);

    // Sync again
    memory.syncIOPoints();

    // Check PLC variable updated
    plcValue = memory.getValue<bool>("sensor_value", true);
    TEST_ASSERT_FALSE(plcValue);
}

void test_output_sync() {
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
        "test_program",
        false, // No function required
        "",
        true
    );

    // Sync IO points (should copy PLC value to endpoint)
    memory.syncIOPoints();

    // Check endpoint was updated from PLC
    Endpoint* updated = registry.getEndpoint("kitchen.wifi.relay.switch1.bool");
    TEST_ASSERT_NOT_NULL(updated);
    TEST_ASSERT_TRUE(updated->currentValue.value.bVal);
}

void test_function_protected_output() {
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
        "test_program",
        true, // Requires function
        "door_control_function",
        true
    );

    // Sync IO points - should NOT update because function is required
    memory.syncIOPoints();

    // Check endpoint was NOT updated (still false)
    Endpoint* updated = registry.getEndpoint("garage.wifi.door.relay.bool");
    TEST_ASSERT_NOT_NULL(updated);
    TEST_ASSERT_FALSE(updated->currentValue.value.bVal);
}

void test_offline_endpoint_skip() {
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
        "test_program",
        false,
        "",
        true
    );

    // Sync - should skip offline endpoint
    memory.syncIOPoints();

    // PLC value should NOT change (still false)
    bool plcValue = memory.getValue<bool>("offline_input", true);
    TEST_ASSERT_FALSE(plcValue);
}

void test_endpoint_online_check() {
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

    TEST_ASSERT_TRUE(online1);
    TEST_ASSERT_FALSE(online2);
    TEST_ASSERT_FALSE(online3);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_plc_memory_basic);
    RUN_TEST(test_io_point_registration);
    RUN_TEST(test_input_sync);
    RUN_TEST(test_output_sync);
    RUN_TEST(test_function_protected_output);
    RUN_TEST(test_offline_endpoint_skip);
    RUN_TEST(test_endpoint_online_check);
    UNITY_END();
    return 0;
}
