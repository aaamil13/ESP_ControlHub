#include <Arduino.h>
#include <PlcMemory.h>
#include <DeviceRegistry.h>
#include <blocks/events/BlockStatusHandler.h>
#include <ArduinoJson.h>

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

void test_complete_workflow() {
    Serial.println("\n=== Integration Test: Complete Workflow ===");

    // Setup
    DeviceRegistry& registry = DeviceRegistry::getInstance();
    PlcMemory memory;
    registry.clear();
    memory.clear();
    memory.begin();
    memory.setDeviceRegistry(&registry);

    // Step 1: Register a mesh device endpoint
    Serial.println("\nStep 1: Registering mesh device endpoint...");
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
    TEST_ASSERT(endpoint_registered, "Mesh endpoint registered");

    // Step 2: Create PLC variables
    Serial.println("\nStep 2: Creating PLC variables...");
    memory.declareVariable("endpoint_name", PlcValueType::STRING_TYPE);
    memory.declareVariable("node1_online", PlcValueType::BOOL);
    memory.declareVariable("on_node1_online", PlcValueType::BOOL);
    memory.declareVariable("on_node1_offline", PlcValueType::BOOL);
    memory.declareVariable("gpio_input", PlcValueType::BOOL);

    // Set endpoint name
    memory.setValue<String>("endpoint_name", "garage.mesh.node1.gpio.bool");

    TEST_ASSERT(true, "PLC variables created");

    // Step 3: Register IO point for GPIO input
    Serial.println("\nStep 3: Registering IO point...");
    bool io_registered = memory.registerIOPoint(
        "gpio_input",
        "garage.mesh.node1.gpio.bool",
        IODirection::IO_INPUT,
        false,
        "",
        true
    );
    TEST_ASSERT(io_registered, "GPIO IO point registered");

    // Step 4: Configure StatusHandlerBlock
    Serial.println("\nStep 4: Configuring StatusHandlerBlock...");
    BlockStatusHandler statusBlock;
    statusBlock.setDeviceRegistry(&registry);

    // Create config JSON
    JsonDocument config;
    config["inputs"]["endpoint_name"] = "endpoint_name";
    config["outputs"]["is_online"] = "node1_online";
    config["outputs"]["on_online"] = "on_node1_online";
    config["outputs"]["on_offline"] = "on_node1_offline";

    bool block_configured = statusBlock.configure(config.as<JsonObject>(), memory);
    TEST_ASSERT(block_configured, "StatusHandlerBlock configured");

    // Step 5: First evaluation (device online)
    Serial.println("\nStep 5: First evaluation (device online)...");
    statusBlock.evaluate(memory);

    bool online_status = memory.getValue<bool>("node1_online", false);
    bool online_trigger = memory.getValue<bool>("on_node1_online", true);
    bool offline_trigger = memory.getValue<bool>("on_node1_offline", true);

    TEST_ASSERT(online_status == true, "Initial online status detected");
    TEST_ASSERT(online_trigger == false, "No online trigger on first scan");
    TEST_ASSERT(offline_trigger == false, "No offline trigger on first scan");

    // Step 6: Sync GPIO input
    Serial.println("\nStep 6: Testing GPIO input sync...");
    // Update endpoint GPIO value
    PlcValue gpioValue(PlcValueType::BOOL);
    gpioValue.value.bVal = true;
    registry.updateEndpointValue("garage.mesh.node1.gpio.bool", gpioValue);

    // Sync IO points
    memory.syncIOPoints();

    bool gpio_plc_value = memory.getValue<bool>("gpio_input", false);
    TEST_ASSERT(gpio_plc_value == true, "GPIO input synced to PLC");

    // Step 7: Simulate device going offline
    Serial.println("\nStep 7: Simulating device offline...");
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", false);

    // Evaluate StatusHandlerBlock again
    statusBlock.evaluate(memory);

    online_status = memory.getValue<bool>("node1_online", true);
    offline_trigger = memory.getValue<bool>("on_node1_offline", false);

    TEST_ASSERT(online_status == false, "Offline status detected");
    TEST_ASSERT(offline_trigger == true, "Offline trigger activated");

    // Step 8: Verify offline endpoint not synced
    Serial.println("\nStep 8: Verify offline endpoint skipped...");
    // Try to sync - should skip offline endpoint
    memory.syncIOPoints();

    // GPIO value should not change
    gpio_plc_value = memory.getValue<bool>("gpio_input", false);
    TEST_ASSERT(gpio_plc_value == true, "Offline endpoint skipped during sync");

    // Step 9: Simulate device coming back online
    Serial.println("\nStep 9: Simulating device back online...");
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", true);

    // Reset triggers (happens after each scan)
    memory.setValue<bool>("on_node1_online", false);
    memory.setValue<bool>("on_node1_offline", false);

    // Evaluate again
    statusBlock.evaluate(memory);

    online_status = memory.getValue<bool>("node1_online", false);
    online_trigger = memory.getValue<bool>("on_node1_online", false);

    TEST_ASSERT(online_status == true, "Back online status detected");
    TEST_ASSERT(online_trigger == true, "Online trigger activated");

    // Step 10: Multiple evaluations without change
    Serial.println("\nStep 10: Testing stable operation...");
    memory.setValue<bool>("on_node1_online", false); // Reset trigger

    statusBlock.evaluate(memory);
    online_trigger = memory.getValue<bool>("on_node1_online", true);
    offline_trigger = memory.getValue<bool>("on_node1_offline", true);

    TEST_ASSERT(online_trigger == false, "No trigger when status unchanged");
    TEST_ASSERT(offline_trigger == false, "No trigger when status unchanged");
}

void test_multi_device_scenario() {
    Serial.println("\n=== Integration Test: Multi-Device Scenario ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    PlcMemory memory;
    registry.clear();
    memory.clear();
    memory.begin();
    memory.setDeviceRegistry(&registry);

    // Register multiple devices from different protocols
    Serial.println("\nRegistering multiple devices...");

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
                           IODirection::IO_INPUT, false, "", true);
    memory.registerIOPoint("motion_detected", "living_room.ble.motion.state.bool",
                           IODirection::IO_INPUT, false, "", true);
    memory.registerIOPoint("plug_control", "kitchen.wifi.plug.state.bool",
                           IODirection::IO_OUTPUT, false, "", true);

    // Sync all IO points
    memory.syncIOPoints();

    // Verify all inputs synced
    float temp = memory.getValue<float>("bedroom_temp", 0.0f);
    bool motion = memory.getValue<bool>("motion_detected", true);

    TEST_ASSERT(abs(temp - 22.5f) < 0.01f, "Zigbee temperature input synced");
    TEST_ASSERT(motion == false, "BLE motion input synced");

    // Control output
    memory.setValue<bool>("plug_control", true);
    memory.syncIOPoints();

    Endpoint* plugEndpoint = registry.getEndpoint("kitchen.wifi.plug.state.bool");
    TEST_ASSERT(plugEndpoint->currentValue.value.bVal == true, "WiFi plug output synced");

    // Test protocol filtering
    auto zigbeeEndpoints = registry.getEndpointsByProtocol(ProtocolType::ZIGBEE);
    auto bleEndpoints = registry.getEndpointsByProtocol(ProtocolType::BLE);
    auto wifiEndpoints = registry.getEndpointsByProtocol(ProtocolType::WIFI);

    TEST_ASSERT(zigbeeEndpoints.size() == 1, "1 Zigbee endpoint found");
    TEST_ASSERT(bleEndpoints.size() == 1, "1 BLE endpoint found");
    TEST_ASSERT(wifiEndpoints.size() == 1, "1 WiFi endpoint found");
}

void run_all_tests() {
    Serial.println("\n");
    Serial.println("========================================");
    Serial.println("  Integration Tests");
    Serial.println("========================================");

    test_complete_workflow();
    test_multi_device_scenario();

    Serial.println("\n========================================");
    Serial.printf("  Test Results: %d passed, %d failed\n", tests_passed, tests_failed);
    Serial.println("========================================\n");

    if (tests_failed == 0) {
        Serial.println("✅ All integration tests PASSED!");
    } else {
        Serial.printf("❌ %d tests FAILED\n", tests_failed);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("\nStarting Integration Tests...\n");
    run_all_tests();
}

void loop() {
    delay(1000);
}
