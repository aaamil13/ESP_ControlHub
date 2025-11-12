#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include "../../Core/Module.h"

// Forward declaration to avoid including WiFiClientSecure.h in header
class WiFiClientSecure;

class MqttManager : public Module {
public:
    MqttManager();
    ~MqttManager();

    // Legacy interface (kept for backward compatibility)
    void begin(const char* server, int port, bool use_tls = false, const char* ca_cert_path = "", const char* client_cert_path = "", const char* client_key_path = "");
    void setCallback(MQTT_CALLBACK_SIGNATURE);
    void publish(const char* topic, const char* payload);
    void subscribe(const char* topic);

    // Module interface implementation
    bool initialize() override;
    bool start() override;
    bool stop() override;
    void loop() override;

    String getName() const override;
    String getDisplayName() const override;
    String getVersion() const override;
    ModuleType getType() const override;
    ModuleCapabilities getCapabilities() const override;
    String getDescription() const override;

    ModuleState getState() const override;
    String getStatusMessage() const override;

    bool configure(const JsonObject& config) override;
    JsonDocument getConfig() const override;
    bool validateConfig(const JsonObject& config) const override;

    JsonDocument getStatistics() const override;
    size_t getMemoryUsage() const override;
    unsigned long getUptime() const override;

    bool healthCheck() const override;
    String getLastError() const override;

private:
    WiFiClient wifiClient;
    WiFiClientSecure* wifiClientSecure; // For MQTTS (pointer to avoid including header)
    mutable PubSubClient mqttClient;  // mutable to allow connected() in const methods

    // Module state
    ModuleState state;
    String statusMessage;
    String lastError;
    unsigned long startTime;

    // Configuration
    String serverAddress;
    int serverPort;
    bool useTls;
    String caCertPath;
    String clientCertPath;
    String clientKeyPath;

    // Statistics
    unsigned long messagesPublished;
    unsigned long messagesReceived;
    unsigned long connectionAttempts;
    unsigned long successfulConnections;
    unsigned long failedConnections;

    // Internal methods
    void reconnect();
    bool loadCertificates();
};

#endif // MQTT_MANAGER_H