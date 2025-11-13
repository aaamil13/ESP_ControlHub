#include <Arduino.h>
#include "../../lib/PlcEngine/Engine/PlcMemory.h"
#include "../../lib/Devices/DeviceRegistry.h"
#include "../../lib/PlcEngine/Blocks/events/BlockStatusHandler.h"
#include "../../lib/LocalIO/LocalIOTypes.h" // For IODirection
#include <ArduinoJson.h>
#include <unity.h>

// Global instance of DeviceRegistry and PlcMemory for testing
DeviceRegistry& registry = DeviceRegistry::getInstance();
PlcMemory memory;

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

void test_complete_workflow() {
    // Step 1: Register a mesh device endpoint
    Endpoint meshEndpoint;
    meshEndpoint.fullName = "garage.mesh.node1.gpio.bool";
    meshEndpoint.location = "garage";
    meshEndpoint.protocol = ProtocolType::MESH;
    meshEndpoint.deviceId = "node1";
    meshEndpoint.endpoint = "gpio";
    meshEndpoint.datatype = PlcValueType::BOOL;
    meshEndpoint.isOnline = true;
    meshEndpoint.lastSeen = millis();
    meshEndpoint.currentValue.type = PlcValueType::BOOL;
    meshEndpoint.currentValue.value.bVal = false;

    bool endpoint_registered = registry.registerEndpoint(meshEndpoint);
    TEST_ASSERT_TRUE(endpoint_registered);

    // Step 2: Create PLC variables
    memory.declareVariable("endpoint_name", PlcValueType::STRING_TYPE);
    memory.declareVariable("node1_online", PlcValueType::BOOL);
    memory.declareVariable("on_node1_online", PlcValueType::BOOL);
    memory.declareVariable("on_node1_offline", PlcValueType::BOOL);
    memory.declareVariable("gpio_input", PlcValueType::BOOL);

    // Set endpoint name
    memory.setValue<String>("endpoint_name", "garage.mesh.node1.gpio.bool");

    // TEST_ASSERT(true, "PLC variables created"); // No assertion needed for variable creation itself

    // Step 3: Register IO point for GPIO input
    bool io_registered = memory.registerIOPoint(
        "gpio_input",
        "garage.mesh.node1.gpio.bool",
        IODirection::IO_INPUT,
        "test_program", // Owner program
        false,
        "",
        true
    );
    TEST_ASSERT_TRUE(io_registered);

    // Step 4: Configure StatusHandlerBlock
    BlockStatusHandler statusBlock;
    statusBlock.setDeviceRegistry(&registry);

    // Create config JSON
    JsonDocument config;
    config["inputs"]["endpoint_name"] = "endpoint_name";
    config["outputs"]["is_online"] = "node1_online";
    config["outputs"]["on_online"] = "on_node1_online";
    config["outputs"]["on_offline"] = "on_node1_offline";

    bool block_configured = statusBlock.configure(config.as<JsonObject>(), memory);
    TEST_ASSERT_TRUE(block_configured);

    // Step 5: First evaluation (device online)
    statusBlock.evaluate(memory);

    bool online_status = memory.getValue<bool>("node1_online", false);
    bool online_trigger = memory.getValue<bool>("on_node1_online", true); // Default value should be false
    bool offline_trigger = memory.getValue<bool>("on_node1_offline", true); // Default value should be false

    TEST_ASSERT_TRUE(online_status);
    TEST_ASSERT_FALSE(online_trigger);
    TEST_ASSERT_FALSE(offline_trigger);

    // Step 6: Sync GPIO input
    // Update endpoint GPIO value
    PlcValue gpioValue(PlcValueType::BOOL);
    gpioValue.value.bVal = true;
    registry.updateEndpointValue("garage.mesh.node1.gpio.bool", gpioValue);

    // Sync IO points
    memory.syncIOPoints();

    bool gpio_plc_value = memory.getValue<bool>("gpio_input", false);
    TEST_ASSERT_TRUE(gpio_plc_value);

    // Step 7: Simulate device going offline
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", false);

    // Evaluate StatusHandlerBlock again
    statusBlock.evaluate(memory);

    online_status = memory.getValue<bool>("node1_online", true);
    offline_trigger = memory.getValue<bool>("on_node1_offline", false);

    TEST_ASSERT_FALSE(online_status);
    TEST_ASSERT_TRUE(offline_trigger);

    // Step 8: Verify offline endpoint not synced
    // Try to sync - should skip offline endpoint
    memory.syncIOPoints();

    // GPIO value should not change
    gpio_plc_value = memory.getValue<bool>("gpio_input", false);
    TEST_ASSERT_TRUE(gpio_plc_value); // Value should remain true as it was skipped

    // Step 9: Simulate device coming back online
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", true);

    // Reset triggers (happens after each scan in real PLC, manually here)
    memory.setValue<bool>("on_node1_online", false);
    memory.setValue<bool>("on_node1_offline", false);

    // Evaluate again
    statusBlock.evaluate(memory);

    online_status = memory.getValue<bool>("node1_online", false);
    online_trigger = memory.getValue<bool>("on_node1_online", false);

    TEST_ASSERT_TRUE(online_status);
    TEST_ASSERT_TRUE(online_trigger);

    // Step 10: Multiple evaluations without change
    memory.setValue<bool>("on_node1_online", false); // Reset trigger

    statusBlock.evaluate(memory);
    online_trigger = memory.getValue<bool>("on_node1_online", true);
    offline_trigger = memory.getValue<bool>("on_node1_offline", true);

    TEST_ASSERT_FALSE(online_trigger);
    TEST_ASSERT_FALSE(offline_trigger);
}

void test_multi_device_scenario() {
    // Register multiple devices from different protocols

    // Zigbee temperature sensor
    Endpoint zigbeeTemp;
    zigbeeTemp.fullName = "bedroom.zigbee.temp_sensor.temperature.real";
    zigbeeTemp.protocol = ProtocolType::ZIGBEE;
    zigbeeTemp.datatype = PlcValueType::REAL;
    zigbeeTemp.isOnline = true;
    zigbeeTemp.currentValue.type = PlcValueType::REAL;
    zigbeeTemp.currentValue.value.fVal = 22.5f;
    registry.registerEndpoint(zigbeeTemp);

    // BLE motion sensor
    Endpoint bleMotion;
    bleMotion.fullName = "living_room.ble.motion.state.bool";
    bleMotion.protocol = ProtocolType::BLE;
    bleMotion.datatype = PlcValueType::BOOL;
    bleMotion.isOnline = true;
    bleMotion.currentValue.type = PlcValueType::BOOL;
    bleMotion.currentValue.value.bVal = false;
    registry.registerEndpoint(bleMotion);

    // WiFi smart plug
    Endpoint wifiPlug;
    wifiPlug.fullName = "kitchen.wifi.plug.state.bool";
    wifiPlug.protocol = ProtocolType::WIFI;
    wifiPlug.datatype = PlcValueType::BOOL;
    wifiPlug.isOnline = true;
    wifiPlug.isWritable = true;
    wifiPlug.currentValue.type = PlcValueType::BOOL;
    wifiPlug.currentValue.value.bVal = false;
    registry.registerEndpoint(wifiPlug);

    // Declare PLC variables for each
    memory.declareVariable("bedroom_temp", PlcValueType::REAL);
    memory.declareVariable("motion_detected", PlcValueType::BOOL);
    memory.declareVariable("plug_control", PlcValueType::BOOL);

    // Register IO points
    memory.registerIOPoint("bedroom_temp", "bedroom.zigbee.temp_sensor.temperature.real",
                           IODirection::IO_INPUT, "test_program", false, "", true);
    memory.registerIOPoint("motion_detected", "living_room.ble.motion.state.bool",
                           IODirection::IO_INPUT, "test_program", false, "", true);
    memory.registerIOPoint("plug_control", "kitchen.wifi.plug.state.bool",
                           IODirection::IO_OUTPUT, "test_program", false, "", true);

    // Sync all IO points
    memory.syncIOPoints();

    // Verify all inputs synced
    float temp = memory.getValue<float>("bedroom_temp", 0.0f);
    bool motion = memory.getValue<bool>("motion_detected", true);

    TEST_ASSERT_EQUAL_FLOAT(22.5f, temp);
    TEST_ASSERT_FALSE(motion);

    // Control output
    memory.setValue<bool>("plug_control", true);
    memory.syncIOPoints();

    Endpoint* plugEndpoint = registry.getEndpoint("kitchen.wifi.plug.state.bool");
    TEST_ASSERT_NOT_NULL(plugEndpoint);
    TEST_ASSERT_TRUE(plugEndpoint->currentValue.value.bVal);

    // Test protocol filtering
    auto zigbeeEndpoints = registry.getEndpointsByProtocol(ProtocolType::ZIGBEE);
    auto bleEndpoints = registry.getEndpointsByProtocol(ProtocolType::BLE);
    auto wifiEndpoints = registry.getEndpointsByProtocol(ProtocolType::WIFI);

    TEST_ASSERT_EQUAL_UINT(1, zigbeeEndpoints.size());
    TEST_ASSERT_EQUAL_UINT(1, bleEndpoints.size());
    TEST_ASSERT_EQUAL_UINT(1, wifiEndpoints.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_complete_workflow);
    RUN_TEST(test_multi_device_scenario);
    UNITY_END();
    return 0;
}
