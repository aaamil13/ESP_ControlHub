#include "mesh_protocol.h"
#include "StreamLogger.h" // For Log

extern StreamLogger* Log;

bool parseMeshMessage(const String& msg_str, JsonDocument& doc) {
    DeserializationError error = deserializeJson(doc, msg_str);
    if (error) {
        Log->printf("deserializeJson() failed: %s\n", error.c_str());
        return false;
    }
    return true;
}

String serializeMeshMessage(const JsonDocument& doc) {
    String output;
    serializeJson(doc, output);
    return output;
}