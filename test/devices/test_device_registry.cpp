/ru#include <Arduino.h>
#include "../../lib/Devices/DeviceRegistry.h"
#include "../../lib/PlcEngine/Engine/PlcMemory.h"
#include "../../lib/LocalIO/LocalIOTypes.h" // For IODirection
#include <unity.h> // Include Unity framework

// Global instance of DeviceRegistry for testing
DeviceRegistry& registry = DeviceRegistry::getInstance();

void setUp(void) {
    // set up runs before each test
    registry.clear(); // Clear registry before each test
}

void tearDown(void) {
    // tear down runs after each test
}

void test_device_registry_singleton() {
    TEST_MESSAGE("\n=== Test: DeviceRegistry Singleton ===");
    DeviceRegistry& registry1 = DeviceRegistry::getInstance();
    DeviceRegistry& registry2 = DeviceRegistry::getInstance();
    TEST_ASSERT_EQUAL_PTR(&registry1, &registry2);
}

void test_endpoint_registration() {
    TEST_MESSAGE("\n=== Test: Endpoint Registration ===");
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
    TEST_ASSERT_TRUE(registered);

    // Retrieve endpoint
    Endpoint* retrieved = registry.getEndpoint("kitchen.zigbee.relay.switch1.bool");
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_STRING("kitchen", retrieved->location.c_str());
    TEST_ASSERT_EQUAL(ProtocolType::ZIGBEE, retrieved->protocol);
    TEST_ASSERT_TRUE(retrieved->isOnline);

    // Try to register same endpoint again (should fail or update)
    bool registered_again = registry.registerEndpoint(testEndpoint);
    TEST_ASSERT_FALSE(registered_again);
}

void test_device_registration() {
    TEST_MESSAGE("\n=== Test: Device Registration ===");
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
    TEST_ASSERT_TRUE(registered);

    // Retrieve device
    DeviceStatus* retrieved = registry.getDevice("kitchen.zigbee.relay");
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_TRUE(retrieved->isOnline);
    TEST_ASSERT_EQUAL_UINT32(60000, retrieved->offlineThreshold);
}

void test_status_updates() {
    TEST_MESSAGE("\n=== Test: Status Updates ===");
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
    TEST_ASSERT_FALSE(updated->isOnline);

    // Update back to online
    registry.updateEndpointStatus("garage.mesh.node1.gpio.bool", true);
    updated = registry.getEndpoint("garage.mesh.node1.gpio.bool");
    TEST_ASSERT_TRUE(updated->isOnline);
}

void test_endpoint_value_updates() {
    TEST_MESSAGE("\n=== Test: Endpoint Value Updates ===");
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
    TEST_ASSERT_NOT_NULL(updated);
    TEST_ASSERT_EQUAL(PlcValueType::REAL, updated->currentValue.type);
    TEST_ASSERT_EQUAL_FLOAT(23.5f, updated->currentValue.value.fVal);
}

void test_io_point_registration() {
    TEST_MESSAGE("\n=== Test: IO Point Registration ===");
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
    TEST_ASSERT_TRUE(registered);

    // Retrieve IO point
    PlcIOPoint* retrieved = registry.getIOPoint("gpio_input");
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_STRING("garage.mesh.node1.gpio.bool", retrieved->mappedEndpoint.c_str());
    TEST_ASSERT_EQUAL(IODirection::IO_INPUT, retrieved->direction);
    TEST_ASSERT_TRUE(retrieved->autoSync);

    // Unregister IO point
    bool unregistered = registry.unregisterIOPoint("gpio_input");
    TEST_ASSERT_TRUE(unregistered);

    // Verify it's gone
    PlcIOPoint* after_unreg = registry.getIOPoint("gpio_input");
    TEST_ASSERT_NULL(after_unreg);
}

void test_protocol_filtering() {
    TEST_MESSAGE("\n=== Test: Protocol Filtering ===");
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
    TEST_ASSERT_EQUAL_UINT(2, zigbeeEndpoints.size());

    // Get Mesh endpoints
    auto meshEndpoints = registry.getEndpointsByProtocol(ProtocolType::MESH);
    TEST_ASSERT_EQUAL_UINT(1, meshEndpoints.size());

    // Get BLE endpoints (should be empty)
    auto bleEndpoints = registry.getEndpointsByProtocol(ProtocolType::BLE);
    TEST_ASSERT_EQUAL_UINT(0, bleEndpoints.size());
}

void test_location_filtering() {
    TEST_MESSAGE("\n=== Test: Location Filtering ===");
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
    TEST_ASSERT_EQUAL_UINT(2, kitchenEndpoints.size());

    // Get garage endpoints
    auto garageEndpoints = registry.getEndpointsByLocation("garage");
    TEST_ASSERT_EQUAL_UINT(1, garageEndpoints.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_device_registry_singleton);
    RUN_TEST(test_endpoint_registration);
    RUN_TEST(test_device_registration);
    RUN_TEST(test_status_updates);
    RUN_TEST(test_endpoint_value_updates);
    RUN_TEST(test_io_point_registration);
    RUN_TEST(test_protocol_filtering);
    RUN_TEST(test_location_filtering);
    UNITY_END();
    return 0;
}
