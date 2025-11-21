#include <unity.h>
#include <Arduino.h>
#include <string>
#include <cstring>
#include <cstdint>
#include <limits>
#include <vector>
#include <map>

// Mock FS for WebManager
#include <FS.h>
fs::FS LittleFS;

#include <WebManager.h>
#include <StreamLogger.h>

#define MAX_STRING_LENGTH 64

// Dummy WebManager for StreamLogger (instantiated with nulls)
WebManager webManager(nullptr, nullptr);
StreamLogger* EspHubLog = new StreamLogger(webManager);

// Include implementation files directly to ensure compilation in native mode
#include "../../lib/PlcEngine/Engine/PlcMemory.cpp"
#include "../../lib/Devices/DeviceRegistry.cpp"
#include "../lib/NativeMock/StreamLoggerMock.cpp"
#include "../lib/NativeMock/WebManagerMock.cpp"

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

// Global instance of PlcMemory for testing
PlcMemory plcMemory;
MockDeviceRegistry mockDeviceRegistry;

void setUp(void) {
    // set up runs before each test
    plcMemory.clear(); // Clear memory before each test
    plcMemory.setDeviceRegistry(&mockDeviceRegistry);
    mockDeviceRegistry.clearAll(); // Clear mock registry before each test
    // Initialize NVS for retentive memory tests
    Preferences preferences;
    preferences.begin("plcmemory", false);
    preferences.clear(); // Clear any previous test data
    preferences.end();
}

void tearDown(void) {
    // tear down runs after each test
    // Clear NVS after each test
    Preferences preferences;
    preferences.begin("plcmemory", false);
    preferences.clear();
    preferences.end();
}

void test_declare_variable_bool() {
    TEST_ASSERT_TRUE(plcMemory.declareVariable("test_bool", PlcValueType::BOOL));
    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("test_bool", false) == false); // Default value
}

void test_declare_variable_int() {
    TEST_ASSERT_TRUE(plcMemory.declareVariable("test_int", PlcValueType::INT));
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("test_int", 0) == 0); // Default value
}

void test_declare_variable_real() {
    TEST_ASSERT_TRUE(plcMemory.declareVariable("test_real", PlcValueType::REAL));
    TEST_ASSERT_TRUE(plcMemory.getValue<float>("test_real", 0.0f) == 0.0f); // Default value
}

void test_set_get_value_bool() {
    plcMemory.declareVariable("var_bool", PlcValueType::BOOL);
    plcMemory.setValue("var_bool", true);
    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("var_bool", false) == true);
    plcMemory.setValue("var_bool", false);
    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("var_bool", true) == false);
}

void test_set_get_value_int() {
    plcMemory.declareVariable("var_int", PlcValueType::INT);
    plcMemory.setValue("var_int", (int16_t)123);
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("var_int", 0) == 123);
    plcMemory.setValue("var_int", (int16_t)-456);
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("var_int", 0) == -456);
}

void test_set_get_value_real() {
    plcMemory.declareVariable("var_real", PlcValueType::REAL);
    plcMemory.setValue("var_real", 123.45f);
    TEST_ASSERT_EQUAL_FLOAT(123.45f, plcMemory.getValue<float>("var_real", 0.0f));
    plcMemory.setValue("var_real", -67.89f);
    TEST_ASSERT_EQUAL_FLOAT(-67.89f, plcMemory.getValue<float>("var_real", 0.0f));
}

void test_set_get_value_string() {
    plcMemory.declareVariable("var_string", PlcValueType::STRING_TYPE);
    plcMemory.setValue("var_string", (const char*)"Hello");
    TEST_ASSERT_EQUAL_STRING("Hello", plcMemory.getValue<const char*>("var_string", ""));
    plcMemory.setValue("var_string", (const char*)"World_Longer_String_Than_Before_To_Test_Buffer_Overflow_But_Not_Too_Long");
    TEST_ASSERT_EQUAL_STRING("World_Longer_String_Than_Before_To_Test_Buffer_Overflow_But_Not_Too_Long", plcMemory.getValue<const char*>("var_string", ""));
}

void test_get_value_non_existent() {
    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("non_existent_bool", true) == true);
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("non_existent_int", 999) == 999);
    TEST_ASSERT_EQUAL_FLOAT(55.5f, plcMemory.getValue<float>("non_existent_real", 55.5f));
    TEST_ASSERT_EQUAL_STRING("default", plcMemory.getValue<const char*>("non_existent_string", "default"));
}

void test_retentive_memory_save_load() {
    plcMemory.declareVariable("ret_bool", PlcValueType::BOOL, true);
    plcMemory.setValue("ret_bool", true);
    plcMemory.declareVariable("ret_int", PlcValueType::INT, true);
    plcMemory.setValue("ret_int", (int16_t)100);
    plcMemory.declareVariable("non_ret_real", PlcValueType::REAL, false);
    plcMemory.setValue("non_ret_real", 50.0f);

    plcMemory.saveRetentiveMemory();

    // Create a new PlcMemory instance to simulate restart
    PlcMemory newPlcMemory;
    newPlcMemory.setDeviceRegistry(&mockDeviceRegistry);
    newPlcMemory.begin(); // This should load retentive memory

    // Retentive variables should retain their values
    newPlcMemory.declareVariable("ret_bool", PlcValueType::BOOL, true); // Re-declare to ensure access
    TEST_ASSERT_TRUE(newPlcMemory.getValue<bool>("ret_bool", false) == true);
    newPlcMemory.declareVariable("ret_int", PlcValueType::INT, true);
    TEST_ASSERT_TRUE(newPlcMemory.getValue<int16_t>("ret_int", 0) == 100);

    // Non-retentive variable should not exist or be at default
    TEST_ASSERT_EQUAL_FLOAT(0.0f, newPlcMemory.getValue<float>("non_ret_real", 0.0f));
}

void test_clear_memory() {
    plcMemory.declareVariable("var1", PlcValueType::BOOL);
    plcMemory.declareVariable("var2", PlcValueType::INT);
    plcMemory.setValue("var1", true);
    plcMemory.setValue("var2", (int16_t)5);

    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("var1", false) == true);
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("var2", 0) == 5);

    plcMemory.clear();

    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("var1", false) == false); // Should be default after clear
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("var2", 0) == 0);     // Should be default after clear
}

void test_io_point_registration_and_sync() {
    plcMemory.declareVariable("plc_output", PlcValueType::BOOL);
    plcMemory.declareVariable("plc_input", PlcValueType::INT);

    // Setup mock endpoint
    Endpoint outputEndpoint;
    outputEndpoint.fullName = "mock.output.endpoint";
    outputEndpoint.protocol = ProtocolType::WIFI; // Example
    outputEndpoint.datatype = PlcValueType::BOOL;
    outputEndpoint.isWritable = true;
    outputEndpoint.isOnline = true;
    mockDeviceRegistry.addMockEndpoint(outputEndpoint);

    Endpoint inputEndpoint;
    inputEndpoint.fullName = "mock.input.endpoint";
    inputEndpoint.protocol = ProtocolType::WIFI; // Example
    inputEndpoint.datatype = PlcValueType::INT;
    inputEndpoint.isWritable = false;
    inputEndpoint.isOnline = true;
    inputEndpoint.currentValue.type = PlcValueType::INT;
    inputEndpoint.currentValue.value.i16Val = 99; // Simulate an external input value
    mockDeviceRegistry.addMockEndpoint(inputEndpoint);

    TEST_ASSERT_TRUE(plcMemory.registerIOPoint("plc_output", "mock.output.endpoint", IODirection::IO_OUTPUT, "test_program", false, "", true));
    TEST_ASSERT_TRUE(plcMemory.registerIOPoint("plc_input", "mock.input.endpoint", IODirection::IO_INPUT, "test_program", false, "", true));

    // Simulate PLC output change
    plcMemory.setValue("plc_output", true);
    plcMemory.syncIOPoints(); // Sync all

    // Verify output was sent to mock endpoint
    TEST_ASSERT_TRUE(mockDeviceRegistry.getEndpoint("mock.output.endpoint")->currentValue.value.bVal == true);

    // Verify input was read from mock endpoint
    plcMemory.syncIOPoints(); // Sync again to read input
    TEST_ASSERT_TRUE(plcMemory.getValue<int16_t>("plc_input", 0) == 99);
}

void test_get_value_as_plc_value() {
    plcMemory.declareVariable("bool_var", PlcValueType::BOOL);
    plcMemory.setValue("bool_var", true);
    PlcValue val_bool = plcMemory.getValueAsPlcValue("bool_var");
    TEST_ASSERT_EQUAL(PlcValueType::BOOL, val_bool.type);
    TEST_ASSERT_TRUE(val_bool.value.bVal);

    plcMemory.declareVariable("int_var", PlcValueType::INT);
    plcMemory.setValue("int_var", (int16_t)42);
    PlcValue val_int = plcMemory.getValueAsPlcValue("int_var");
    TEST_ASSERT_EQUAL(PlcValueType::INT, val_int.type);
    TEST_ASSERT_EQUAL(42, val_int.value.i16Val);

    plcMemory.declareVariable("real_var", PlcValueType::REAL);
    plcMemory.setValue("real_var", 3.14f);
    PlcValue val_real = plcMemory.getValueAsPlcValue("real_var");
    TEST_ASSERT_EQUAL(PlcValueType::REAL, val_real.type);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, val_real.value.fVal);
}

void test_declare_and_set_get_additional_int_types() {
    // DINT (int32_t)
    TEST_ASSERT_TRUE(plcMemory.declareVariable("test_dint", PlcValueType::DINT));
    plcMemory.setValue("test_dint", (int32_t)123456);
    TEST_ASSERT_EQUAL_INT32(123456, plcMemory.getValue<int32_t>("test_dint", 0));
    plcMemory.setValue("test_dint", (int32_t)-789012);
    TEST_ASSERT_EQUAL_INT32(-789012, plcMemory.getValue<int32_t>("test_dint", 0));
}

void test_integer_boundary_values() {
    // INT (int16_t)
    plcMemory.declareVariable("int_max", PlcValueType::INT);
    plcMemory.setValue("int_max", std::numeric_limits<int16_t>::max());
    TEST_ASSERT_EQUAL_INT16(std::numeric_limits<int16_t>::max(), plcMemory.getValue<int16_t>("int_max", 0));

    plcMemory.declareVariable("int_min", PlcValueType::INT);
    plcMemory.setValue("int_min", std::numeric_limits<int16_t>::min());
    TEST_ASSERT_EQUAL_INT16(std::numeric_limits<int16_t>::min(), plcMemory.getValue<int16_t>("int_min", 0));

    // DINT (int32_t)
    plcMemory.declareVariable("dint_max", PlcValueType::DINT);
    plcMemory.setValue("dint_max", std::numeric_limits<int32_t>::max());
    TEST_ASSERT_EQUAL_INT32(std::numeric_limits<int32_t>::max(), plcMemory.getValue<int32_t>("dint_max", 0));

    plcMemory.declareVariable("dint_min", PlcValueType::DINT);
    plcMemory.setValue("dint_min", std::numeric_limits<int32_t>::min());
    TEST_ASSERT_EQUAL_INT32(std::numeric_limits<int32_t>::min(), plcMemory.getValue<int32_t>("dint_min", 0));
}

void test_error_handling() {
    // Redeclaring a variable should fail
    plcMemory.declareVariable("redec_var", PlcValueType::BOOL);
    TEST_ASSERT_FALSE(plcMemory.declareVariable("redec_var", PlcValueType::INT));

    // Setting value of a non-existent variable should not create it
    plcMemory.setValue("no_such_var", true);
    TEST_ASSERT_TRUE(plcMemory.getValue<bool>("no_such_var", false) == false); // Should return default

    // Type mismatch
    plcMemory.declareVariable("type_mismatch_int", PlcValueType::INT);
    plcMemory.setValue("type_mismatch_int", (const char*)"not a number");
    TEST_ASSERT_EQUAL_INT16(0, plcMemory.getValue<int16_t>("type_mismatch_int", -1));
}

void test_string_length_limit() {
    plcMemory.declareVariable("long_string", PlcValueType::STRING_TYPE);
    // Create a string that is exactly MAX_STRING_LENGTH
    std::string s(MAX_STRING_LENGTH, 'a');
    plcMemory.setValue("long_string", s.c_str());
    TEST_ASSERT_EQUAL_STRING(s.c_str(), plcMemory.getValue<const char*>("long_string", ""));

    // Create a string that is longer than MAX_STRING_LENGTH
    std::string too_long = s + "a";
    plcMemory.setValue("long_string", too_long.c_str());
    
    // The value should be truncated to MAX_STRING_LENGTH
    const char* result = plcMemory.getValue<const char*>("long_string", "");
    TEST_ASSERT_EQUAL(MAX_STRING_LENGTH, strlen(result));
    TEST_ASSERT_EQUAL_STRING_LEN(s.c_str(), result, MAX_STRING_LENGTH);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_declare_variable_bool);
    RUN_TEST(test_declare_variable_int);
    RUN_TEST(test_declare_variable_real);
    RUN_TEST(test_set_get_value_bool);
    RUN_TEST(test_set_get_value_int);
    RUN_TEST(test_set_get_value_real);
    RUN_TEST(test_set_get_value_string);
    RUN_TEST(test_get_value_non_existent);
    RUN_TEST(test_retentive_memory_save_load);
    RUN_TEST(test_clear_memory);
    RUN_TEST(test_io_point_registration_and_sync);
    RUN_TEST(test_get_value_as_plc_value);
    RUN_TEST(test_declare_and_set_get_additional_int_types);
    RUN_TEST(test_integer_boundary_values);
    RUN_TEST(test_error_handling);
    RUN_TEST(test_string_length_limit);
    UNITY_END();
}

void loop() {
}
