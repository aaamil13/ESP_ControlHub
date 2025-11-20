#include <unity.h>
#include "Engine/PlcMemory.h"
#include "Blocks/logic/BlockAND.h"
#include "Blocks/logic/BlockOR.h"
#include "Blocks/logic/BlockNOT.h"
#include "Blocks/timers/BlockTON.h"
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

/**
 * @brief Test Latch Circuit (Start/Stop)
 * 
 * Logic: Motor = (Start OR Motor) AND (NOT Stop)
 */
void test_latch_circuit() {
    // Blocks
    BlockOR* orBlock = new BlockOR();
    BlockAND* andBlock = new BlockAND();
    BlockNOT* notBlock = new BlockNOT();

    helper->registerBlock("OR1", orBlock);
    helper->registerBlock("AND1", andBlock);
    helper->registerBlock("NOT1", notBlock);

    // Wiring
    // OR1: IN1=Start, IN2=Motor -> OUT=OrOut
    JsonDocument docOr;
    docOr["inputs"]["in1"] = "start";
    docOr["inputs"]["in2"] = "motor";
    docOr["outputs"]["out"] = "or_out";
    helper->configureBlock("OR1", docOr);

    // NOT1: IN=Stop -> OUT=NotStop
    JsonDocument docNot;
    docNot["inputs"]["in"] = "stop";
    docNot["outputs"]["out"] = "not_stop";
    helper->configureBlock("NOT1", docNot);

    // AND1: IN1=OrOut, IN2=NotStop -> OUT=Motor
    JsonDocument docAnd;
    docAnd["inputs"]["in1"] = "or_out";
    docAnd["inputs"]["in2"] = "not_stop";
    docAnd["outputs"]["out"] = "motor";
    helper->configureBlock("AND1", docAnd);

    // Initial State
    helper->setInput("start", false);
    helper->setInput("stop", false);
    helper->setInput("motor", false);

    // Cycle 1: Idle
    helper->runBlock("NOT1");
    helper->runBlock("OR1");
    helper->runBlock("AND1");
    helper->assertOutput("motor", false);

    // Cycle 2: Press Start
    helper->setInput("start", true);
    helper->runBlock("NOT1");
    helper->runBlock("OR1");
    helper->runBlock("AND1");
    helper->assertOutput("motor", true);

    // Cycle 3: Release Start (Latch should hold)
    helper->setInput("start", false);
    helper->runBlock("NOT1");
    helper->runBlock("OR1"); // IN2 (motor) is true from prev cycle
    helper->runBlock("AND1");
    helper->assertOutput("motor", true);

    // Cycle 4: Press Stop
    helper->setInput("stop", true);
    helper->runBlock("NOT1"); // not_stop becomes false
    helper->runBlock("OR1");
    helper->runBlock("AND1"); // true AND false = false
    helper->assertOutput("motor", false);

    // Cycle 5: Release Stop
    helper->setInput("stop", false);
    helper->runBlock("NOT1");
    helper->runBlock("OR1");
    helper->runBlock("AND1");
    helper->assertOutput("motor", false);
}

/**
 * @brief Test Traffic Light Sequence
 * 
 * Simple sequence: Red -> Green -> Yellow -> Red
 * Using timers (TON)
 */
void test_traffic_light_sequence() {
    // Timers
    BlockTON* timerRed = new BlockTON();
    BlockTON* timerGreen = new BlockTON();
    BlockTON* timerYellow = new BlockTON();

    helper->registerBlock("T_RED", timerRed);
    helper->registerBlock("T_GREEN", timerGreen);
    helper->registerBlock("T_YELLOW", timerYellow);

    // Configuration
    JsonDocument docRed;
    docRed["inputs"]["in"] = "state_red";
    docRed["inputs"]["pt"] = 3000;
    docRed["outputs"]["q"] = "red_done";
    helper->configureBlock("T_RED", docRed);

    JsonDocument docGreen;
    docGreen["inputs"]["in"] = "state_green";
    docGreen["inputs"]["pt"] = 3000;
    docGreen["outputs"]["q"] = "green_done";
    helper->configureBlock("T_GREEN", docGreen);

    JsonDocument docYellow;
    docYellow["inputs"]["in"] = "state_yellow";
    docYellow["inputs"]["pt"] = 1000;
    docYellow["outputs"]["q"] = "yellow_done";
    helper->configureBlock("T_YELLOW", docYellow);

    // Initial State: Red is ON
    helper->setInput("state_red", true);
    helper->setInput("state_green", false);
    helper->setInput("state_yellow", false);

    // Run Red Timer (0ms)
    helper->runBlock("T_RED", timeManager->millis());
    helper->assertOutput("red_done", false);

    // Advance 3000ms
    timeManager->advance(3000);
    helper->runBlock("T_RED", timeManager->millis());
    helper->assertOutput("red_done", true);

    // Transition Logic (Simulated): Red Done -> Green Start
    if (memory->getValue<bool>("red_done", false)) {
        helper->setInput("state_red", false);
        helper->setInput("state_green", true);
    }

    // Run Green Timer (3000ms absolute time)
    // Note: Timer starts counting when IN goes true
    helper->runBlock("T_GREEN", timeManager->millis()); 
    helper->assertOutput("green_done", false);

    // Advance 3000ms
    timeManager->advance(3000);
    helper->runBlock("T_GREEN", timeManager->millis());
    helper->assertOutput("green_done", true);

    // Transition Logic: Green Done -> Yellow Start
    if (memory->getValue<bool>("green_done", false)) {
        helper->setInput("state_green", false);
        helper->setInput("state_yellow", true);
    }

    // Run Yellow Timer
    helper->runBlock("T_YELLOW", timeManager->millis());
    helper->assertOutput("yellow_done", false);

    // Advance 1000ms
    timeManager->advance(1000);
    helper->runBlock("T_YELLOW", timeManager->millis());
    helper->assertOutput("yellow_done", true);
}

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_latch_circuit);
    RUN_TEST(test_traffic_light_sequence);
    UNITY_END();
}

void loop() {}
