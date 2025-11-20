#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/timers/BlockTON.h"
#include "Blocks/timers/BlockTOF.h"
#include "Blocks/timers/BlockTP.h"
#include "../lib/PlcTestHelpers/BlockTestHelper.h"
#include "../lib/PlcTestHelpers/MockTimeManager.h"

PlcMemory* memory = nullptr;
BlockTestHelper* helper = nullptr;
MockTimeManager* timeManager = nullptr;

void setUp(void) {
    memory = new PlcMemory();
    helper = new BlockTestHelper(memory);
    timeManager = new MockTimeManager();
}

void tearDown(void) {
    delete timeManager;
    delete helper;
    delete memory;
}

void test_ton_block() {
    // Timer On-Delay
    BlockTON* block = new BlockTON();
    helper->registerBlock("TON1", block);

    JsonDocument doc;
    doc["inputs"]["in"] = "in";
    doc["inputs"]["pt"] = 1000; // Static PT as per implementation
    doc["outputs"]["q"] = "q";
    doc["outputs"]["et"] = "et";
    helper->configureBlock("TON1", doc);

    helper->setInput("in", false);
    
    // Initial state
    helper->runBlock("TON1", timeManager->millis());
    helper->assertOutput("q", false);
    helper->assertOutput("et", 0);

    // Start timer
    helper->setInput("in", true);
    helper->runBlock("TON1", timeManager->millis());
    helper->assertOutput("q", false);

    // Advance 500ms (halfway)
    timeManager->advance(500);
    helper->runBlock("TON1", timeManager->millis());
    helper->assertOutput("q", false);
    helper->assertOutput("et", 500);

    // Advance to 1000ms (done)
    timeManager->advance(500);
    helper->runBlock("TON1", timeManager->millis());
    helper->assertOutput("q", true);
    helper->assertOutput("et", 1000);

    // Advance past 1000ms (should stay true)
    timeManager->advance(500);
    helper->runBlock("TON1", timeManager->millis());
    helper->assertOutput("q", true);
    helper->assertOutput("et", 1000); // ET usually clamps at PT

    // Stop timer
    helper->setInput("in", false);
    helper->runBlock("TON1", timeManager->millis());
    helper->assertOutput("q", false);
    helper->assertOutput("et", 0);
}

void test_tof_block() {
    // Timer Off-Delay
    BlockTOF* block = new BlockTOF();
    helper->registerBlock("TOF1", block);

    JsonDocument doc;
    doc["inputs"]["in"] = "in";
    doc["inputs"]["pt"] = 1000;
    doc["outputs"]["q"] = "q";
    doc["outputs"]["et"] = "et";
    helper->configureBlock("TOF1", doc);
    
    // Initial state (IN=false, Q=false)
    helper->setInput("in", false);
    helper->runBlock("TOF1", timeManager->millis());
    helper->assertOutput("q", false);

    // Set IN=true (Q becomes true immediately)
    helper->setInput("in", true);
    helper->runBlock("TOF1", timeManager->millis());
    helper->assertOutput("q", true);
    helper->assertOutput("et", 0);

    // Set IN=false (Timer starts, Q stays true)
    helper->setInput("in", false);
    helper->runBlock("TOF1", timeManager->millis());
    helper->assertOutput("q", true);

    // Advance 500ms
    timeManager->advance(500);
    helper->runBlock("TOF1", timeManager->millis());
    helper->assertOutput("q", true);
    helper->assertOutput("et", 500);

    // Advance to 1000ms (Timer done, Q becomes false)
    timeManager->advance(500);
    helper->runBlock("TOF1", timeManager->millis());
    helper->assertOutput("q", false);
    helper->assertOutput("et", 1000);
}

void test_tp_block() {
    // Pulse Timer
    BlockTP* block = new BlockTP();
    helper->registerBlock("TP1", block);

    JsonDocument doc;
    doc["inputs"]["in"] = "in";
    doc["inputs"]["pt"] = 1000;
    doc["outputs"]["q"] = "q";
    doc["outputs"]["et"] = "et";
    helper->configureBlock("TP1", doc);

    helper->setInput("in", false);

    // Start pulse
    helper->setInput("in", true);
    helper->runBlock("TP1", timeManager->millis());
    helper->assertOutput("q", true);

    // Advance 500ms
    timeManager->advance(500);
    helper->runBlock("TP1", timeManager->millis());
    helper->assertOutput("q", true);

    // Turn off input (Pulse should continue)
    helper->setInput("in", false);
    helper->runBlock("TP1", timeManager->millis());
    helper->assertOutput("q", true);

    // Advance to 1000ms (Pulse done)
    timeManager->advance(500);
    helper->runBlock("TP1", timeManager->millis());
    helper->assertOutput("q", false);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_ton_block);
    RUN_TEST(test_tof_block);
    RUN_TEST(test_tp_block);
    UNITY_END();
}

void loop() {}
