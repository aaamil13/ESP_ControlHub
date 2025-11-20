#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/logic/BlockAND.h"
#include "Blocks/math/BlockADD.h"
#include "../lib/PlcTestHelpers/BlockTestHelper.h"
#include <vector>

PlcMemory* memory = nullptr;
BlockTestHelper* helper = nullptr;

void setUp(void) {
    memory = new PlcMemory();
    helper = new BlockTestHelper(memory);
}

void tearDown(void) {
    delete helper;
    delete memory;
}

/**
 * @brief Test Maximum Variable Allocation
 * 
 * Tries to allocate a large number of variables to check memory stability.
 */
void test_max_variables() {
    const int NUM_VARS = 1000;
    char varName[32];

    // Allocation Phase
    for (int i = 0; i < NUM_VARS; i++) {
        sprintf(varName, "var_%d", i);
        memory->setValue<int32_t>(varName, i);
    }

    // Verification Phase
    for (int i = 0; i < NUM_VARS; i++) {
        sprintf(varName, "var_%d", i);
        TEST_ASSERT_EQUAL_INT32(i, memory->getValue<int32_t>(varName, -1));
    }
}

/**
 * @brief Test Block Chaining Depth
 * 
 * Creates a long chain of ADD blocks: Out = 1 + 1 + 1 ...
 */
void test_block_chaining() {
    const int CHAIN_LENGTH = 100;
    std::vector<BlockADD*> blocks;
    char outName[32];
    char inName[32];

    // Create Chain
    for (int i = 0; i < CHAIN_LENGTH; i++) {
        BlockADD* block = new BlockADD();
        blocks.push_back(block);
        
        // Register manually to avoid helper overhead for mass creation, 
        // or use helper if we want consistent config
        char blockName[32];
        sprintf(blockName, "ADD_%d", i);
        helper->registerBlock(blockName, block);

        JsonDocument doc;
        JsonArray inputs = doc["inputs"].to<JsonArray>();
        
        if (i == 0) {
            inputs.add("in_start");
            memory->setValue<float>("in_start", 0.0f);
        } else {
            sprintf(inName, "res_%d", i-1);
            inputs.add(inName);
        }
        
        inputs.add("const_1"); // We'll set this var to 1.0
        
        sprintf(outName, "res_%d", i);
        doc["outputs"]["out"] = outName;
        
        helper->configureBlock(blockName, doc);
    }
    
    memory->setValue<float>("const_1", 1.0f);

    // Execute Chain
    for (int i = 0; i < CHAIN_LENGTH; i++) {
        char blockName[32];
        sprintf(blockName, "ADD_%d", i);
        helper->runBlock(blockName);
    }

    // Verify Final Result
    sprintf(outName, "res_%d", CHAIN_LENGTH - 1);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, (float)CHAIN_LENGTH, memory->getValue<float>(outName, 0.0f));
}

/**
 * @brief Test Memory Fragmentation (Simulation)
 * 
 * Repeatedly allocates and deallocates variables.
 */
void test_memory_churn() {
    const int CYCLES = 100;
    const int VARS_PER_CYCLE = 50;
    char varName[32];

    for (int c = 0; c < CYCLES; c++) {
        // Allocate
        for (int i = 0; i < VARS_PER_CYCLE; i++) {
            sprintf(varName, "churn_%d_%d", c, i);
            memory->setValue<std::string>(varName, "some_test_string_data");
        }

        // Verify & Modify
        for (int i = 0; i < VARS_PER_CYCLE; i++) {
            sprintf(varName, "churn_%d_%d", c, i);
            std::string val = memory->getValue<std::string>(varName, "");
            TEST_ASSERT_EQUAL_STRING("some_test_string_data", val.c_str());
        }
    }
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_max_variables);
    RUN_TEST(test_block_chaining);
    RUN_TEST(test_memory_churn);
    UNITY_END();
}

void loop() {}
