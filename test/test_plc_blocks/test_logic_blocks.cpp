#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/logic/BlockAND.h"
#include "Blocks/logic/BlockOR.h"
#include "Blocks/logic/BlockNOT.h"
#include "Blocks/logic/BlockXOR.h"
#include "../lib/PlcTestHelpers/BlockTestHelper.h"

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

void test_and_block() {
    // Setup
    BlockAND* block = new BlockAND();
    helper->registerBlock("AND1", block);

    JsonDocument doc;
    doc["inputs"]["in1"] = "in1";
    doc["inputs"]["in2"] = "in2";
    doc["outputs"]["out"] = "out";
    helper->configureBlock("AND1", doc);

    // Test Case 1: F && F = F
    helper->setInput("in1", false);
    helper->setInput("in2", false);
    helper->runBlock("AND1");
    helper->assertOutput("out", false);

    // Test Case 2: T && F = F
    helper->setInput("in1", true);
    helper->setInput("in2", false);
    helper->runBlock("AND1");
    helper->assertOutput("out", false);

    // Test Case 3: F && T = F
    helper->setInput("in1", false);
    helper->setInput("in2", true);
    helper->runBlock("AND1");
    helper->assertOutput("out", false);

    // Test Case 4: T && T = T
    helper->setInput("in1", true);
    helper->setInput("in2", true);
    helper->runBlock("AND1");
    helper->assertOutput("out", true);
}

void test_or_block() {
    // Setup
    BlockOR* block = new BlockOR();
    helper->registerBlock("OR1", block);

    JsonDocument doc;
    doc["inputs"]["in1"] = "in1";
    doc["inputs"]["in2"] = "in2";
    doc["outputs"]["out"] = "out";
    helper->configureBlock("OR1", doc);

    // Test Case 1: F || F = F
    helper->setInput("in1", false);
    helper->setInput("in2", false);
    helper->runBlock("OR1");
    helper->assertOutput("out", false);

    // Test Case 2: T || F = T
    helper->setInput("in1", true);
    helper->setInput("in2", false);
    helper->runBlock("OR1");
    helper->assertOutput("out", true);

    // Test Case 3: F || T = T
    helper->setInput("in1", false);
    helper->setInput("in2", true);
    helper->runBlock("OR1");
    helper->assertOutput("out", true);

    // Test Case 4: T || T = T
    helper->setInput("in1", true);
    helper->setInput("in2", true);
    helper->runBlock("OR1");
    helper->assertOutput("out", true);
}

void test_not_block() {
    // Setup
    BlockNOT* block = new BlockNOT();
    helper->registerBlock("NOT1", block);

    JsonDocument doc;
    doc["inputs"]["in"] = "in";
    doc["outputs"]["out"] = "out";
    helper->configureBlock("NOT1", doc);

    // Test Case 1: !F = T
    helper->setInput("in", false);
    helper->runBlock("NOT1");
    helper->assertOutput("out", true);

    // Test Case 2: !T = F
    helper->setInput("in", true);
    helper->runBlock("NOT1");
    helper->assertOutput("out", false);
}

void test_xor_block() {
    // Setup
    BlockXOR* block = new BlockXOR();
    helper->registerBlock("XOR1", block);

    JsonDocument doc;
    doc["inputs"]["in1"] = "in1";
    doc["inputs"]["in2"] = "in2";
    doc["outputs"]["out"] = "out";
    helper->configureBlock("XOR1", doc);

    // Test Case 1: F ^ F = F
    helper->setInput("in1", false);
    helper->setInput("in2", false);
    helper->runBlock("XOR1");
    helper->assertOutput("out", false);

    // Test Case 2: T ^ F = T
    helper->setInput("in1", true);
    helper->setInput("in2", false);
    helper->runBlock("XOR1");
    helper->assertOutput("out", true);

    // Test Case 3: F ^ T = T
    helper->setInput("in1", false);
    helper->setInput("in2", true);
    helper->runBlock("XOR1");
    helper->assertOutput("out", true);

    // Test Case 4: T ^ T = F
    helper->setInput("in1", true);
    helper->setInput("in2", true);
    helper->runBlock("XOR1");
    helper->assertOutput("out", false);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_and_block);
    RUN_TEST(test_or_block);
    RUN_TEST(test_not_block);
    RUN_TEST(test_xor_block);
    UNITY_END();
}

void loop() {}
