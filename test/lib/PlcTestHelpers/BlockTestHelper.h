#ifndef BLOCK_TEST_HELPER_H
#define BLOCK_TEST_HELPER_H

#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/PlcBlock.h"
#include <ArduinoJson.h>
#include <map>
#include <string>

/**
 * @brief Helper class for testing PLC blocks
 * 
 * Simplifies setting inputs, running blocks, and verifying outputs.
 */
class BlockTestHelper {
private:
    PlcMemory* memory;
    std::map<String, PlcBlock*> blocks;

public:
    BlockTestHelper(PlcMemory* mem) : memory(mem) {}

    ~BlockTestHelper() {
        // Clean up blocks
        for (auto const& [name, block] : blocks) {
            delete block;
        }
        blocks.clear();
    }

    /**
     * @brief Register a block for testing
     */
    void registerBlock(const String& name, PlcBlock* block) {
        blocks[name] = block;
    }

    /**
     * @brief Configure a block with JSON
     */
    void configureBlock(const String& name, JsonDocument& doc) {
        if (blocks.find(name) != blocks.end()) {
            blocks[name]->configure(doc.as<JsonObject>(), *memory);
        }
    }

    /**
     * @brief Set a boolean input variable
     */
    void setInput(const String& varName, bool value) {
        memory->setValue<bool>(varName.c_str(), value);
    }

    /**
     * @brief Set an integer input variable
     */
    void setInput(const String& varName, int32_t value) {
        memory->setValue<int32_t>(varName.c_str(), value);
    }

    /**
     * @brief Set a float input variable
     */
    void setInput(const String& varName, float value) {
        memory->setValue<float>(varName.c_str(), value);
    }

    /**
     * @brief Execute a specific block
     */
    void runBlock(const String& name, unsigned long currentMillis = 0) {
        if (blocks.find(name) != blocks.end()) {
            blocks[name]->evaluate(*memory);
        }
    }

    /**
     * @brief Verify boolean output
     */
    void assertOutput(const String& varName, bool expected) {
        bool actual = memory->getValue<bool>(varName.c_str(), !expected);
        TEST_ASSERT_EQUAL_MESSAGE(expected, actual, ("Output mismatch for " + varName).c_str());
    }

    /**
     * @brief Verify integer output
     */
    void assertOutput(const String& varName, int32_t expected) {
        int32_t actual = memory->getValue<int32_t>(varName.c_str(), expected - 1);
        TEST_ASSERT_EQUAL_INT32_MESSAGE(expected, actual, ("Output mismatch for " + varName).c_str());
    }

    /**
     * @brief Verify float output
     */
    void assertOutput(const String& varName, float expected, float delta = 0.001f) {
        float actual = memory->getValue<float>(varName.c_str(), expected + 1.0f);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(delta, expected, actual, ("Output mismatch for " + varName).c_str());
    }
};

#endif // BLOCK_TEST_HELPER_H
