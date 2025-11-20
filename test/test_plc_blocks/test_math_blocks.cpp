#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/math/BlockADD.h"
#include "Blocks/math/BlockSUB.h"
#include "Blocks/math/BlockMUL.h"
#include "Blocks/math/BlockDIV.h"
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

void test_add_block() {
    BlockADD* block = new BlockADD();
    helper->registerBlock("ADD1", block);

    JsonDocument doc;
    JsonArray inputs = doc["inputs"].to<JsonArray>();
    inputs.add("in1");
    inputs.add("in2");
    doc["outputs"]["out"] = "out";
    helper->configureBlock("ADD1", doc);

    helper->setInput("in1", 10.0f);
    helper->setInput("in2", 20.0f);
    helper->runBlock("ADD1");
    helper->assertOutput("out", 30.0f);

    helper->setInput("in1", -5.0f);
    helper->setInput("in2", 5.0f);
    helper->runBlock("ADD1");
    helper->assertOutput("out", 0.0f);
}

void test_sub_block() {
    BlockSUB* block = new BlockSUB();
    helper->registerBlock("SUB1", block);

    JsonDocument doc;
    doc["inputs"]["in1"] = "in1";
    doc["inputs"]["in2"] = "in2";
    doc["outputs"]["out"] = "out";
    helper->configureBlock("SUB1", doc);

    helper->setInput("in1", 50.0f);
    helper->setInput("in2", 20.0f);
    helper->runBlock("SUB1");
    helper->assertOutput("out", 30.0f);

    helper->setInput("in1", 10.0f);
    helper->setInput("in2", 20.0f);
    helper->runBlock("SUB1");
    helper->assertOutput("out", -10.0f);
}

void test_mul_block() {
    BlockMUL* block = new BlockMUL();
    helper->registerBlock("MUL1", block);

    JsonDocument doc;
    JsonArray inputs = doc["inputs"].to<JsonArray>();
    inputs.add("in1");
    inputs.add("in2");
    doc["outputs"]["out"] = "out";
    helper->configureBlock("MUL1", doc);

    helper->setInput("in1", 5.0f);
    helper->setInput("in2", 6.0f);
    helper->runBlock("MUL1");
    helper->assertOutput("out", 30.0f);

    helper->setInput("in1", -2.0f);
    helper->setInput("in2", 5.0f);
    helper->runBlock("MUL1");
    helper->assertOutput("out", -10.0f);
}

void test_div_block() {
    BlockDIV* block = new BlockDIV();
    helper->registerBlock("DIV1", block);

    JsonDocument doc;
    doc["inputs"]["in1"] = "in1";
    doc["inputs"]["in2"] = "in2";
    doc["outputs"]["out"] = "out";
    helper->configureBlock("DIV1", doc);

    // Normal division
    helper->setInput("in1", 10.0f);
    helper->setInput("in2", 2.0f);
    helper->runBlock("DIV1");
    helper->assertOutput("out", 5.0f);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_add_block);
    RUN_TEST(test_sub_block);
    RUN_TEST(test_mul_block);
    RUN_TEST(test_div_block);
    UNITY_END();
}

void loop() {}
