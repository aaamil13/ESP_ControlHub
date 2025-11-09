#ifndef ESP_HUB_PROTOCOL_H
#define ESP_HUB_PROTOCOL_H

// Define message types
typedef enum {
    MESSAGE_TYPE_REGISTRATION,
    MESSAGE_TYPE_DATA
} message_type_t;

// Structure for registration messages
typedef struct {
    message_type_t type;
    uint8_t id; // Unique ID for the device
} registration_message_t;

// Structure for data messages
typedef struct {
    message_type_t type;
    uint8_t id;
    float value;
} data_message_t;

#endif // ESP_HUB_PROTOCOL_H