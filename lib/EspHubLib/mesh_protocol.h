#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

#include <ArduinoJson.h>

// Define common message types for mesh communication
enum MeshMessageType {
    MESH_MSG_TYPE_UNKNOWN,
    MESH_MSG_TYPE_REGISTRATION,
    MESH_MSG_TYPE_SENSOR_DATA,
    MESH_MSG_TYPE_ACTUATOR_COMMAND,
    MESH_MSG_TYPE_HEARTBEAT
};

// Function to parse a mesh message string into a JsonDocument
// Returns true on success, false on failure
bool parseMeshMessage(const String& msg_str, JsonDocument& doc);

// Function to create a mesh message string from a JsonDocument
// Returns the serialized string
String serializeMeshMessage(const JsonDocument& doc);

#endif // MESH_PROTOCOL_H