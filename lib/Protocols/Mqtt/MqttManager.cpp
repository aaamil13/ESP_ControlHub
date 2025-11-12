#include "MqttManager.h"
#include <LittleFS.h>
#include <WiFiClientSecure.h>
#include "../../Core/StreamLogger.h"

// Define LITTLEFS as an alias for LittleFS if not already defined
#ifndef LITTLEFS
#define LITTLEFS LittleFS
#endif

extern StreamLogger* EspHubLog;

MqttManager::MqttManager()
    : mqttClient(wifiClient),
      wifiClientSecure(nullptr),
      state(ModuleState::MODULE_DISABLED),
      statusMessage("Not initialized"),
      startTime(0),
      serverPort(1883),
      useTls(false),
      messagesPublished(0),
      messagesReceived(0),
      connectionAttempts(0),
      successfulConnections(0),
      failedConnections(0) {
}

MqttManager::~MqttManager() {
    stop();
    if (wifiClientSecure) {
        delete wifiClientSecure;
        wifiClientSecure = nullptr;
    }
}

// ============================================================================
// Module Interface Implementation
// ============================================================================

bool MqttManager::initialize() {
    if (state != ModuleState::MODULE_DISABLED) {
        return true;  // Already initialized
    }

    EspHubLog->println("Initializing MQTT module");

    state = ModuleState::MODULE_ENABLED;
    statusMessage = "Initialized";
    lastError = "";

    return true;
}

bool MqttManager::start() {
    if (state == ModuleState::MODULE_RUNNING) {
        return true;  // Already running
    }

    if (serverAddress.isEmpty()) {
        lastError = "Server address not configured";
        EspHubLog->println(lastError);
        state = ModuleState::MODULE_ERROR;
        return false;
    }

    // EspHubLog->log(LogLevel::INFO, "MqttManager",
    //                String("Starting MQTT module - Server: ") + serverAddress + ":" + String(serverPort));

    state = ModuleState::MODULE_STARTING;
    statusMessage = "Starting";

    // Setup MQTT client
    if (useTls) {
        if (!loadCertificates()) {
            lastError = "Failed to load certificates";
            state = ModuleState::MODULE_ERROR;
            return false;
        }
        mqttClient.setClient(*wifiClientSecure);
    } else {
        mqttClient.setClient(wifiClient);
    }

    mqttClient.setServer(serverAddress.c_str(), serverPort);

    state = ModuleState::MODULE_RUNNING;
    statusMessage = "Running";
    startTime = millis();

    EspHubLog->println("MQTT module started successfully");

    return true;
}

bool MqttManager::stop() {
    if (state == ModuleState::MODULE_DISABLED) {
        return true;  // Already stopped
    }

    EspHubLog->println("Stopping MQTT module");

    state = ModuleState::MODULE_STOPPING;
    statusMessage = "Stopping";

    // Disconnect MQTT client
    if (mqttClient.connected()) {
        mqttClient.publish("esphub/status", "offline");
        mqttClient.disconnect();
    }

    state = ModuleState::MODULE_DISABLED;
    statusMessage = "Stopped";

    return true;
}

void MqttManager::loop() {
    if (state != ModuleState::MODULE_RUNNING) {
        return;
    }

    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();
}

String MqttManager::getName() const {
    return "mqtt";
}

String MqttManager::getDisplayName() const {
    return "MQTT Protocol";
}

String MqttManager::getVersion() const {
    return "1.0.0";
}

ModuleType MqttManager::getType() const {
    return ModuleType::PROTOCOL;
}

ModuleCapabilities MqttManager::getCapabilities() const {
    ModuleCapabilities caps;
    caps.canDisable = true;
    caps.requiresReboot = false;
    caps.hasWebUI = true;
    caps.hasSecurity = useTls;
    caps.estimatedMemory = useTls ? 15360 : 8192;  // 15KB with TLS, 8KB without
    caps.dependencies = {};  // No dependencies
    caps.hardwareRequirement = "";  // Works on all ESP32 variants

    return caps;
}

String MqttManager::getDescription() const {
    return "MQTT protocol manager for message broker communication. Supports both standard MQTT and MQTTS (TLS encryption).";
}

ModuleState MqttManager::getState() const {
    return state;
}

String MqttManager::getStatusMessage() const {
    if (state == ModuleState::MODULE_RUNNING) {
        if (mqttClient.connected()) {
            return String("Connected to ") + serverAddress + ":" + String(serverPort);
        } else {
            return "Running (disconnected)";
        }
    }
    return statusMessage;
}

bool MqttManager::configure(const JsonObject& config) {
    if (!config.containsKey("server")) {
        lastError = "Missing 'server' in configuration";
        return false;
    }

    serverAddress = config["server"].as<String>();
    serverPort = config["port"] | 1883;
    useTls = config["use_tls"] | false;

    if (useTls) {
        caCertPath = config["ca_cert_path"] | "";
        clientCertPath = config["client_cert_path"] | "";
        clientKeyPath = config["client_key_path"] | "";
    }

    // EspHubLog->log(LogLevel::INFO, "MqttManager",
    //                String("Configured - Server: ") + serverAddress + ":" + String(serverPort) +
    //                ", TLS: " + (useTls ? "enabled" : "disabled"));

    return true;
}

JsonDocument MqttManager::getConfig() const {
    JsonDocument doc;

    doc["server"] = serverAddress;
    doc["port"] = serverPort;
    doc["use_tls"] = useTls;

    if (useTls) {
        doc["ca_cert_path"] = caCertPath;
        doc["client_cert_path"] = clientCertPath;
        doc["client_key_path"] = clientKeyPath;
    }

    return doc;
}

bool MqttManager::validateConfig(const JsonObject& config) const {
    // Check required fields
    if (!config.containsKey("server")) {
        return false;
    }

    String server = config["server"];
    if (server.isEmpty()) {
        return false;
    }

    int port = config["port"] | 1883;
    if (port < 1 || port > 65535) {
        return false;
    }

    return true;
}

JsonDocument MqttManager::getStatistics() const {
    JsonDocument doc;

    doc["messages_published"] = messagesPublished;
    doc["messages_received"] = messagesReceived;
    doc["connection_attempts"] = connectionAttempts;
    doc["successful_connections"] = successfulConnections;
    doc["failed_connections"] = failedConnections;
    doc["currently_connected"] = mqttClient.connected();
    doc["uptime_ms"] = getUptime();

    return doc;
}

size_t MqttManager::getMemoryUsage() const {
    return getCapabilities().estimatedMemory;
}

unsigned long MqttManager::getUptime() const {
    if (state == ModuleState::MODULE_RUNNING && startTime > 0) {
        return millis() - startTime;
    }
    return 0;
}

bool MqttManager::healthCheck() const {
    if (state != ModuleState::MODULE_RUNNING) {
        return false;
    }

    // Check if we're connected or at least trying to connect
    return true;  // Module is running, connection status is reported separately
}

String MqttManager::getLastError() const {
    return lastError;
}

// ============================================================================
// Legacy Interface (Backward Compatibility)
// ============================================================================

void MqttManager::begin(const char* server, int port, bool use_tls, const char* ca_cert_path, const char* client_cert_path, const char* client_key_path) {
    // Store configuration
    serverAddress = server;
    serverPort = port;
    useTls = use_tls;
    caCertPath = ca_cert_path;
    clientCertPath = client_cert_path;
    clientKeyPath = client_key_path;

    if (strlen(server) == 0) {
        EspHubLog->println("WARNING: MQTT server not configured. MQTT client will not connect.");
        return;
    }

    // Initialize and start the module using the new interface
    initialize();
    start();
}

void MqttManager::setCallback(MQTT_CALLBACK_SIGNATURE) {
    mqttClient.setCallback(callback);
}

void MqttManager::publish(const char* topic, const char* payload) {
    if (mqttClient.publish(topic, payload)) {
        messagesPublished++;
    }
}

void MqttManager::subscribe(const char* topic) {
    mqttClient.subscribe(topic);
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

void MqttManager::reconnect() {
    static unsigned long lastReconnectAttempt = 0;
    static unsigned long reconnectInterval = 1000; // Start with 1 second

    // Loop until we're reconnected
    while (!mqttClient.connected() && state == ModuleState::MODULE_RUNNING) {
        unsigned long currentMillis = millis();
        if (currentMillis - lastReconnectAttempt > reconnectInterval) {
            lastReconnectAttempt = currentMillis;
            connectionAttempts++;

            EspHubLog->print("Attempting MQTT connection...");

            // Attempt to connect
            bool connected;
            if (useTls) {
                connected = mqttClient.connect("EspHubClient-TLS");
            } else {
                connected = mqttClient.connect("EspHubClient");
            }

            if (connected) {
                EspHubLog->println("connected");
                successfulConnections++;

                // Once connected, publish an announcement...
                mqttClient.publish("esphub/status", "online");
                // ... and resubscribe
                subscribe("esphub/config/plc");
                subscribe("esphub/plc/control");

                reconnectInterval = 1000; // Reset interval on success
                statusMessage = String("Connected to ") + serverAddress + ":" + String(serverPort);
            } else {
                failedConnections++;
                lastError = String("Connection failed, rc=") + String(mqttClient.state());

                EspHubLog->print("failed, rc=");
                EspHubLog->print(mqttClient.state());
                EspHubLog->print(" retrying in ");
                EspHubLog->print(reconnectInterval / 1000);
                EspHubLog->println(" seconds");

                // Increase interval exponentially, up to a limit
                reconnectInterval *= 2;
                if (reconnectInterval > 60000) reconnectInterval = 60000; // Max 1 minute
            }
        }
        // Allow other tasks to run
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

bool MqttManager::loadCertificates() {
    if (!wifiClientSecure) {
        wifiClientSecure = new WiFiClientSecure();
    }

    // Load CA certificate
    if (!caCertPath.isEmpty()) {
        File ca = LITTLEFS.open(caCertPath.c_str(), "r");
        if (ca) {
            String certStr = ca.readString();
            wifiClientSecure->setCACert(certStr.c_str());
            ca.close();
            // EspHubLog->log(LogLevel::INFO, "MqttManager",
            //              String("Loaded CA cert from ") + caCertPath);
        } else {
            lastError = String("Failed to open CA cert file: ") + caCertPath;
            EspHubLog->println(lastError);
            return false;
        }
    }

    // Load client certificate and key
    if (!clientCertPath.isEmpty() && !clientKeyPath.isEmpty()) {
        File clientCert = LITTLEFS.open(clientCertPath.c_str(), "r");
        File clientKey = LITTLEFS.open(clientKeyPath.c_str(), "r");

        if (clientCert && clientKey) {
            String certStr = clientCert.readString();
            String keyStr = clientKey.readString();

            wifiClientSecure->setCertificate(certStr.c_str());
            wifiClientSecure->setPrivateKey(keyStr.c_str());

            clientCert.close();
            clientKey.close();

            // EspHubLog->log(LogLevel::INFO, "MqttManager",
            //              String("Loaded client cert from ") + clientCertPath);
        } else {
            lastError = String("Failed to open client cert/key files: ") +
                       clientCertPath + ", " + clientKeyPath;
            EspHubLog->println(lastError);
            return false;
        }
    }

    return true;
}