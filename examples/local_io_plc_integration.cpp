/**
 * @file local_io_plc_integration.cpp
 * @brief Complete example of Local IO Manager integrated with PLC Engine
 *
 * This example demonstrates:
 * 1. Setting up Local IO Manager with hardware pins
 * 2. Configuring PLC-to-IO variable mapping
 * 3. Running PLC logic that uses local hardware I/O
 * 4. Automatic synchronization between PLC variables and IO pins
 *
 * Hardware Setup:
 * - GPIO 34: Temperature sensor (analog input, 0-3.3V)
 * - GPIO 35: Temperature sensor online signal (digital input, pulldown)
 * - GPIO 25: Heater control relay (digital output)
 * - GPIO 26: Alarm LED (digital output)
 * - GPIO 27: Status LED (PWM output for blinking)
 * - GPIO 12: Acknowledge button (digital input, pullup)
 * - GPIO 13: Fan speed control (PWM output)
 */

#include <Arduino.h>
#include <LittleFS.h>
#include "Core/EspHub.h"
#include "LocalIO/LocalIOManager.h"
#include "PlcEngine/Engine/PlcEngine.h"
#include "Core/TimeManager.h"
#include "Protocols/Mesh/MeshDeviceManager.h"

// Global objects
LocalIOManager ioManager;
PlcEngine* plcEngine;
TimeManager* timeManager;
MeshDeviceManager* meshManager;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== EspHub: Local IO + PLC Integration Example ===\n");

    // Initialize filesystem
    if (!LittleFS.begin(true)) {
        Serial.println("ERROR: Failed to mount LittleFS");
        return;
    }

    // Initialize time manager
    timeManager = new TimeManager();
    timeManager->begin();

    // Initialize mesh manager (required by PLC engine, but not used for local IO)
    meshManager = new MeshDeviceManager();

    // Initialize PLC engine
    plcEngine = new PlcEngine(timeManager, meshManager);
    plcEngine->begin();

    // Initialize Local IO Manager
    Serial.println("1. Initializing Local IO Manager...");
    ioManager.begin();

    // Load IO configuration
    Serial.println("2. Loading IO configuration...");
    if (!ioManager.loadConfigFromFile("/config/local_io_example.json")) {
        Serial.println("ERROR: Failed to load IO configuration");
        return;
    }

    // Connect IO Manager to PLC Engine
    Serial.println("3. Connecting IO Manager to PLC Engine...");
    PlcProgram* mainProgram = plcEngine->getProgram("main_program");
    if (mainProgram) {
        PlcMemory& plcMemory = mainProgram->getMemory();

        // This automatically:
        // - Declares all PLC variables based on IO mapping
        // - Enables auto-sync between IO and PLC
        ioManager.setPlcMemory(&plcMemory);

        Serial.println("   -> PLC variables declared from IO mapping");
        Serial.println("   -> Auto-sync enabled");
    }

    // Load PLC program that uses local IO
    Serial.println("4. Loading PLC program...");
    File plcFile = LittleFS.open("/config/plc_with_local_io_example.json", "r");
    if (!plcFile) {
        Serial.println("ERROR: Failed to open PLC program file");
        return;
    }

    String plcConfig = plcFile.readString();
    plcFile.close();

    if (!plcEngine->loadProgram("temperature_control", plcConfig.c_str())) {
        Serial.println("ERROR: Failed to load PLC program");
        return;
    }

    // Start PLC program
    Serial.println("5. Starting PLC program...");
    plcEngine->runProgram("temperature_control");

    Serial.println("\n=== System Running ===");
    Serial.println("IO variables are automatically synchronized with PLC:");
    Serial.println("  - Inputs:  Hardware -> IO Manager -> PLC Variables");
    Serial.println("  - Outputs: PLC Variables -> IO Manager -> Hardware");
    Serial.println("\nMonitor loop will print status every 2 seconds...\n");
}

void loop() {
    static unsigned long lastPrint = 0;

    // Update Local IO Manager
    // This automatically syncs with PLC if auto-sync is enabled
    ioManager.loop();

    // Print status every 2 seconds
    if (millis() - lastPrint >= 2000) {
        lastPrint = millis();

        Serial.println("\n--- System Status ---");

        // Read local IO status
        Serial.printf("Temperature: %.2f°C (Sensor online: %s)\n",
                     ioManager.readAnalog("temp_sensor"),
                     ioManager.readDigital("temp_sensor_online") ? "YES" : "NO");

        Serial.printf("Heater: %s\n",
                     ioManager.readDigital("heater") ? "ON" : "OFF");

        Serial.printf("Alarm LED: %s\n",
                     ioManager.readDigital("alarm_led") ? "ON" : "OFF");

        Serial.printf("Fan Speed: %.1f%%\n",
                     ioManager.readAnalog("fan_speed"));

        Serial.printf("Ack Button: %s\n",
                     ioManager.readDigital("ack_button") ? "PRESSED" : "RELEASED");

        // PLC variables are automatically synchronized, so they reflect IO state
        PlcProgram* program = plcEngine->getProgram("temperature_control");
        if (program) {
            PlcMemory& mem = program->getMemory();

            Serial.println("\nPLC Variables (auto-synced from IO):");
            Serial.printf("  AI.Temperature: %.2f\n", mem.getValue<float>("AI.Temperature", 0.0f));
            Serial.printf("  DI.TempSensorOnline: %s\n",
                         mem.getValue<bool>("DI.TempSensorOnline", false) ? "true" : "false");
            Serial.printf("  DO.HeaterEnable: %s\n",
                         mem.getValue<bool>("DO.HeaterEnable", false) ? "true" : "false");
            Serial.printf("  AO.FanSpeed: %.2f\n", mem.getValue<float>("AO.FanSpeed", 0.0f));
        }

        Serial.println("--------------------");
    }

    delay(10); // 10ms cycle time for IO scanning
}

/**
 * Example Configuration Files:
 *
 * 1. /config/local_io_example.json - Defines hardware IO pins and PLC mapping
 * 2. /config/plc_with_local_io_example.json - PLC program using IO variables
 *
 * Key Concepts:
 *
 * 1. IO Declaration:
 *    LocalIOManager reads IO configuration and creates pin objects
 *
 * 2. PLC Variable Declaration:
 *    When setPlcMemory() is called, LocalIOManager automatically declares
 *    PLC variables based on the plc_mapping configuration:
 *    - Input pins -> PLC input variables (DI.*, AI.*)
 *    - Output pins -> PLC output variables (DO.*, AO.*)
 *
 * 3. Automatic Synchronization:
 *    Each loop() call:
 *    - Updates all IO pins (reads inputs, updates PWM, etc.)
 *    - Syncs inputs: Hardware -> PLC variables
 *    - Syncs outputs: PLC variables -> Hardware
 *
 * 4. PLC Logic:
 *    PLC program can read/write these variables just like mesh endpoints:
 *    - Read: temp = AI.Temperature
 *    - Write: DO.HeaterEnable = true
 *
 * Benefits:
 * - Seamless integration between hardware IO and PLC logic
 * - No manual synchronization code needed
 * - PLC programs work identically with local IO and mesh endpoints
 * - Configuration-driven: change pin assignments without code changes
 */
