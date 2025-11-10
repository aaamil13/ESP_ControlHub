#include <Arduino.h>
#include <DeviceRegistry.h>
#include <PlcMemory.h>

// Simple test framework for embedded
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

void test_device_registry_singleton() {
    Serial.println("\n=== Test: DeviceRegistry Singleton ===");

    DeviceRegistry& registry1 = DeviceRegistry::getInstance();
    DeviceRegistry& registry2 = DeviceRegistry::getInstance();

    TEST_ASSERT(&registry1 == &registry2, "Singleton returns same instance");
}

void test_endpoint_registration() {
    Serial.println("\n=== Test: Endpoint Registration ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear(); // Start fresh

    // Create test endpoint
    Endpoint testEndpoint;
    testEndpoint.fullName = "kitchen.zigbee.relay.switch1.bool";
    testEndpoint.location = "kitchen";
    testEndpoint.protocol = ProtocolType::ZIGBEE;
    testEndpoint.deviceId = "relay";
    testEndpoint.endpoint = "switch1";
    testEndpoint.datatype = PlcValueType::BOOL;
    testEndpoint.isOnline = true;
    testEndpoint.lastSeen = millis();
    testEndpoint.isWritable = true;

    // Register endpoint
    bool registered = registry.registerEndpoint(testEndpoint);
    TEST_ASSERT(registered, "Endpoint registered successfully");

    // Retrieve endpoint
    Endpoint* retrieved = registry.getEndpoint("kitchen.zigbee.relay.switch1.bool");
    TEST_ASSERT(retrieved != nullptr, "Endpoint can be retrieved");
    TEST_ASSERT(retrieved->location == "kitchen", "Endpoint location correct");
    TEST_ASSERT(retrieved->protocol == ProtocolType::ZIGBEE, "Endpoint protocol correct");
    TEST_ASSERT(retrieved->isOnline == true, "Endpoint is online");

    // Try to register same endpoint again (should fail or update)
    bool registered_again = registry.registerEndpoint(testEndpoint);
    TEST_ASSERT(registered_again == false, "Duplicate endpoint registration fails");
}

void test_device_registration() {
    Serial.println("\n=== Test: Device Registration ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();

    // Create test device
    DeviceStatus testDevice;
    testDevice.deviceId = "kitchen.zigbee.relay";
    testDevice.protocol = ProtocolType::ZIGBEE;
    testDevice.isOnline = true;
    testDevice.lastSeen = millis();
    testDevice.offlineThreshold = 60000;

    // Register device
    bool registered = registry.registerDevice(testDevice);
    TEST_ASSERT(registered, "Device registered successfully");

    // Retrieve device
    DeviceStatus* retrieved = registry.getDevice("kitchen.zigbee.relay");
    TEST_ASSERT(retrieved != nullptr, "Device can be retrieved");
    TEST_ASSERT(retrieved->isOnline == true, "Device is online");
    TEST_ASSERT(retrieved->offlineThreshold == 60000, "Device offline threshold correct");
}

void test_status_updates() {
    Serial.println("\n=== Test: Status Updates ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();

    // Register endpoint
    Endpoint testEndpoint;
    testEndpoint.fullName = "garage.mesh.node1.gpio.bool";
    testEndpoint.location = "garage";
    testEndpoint.protocol = ProtocolType::MESH;
    testEndpoint.deviceId = "node1";
    testEndpoint.endpoint = "gpio";
    testEndpoint.datatype = PlcValueType::BOOL;
    testEndpoint.isOnline = true;
    testEndpoint.lastSeen = millis();

    registry.registerEndpoint(testEndpoint);

    // Update status to offline
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", false);

    // Check status changed
    Endpoint* updated = registry.getEndpoint("garage.mesh.node1.gpio.bool");
    TEST_ASSERT(updated->isOnline == false, "Endpoint status updated to offline");

    // Update back to online
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", true);
    updated = registry.getEndpoint("garage.mesh.node1.gpio.bool");
    TEST_ASSERT(updated->isOnline == true, "Endpoint status updated to online");
}

void test_endpoint_value_updates() {
    Serial.println("\n=== Test: Endpoint Value Updates ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();

    // Register endpoint
    Endpoint testEndpoint;
    testEndpoint.fullName = "bedroom.ble.temp_sensor.temperature.real";
    testEndpoint.location = "bedroom";
    testEndpoint.protocol = ProtocolType::BLE;
    testEndpoint.deviceId = "temp_sensor";
    testEndpoint.endpoint = "temperature";
    testEndpoint.datatype = PlcValueType::REAL;
    testEndpoint.isOnline = true;

    registry.registerEndpoint(testEndpoint);

    // Update value
    PlcValue newValue(PlcValueType::REAL);
    newValue.value.fVal = 23.5f;
    registry.updateEndpointValue("bedroom.ble.temp_sensor.temperature.real", newValue);

    // Check value updated
    Endpoint* updated = registry.getEndpoint("bedroom.ble.temp_sensor.temperature.real");
    TEST_ASSERT(updated != nullptr, "Endpoint exists");
    TEST_ASSERT(updated->currentValue.type == PlcValueType::REAL, "Value type correct");
    TEST_ASSERT(abs(updated->currentValue.value.fVal - 23.5f) < 0.01f, "Value updated correctly");
}

void test_io_point_registration() {
    Serial.println("\n=== Test: IO Point Registration ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();

    // Create IO point
    PlcIOPoint ioPoint;
    ioPoint.plcVarName = "gpio_input";
    ioPoint.mappedEndpoint = "garage.mesh.node1.gpio.bool";
    ioPoint.direction = IODirection::IO_INPUT;
    ioPoint.requiresFunction = false;
    ioPoint.autoSync = true;

    // Register IO point
    bool registered = registry.registerIOPoint(ioPoint);
    TEST_ASSERT(registered, "IO point registered successfully");

    // Retrieve IO point
    PlcIOPoint* retrieved = registry.getIOPoint("gpio_input");
    TEST_ASSERT(retrieved != nullptr, "IO point can be retrieved");
    TEST_ASSERT(retrieved->mappedEndpoint == "garage.mesh.node1.gpio.bool", "IO point endpoint correct");
    TEST_ASSERT(retrieved->direction == IODirection::IO_INPUT, "IO point direction correct");
    TEST_ASSERT(retrieved->autoSync == true, "IO point auto-sync enabled");

    // Unregister IO point
    bool unregistered = registry.unregisterIOPoint("gpio_input");
    TEST_ASSERT(unregistered, "IO point unregistered successfully");

    // Verify it's gone
    PlcIOPoint* after_unreg = registry.getIOPoint("gpio_input");
    TEST_ASSERT(after_unreg == nullptr, "IO point no longer exists after unregister");
}

void test_protocol_filtering() {
    Serial.println("\n=== Test: Protocol Filtering ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();

    // Register endpoints with different protocols
    Endpoint zigbee1;
    zigbee1.fullName = "kitchen.zigbee.relay.switch1.bool";
    zigbee1.protocol = ProtocolType::ZIGBEE;
    registry.registerEndpoint(zigbee1);

    Endpoint zigbee2;
    zigbee2.fullName = "bedroom.zigbee.temp.sensor.real";
    zigbee2.protocol = ProtocolType::ZIGBEE;
    registry.registerEndpoint(zigbee2);

    Endpoint mesh1;
    mesh1.fullName = "garage.mesh.node1.gpio.bool";
    mesh1.protocol = ProtocolType::MESH;
    registry.registerEndpoint(mesh1);

    // Get Zigbee endpoints
    auto zigbeeEndpoints = registry.getEndpointsByProtocol(ProtocolType::ZIGBEE);
    TEST_ASSERT(zigbeeEndpoints.size() == 2, "Found 2 Zigbee endpoints");

    // Get Mesh endpoints
    auto meshEndpoints = registry.getEndpointsByProtocol(ProtocolType::MESH);
    TEST_ASSERT(meshEndpoints.size() == 1, "Found 1 Mesh endpoint");

    // Get BLE endpoints (should be empty)
    auto bleEndpoints = registry.getEndpointsByProtocol(ProtocolType::BLE);
    TEST_ASSERT(bleEndpoints.size() == 0, "Found 0 BLE endpoints");
}

void test_location_filtering() {
    Serial.println("\n=== Test: Location Filtering ===");

    DeviceRegistry& registry = DeviceRegistry::getInstance();
    registry.clear();

    // Register endpoints in different locations
    Endpoint kitchen1;
    kitchen1.fullName = "kitchen.zigbee.relay.switch1.bool";
    kitchen1.location = "kitchen";
    registry.registerEndpoint(kitchen1);

    Endpoint kitchen2;
    kitchen2.fullName = "kitchen.wifi.light.dimmer.int";
    kitchen2.location = "kitchen";
    registry.registerEndpoint(kitchen2);

    Endpoint garage1;
    garage1.fullName = "garage.mesh.node1.gpio.bool";
    garage1.location = "garage";
    registry.registerEndpoint(garage1);

    // Get kitchen endpoints
    auto kitchenEndpoints = registry.getEndpointsByLocation("kitchen");
    TEST_ASSERT(kitchenEndpoints.size() == 2, "Found 2 kitchen endpoints");

    // Get garage endpoints
    auto garageEndpoints = registry.getEndpointsByLocation("garage");
    TEST_ASSERT(garageEndpoints.size() == 1, "Found 1 garage endpoint");
}

void run_all_tests() {
    Serial.println("\n");
    Serial.println("========================================");
    Serial.println("  DeviceRegistry Unit Tests");
    Serial.println("========================================");

    test_device_registry_singleton();
    test_endpoint_registration();
    test_device_registration();
    test_status_updates();
    test_endpoint_value_updates();
    test_io_point_registration();
    test_protocol_filtering();
    test_location_filtering();

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
    delay(2000); // Wait for serial monitor

    Serial.println("\nStarting DeviceRegistry tests...\n");
    run_all_tests();
}

void loop() {
    // Tests run once in setup()
    delay(1000);
}
