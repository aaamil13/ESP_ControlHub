# Test Expansion Plan

This plan outlines the steps to expand the test coverage for the EspHub project, focusing on the PLC Engine, Event Manager, and PLC Memory.

## Phase 1: Setup and `PlcMemory`

- [x] **1.1: Create Test Directory Structure:** Create dedicated directories for PLC, Event Manager, and Memory tests within the `test` directory.
- [x] **1.2: Analyze Existing `PlcMemory` Tests:** Review `test/plc_memory/test_plc_memory.cpp` to understand the current test coverage.
- [ ] **1.3: Expand `PlcMemory` Tests:** Add new tests to achieve comprehensive coverage of `PlcMemory`, including various data types and memory areas.
- [ ] **1.4: Document `PlcMemory` Tests:** Create a `README.md` in `test/plc_memory` detailing the tests performed and the results.
- [ ] **1.5: Commit `PlcMemory` work.**

## Phase 2: `PlcEngine` and PLC Blocks

- [ ] **2.1: List all PLC Blocks:** Systematically identify every PLC block available in `lib/PlcEngine/Blocks/`.
- [ ] **2.2: Create Test Programs (JSON):** For each block, create a minimal JSON PLC program that isolates and tests its functionality. Store these in `test/plc_engine/programs/`.
- [ ] **2.3: Implement Block Tests:** Create a new test file `test/plc_engine/test_plc_blocks.cpp`. Write a test case for each JSON program, load it into the `PlcEngine`, execute it, and assert the expected outcome.
- [ ] **2.4: Test Program Import:** Create a larger, more complex JSON program that combines several blocks and test the `PlcEngine`'s ability to import and run it correctly.
- [ ] **2.5: Document `PlcEngine` Tests:** Create a `README.md` in `test/plc_engine` detailing the block-by-block tests and the integration test.
- [ ] **2.6: Commit `PlcEngine` work.**

## Phase 3: `IOEventManager`

- [ ] **3.1: Create `IOEventManager` Test File:** Create `test/event_manager/test_event_manager.cpp`.
- [ ] **3.2: Implement Event Tests:** Write tests to cover:
    - Event creation and configuration from JSON.
    - Triggering events based on PLC outputs.
    - Correct execution of actions associated with events.
- [ ] **3.3: Document `IOEventManager` Tests:** Create a `README.md` in `test/event_manager` detailing the tests.
- [ ] **3.4: Commit `IOEventManager` work.**

## Phase 4: Final Review and Recommendations

- [ ] **4.1: Review Test Coverage:** Analyze the overall test coverage and identify any remaining gaps.
- [ ] **4.2: Write Final Report:** Create a final report in `test/test_summary.md` summarizing the work done, the results, and any recommendations for code improvements discovered during testing.
- [ ] **4.3: Commit final report.**
