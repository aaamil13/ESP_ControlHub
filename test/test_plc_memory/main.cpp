#include <unity.h>
#include <ArduinoFake.h>

// Mock FS for WebManager
#include <FS.h>
// fs::FS LittleFS; // Keep commented out for now

#include <WebManager.h>
#include <StreamLogger.h>

// Include implementation files directly
#include "../lib/NativeMock/StreamLoggerMock.cpp"
#include "../lib/NativeMock/WebManagerMock.cpp"
#include "../lib/NativeMock/DeviceRegistryMock.cpp"
#include "../../lib/PlcEngine/Engine/PlcMemory.cpp"
#include "../lib/NativeMock/Preferences.h" 

// Mock for DeviceRegistry to allow instantiation (constructor is protected)
class MockDeviceRegistry : public DeviceRegistry {
public:
    MockDeviceRegistry() : DeviceRegistry() {}

    // Helper to clear registry
    void clearAll() {
        clear();
    }

    // Helper to add mock endpoint
    void addMockEndpoint(const Endpoint& ep) {
        registerEndpoint(ep);
    }
};

WebManager* webManager = nullptr;
StreamLogger* EspHubLog = nullptr;
MockDeviceRegistry* mockDeviceRegistry = nullptr;
PlcMemory* plcMemory = nullptr;

void setUp() {
    if (webManager == nullptr) {
        webManager = new WebManager(nullptr, nullptr, nullptr);
    }
    if (EspHubLog == nullptr) {
        EspHubLog = new StreamLogger(*webManager);
    }
    if (mockDeviceRegistry == nullptr) {
        mockDeviceRegistry = new MockDeviceRegistry();
    }
    if (plcMemory == nullptr) {
        plcMemory = new PlcMemory();
        plcMemory->setDeviceRegistry(mockDeviceRegistry);
    }
    
    // Reset state
    plcMemory->clear();
    mockDeviceRegistry->clearAll();
}

void tearDown() {
    // Optional: clean up
}

void test_dummy() {
    TEST_ASSERT_TRUE(true);
}

void test_plc_memory_init() {
    TEST_ASSERT_NOT_NULL(plcMemory);
    TEST_ASSERT_TRUE(plcMemory->declareVariable("test_var", PlcValueType::BOOL));
}

int main(int argc, char **argv) {
    printf("Starting test with PlcMemory...\n");
    UNITY_BEGIN();
    RUN_TEST(test_dummy);
    RUN_TEST(test_plc_memory_init);
    UNITY_END();
    return 0;
}
