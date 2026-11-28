#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef const char *esp_event_base_t;
typedef void *esp_mqtt_client_handle_t;
typedef enum {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_CONNECTED = 1,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_ERROR,
} esp_mqtt_event_id_t;
enum { MQTT_ERROR_TYPE_TCP_TRANSPORT = 1 };
typedef struct {
    int error_type;
    int esp_tls_last_esp_err;
    int esp_tls_stack_err;
    int esp_transport_sock_errno;
    int connect_return_code;
} esp_mqtt_error_codes_t;
typedef void (*esp_event_handler_t)(void *, esp_event_base_t, int32_t, void *);
typedef struct {
    char *topic;
    int topic_len;
    char *data;
    int data_len;
    int total_data_len;
    int current_data_offset;
    esp_mqtt_error_codes_t *error_handle;
} esp_mqtt_event_t;
typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
enum { MQTT_TRANSPORT_OVER_SSL = 1 };
typedef struct {
    struct {
        struct { const char *hostname; int transport; int port; } address;
        struct { int (*crt_bundle_attach)(void *); } verification;
    } broker;
    struct {
        const char *username;
        const char *client_id;
        struct { const char *password; } authentication;
    } credentials;
    struct { int size; int out_size; } buffer;
    struct { int keepalive; } session;
} esp_mqtt_client_config_t;
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event,
                                       esp_event_handler_t handler, void *argument);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data,
                            int length, int qos, int retain);
