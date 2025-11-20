#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/counters/BlockCTU.h"
#include "Blocks/counters/BlockCTD.h"
#include "Blocks/counters/BlockCTUD.h"
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

void test_ctu_block() {
    // Count Up
    BlockCTU* block = new BlockCTU();
    helper->registerBlock("CTU1", block);

    JsonDocument doc;
    doc["inputs"]["cu"] = "cu";
    doc["inputs"]["reset"] = "reset";
    doc["inputs"]["pv"] = "pv";
    doc["outputs"]["q"] = "q";
    doc["outputs"]["cv"] = "cv";
    helper->configureBlock("CTU1", doc);

    helper->setInput("pv", 3);
    helper->setInput("reset", false);
    helper->setInput("cu", false);

    // Initial state
    helper->runBlock("CTU1");
    helper->assertOutput("cv", 0);
    helper->assertOutput("q", false);

    // Count 1
    helper->setInput("cu", true);
    helper->runBlock("CTU1");
    helper->assertOutput("cv", 1);
    helper->setInput("cu", false); // Reset edge
    helper->runBlock("CTU1");

    // Count 2
    helper->setInput("cu", true);
    helper->runBlock("CTU1");
    helper->assertOutput("cv", 2);
    helper->setInput("cu", false);
    helper->runBlock("CTU1");

    // Count 3 (Reached PV)
    helper->setInput("cu", true);
    helper->runBlock("CTU1");
    helper->assertOutput("cv", 3);
    helper->assertOutput("q", true);

    // Count 4 (Above PV)
    helper->setInput("cu", false);
    helper->runBlock("CTU1");
    helper->setInput("cu", true);
    helper->runBlock("CTU1");
    helper->assertOutput("cv", 4);
    helper->assertOutput("q", true);

    // Reset
    helper->setInput("reset", true);
    helper->runBlock("CTU1");
    helper->assertOutput("cv", 0);
    helper->assertOutput("q", false);
}

void test_ctd_block() {
    // Count Down
    BlockCTD* block = new BlockCTD();
    helper->registerBlock("CTD1", block);

    JsonDocument doc;
    doc["inputs"]["cd"] = "cd";
    doc["inputs"]["load"] = "load";
    doc["inputs"]["pv"] = "pv";
    doc["outputs"]["q"] = "q";
    doc["outputs"]["cv"] = "cv";
    helper->configureBlock("CTD1", doc);

    helper->setInput("pv", 3);
    helper->setInput("load", true); // Load PV
    helper->runBlock("CTD1");
    helper->assertOutput("cv", 3);
    helper->assertOutput("q", false);
    helper->setInput("load", false);

    // Count down 1
    helper->setInput("cd", true);
    helper->runBlock("CTD1");
    helper->assertOutput("cv", 2);
    helper->setInput("cd", false);
    helper->runBlock("CTD1");

    // Count down 2
    helper->setInput("cd", true);
    helper->runBlock("CTD1");
    helper->assertOutput("cv", 1);
    helper->setInput("cd", false);
    helper->runBlock("CTD1");

    // Count down 3 (Zero)
    helper->setInput("cd", true);
    helper->runBlock("CTD1");
    helper->assertOutput("cv", 0);
    helper->assertOutput("q", true);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_ctu_block);
    RUN_TEST(test_ctd_block);
    UNITY_END();
}

void loop() {}
