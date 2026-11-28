// Actual app_mqtt.c with deterministic transport/config fixtures; no network.
#include "app_mqtt.h"
#include "app_config.h"
#include "app_wifi.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "mbedtls/ssl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int machine_count;
static unsigned sent;
static unsigned subscriptions;
static unsigned version_requests;
static unsigned sent_by_machine[4];
static unsigned char client_token;
static unsigned char mutex_token;
static esp_event_handler_t event_handler;
static void *event_argument;
static esp_timer_create_args_t timer_args;
static uint64_t timer_period;
static int64_t clock_us = 1000000;
static char last_client_id[32];
static bool connect_during_start;
static bool ca_bundle_attached;
static unsigned char requested_fragment;
struct mbedtls_ssl_config { unsigned char fixture; };
static mbedtls_ssl_config tls_config;

void host_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
uint32_t esp_get_free_heap_size(void) { return 65536; }
size_t heap_caps_get_largest_free_block(uint32_t caps) { (void)caps; return 32768; }
bool app_wifi_connected(void) { return true; }
int app_wifi_rssi(void) { return -49; }
int esp_crt_bundle_attach(void *configuration)
{
    assert(configuration == &tls_config);
    ca_bundle_attached = true;
    return ESP_OK;
}
int mbedtls_ssl_conf_max_frag_len(mbedtls_ssl_config *configuration, unsigned char length)
{
    assert(configuration == &tls_config && ca_bundle_attached);
    requested_fragment = length;
    return 0;
}
int64_t esp_timer_get_time(void) { return clock_us; }
uint32_t esp_random(void)
{
    static uint32_t next = 0x819ce6dc;
    return ++next;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &mutex_token; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t lock, uint32_t timeout)
{ assert(lock == &mutex_token); (void)timeout; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t lock)
{ assert(lock == &mutex_token); return pdTRUE; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *timer)
{ timer_args = *args; *timer = &timer_args; return ESP_OK; }
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us)
{ assert(timer == &timer_args); timer_period = period_us; return ESP_OK; }

bool config_load(device_config_t *config)
{
    memset(config, 0, sizeof(*config));
    strcpy(config->bambu_uid, "123");
    strcpy(config->bambu_token, "host-fixture-token");
    for (int i = 0; i < machine_count; i++) {
        snprintf(config->bambu_sn[i], sizeof(config->bambu_sn[i]), "CORE-HOST-%d", i + 1);
        snprintf(config->bambu_name[i], sizeof(config->bambu_name[i]), "Fixture%d", i + 1);
    }
    return true;
}

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config)
{
    assert(config->broker.address.transport == MQTT_TRANSPORT_OVER_SSL);
    assert(config->broker.address.port == 8883);
    assert(config->buffer.size == 13 * 1024);
    ca_bundle_attached = false;
    requested_fragment = 0;
    assert(config->broker.verification.crt_bundle_attach(&tls_config) == ESP_OK);
    assert(ca_bundle_attached);
    if (requested_fragment != MBEDTLS_SSL_MAX_FRAG_LEN_4096) {
        fputs("Cloud TLS connects without requesting the broker-supported 4096-byte fragment limit\n", stderr);
        exit(1);
    }
    assert(!strcmp(config->credentials.username, "u_123"));
    assert(!strcmp(config->credentials.authentication.password, "host-fixture-token"));
    assert(strncmp(config->credentials.client_id, "bbl_", 4) == 0);
    assert(strcmp(last_client_id, config->credentials.client_id) != 0 &&
           "Different devices starting at the same timer value must not evict each other");
    snprintf(last_client_id, sizeof(last_client_id), "%s", config->credentials.client_id);
    return &client_token;
}
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event,
                                       esp_event_handler_t handler, void *argument)
{
    assert(client == &client_token && event == MQTT_EVENT_ANY);
    event_handler = handler;
    event_argument = argument;
    return ESP_OK;
}
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client)
{
    assert(client == &client_token);
    if (connect_during_start) event_handler(event_argument, "host-mqtt", MQTT_EVENT_CONNECTED, NULL);
    return ESP_OK;
}

static int topic_machine(const char *topic, const char *suffix)
{
    for (int i = 0; i < machine_count; i++) {
        char expected[64];
        snprintf(expected, sizeof(expected), "device/CORE-HOST-%d/%s", i + 1, suffix);
        if (!strcmp(topic, expected)) return i;
    }
    assert(!"Unexpected MQTT topic");
    return -1;
}
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos)
{
    assert(client == &client_token && qos == 0);
    topic_machine(topic, "report");
    return (int)++subscriptions;
}
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data,
                            int length, int qos, int retain)
{
    assert(client == &client_token && length == 0 && qos == 0 && retain == 0);
    int index = topic_machine(topic, "request");
    cJSON *root = cJSON_Parse(data);
    assert(root && cJSON_IsObject(root) && cJSON_GetArraySize(root) == 1);
    cJSON *info = cJSON_GetObjectItemCaseSensitive(root, "info");
    if (info) {
        assert(cJSON_IsObject(info) && cJSON_GetArraySize(info) == 2);
        cJSON *name = cJSON_GetObjectItemCaseSensitive(info, "command");
        assert(cJSON_IsString(name) && !strcmp(name->valuestring, "get_version"));
        cJSON_Delete(root);
        return (int)++version_requests;
    }
    cJSON *pushing = cJSON_GetObjectItemCaseSensitive(root, "pushing");
    assert(cJSON_IsObject(pushing) && cJSON_GetArraySize(pushing) == 1);
    cJSON *command = cJSON_GetObjectItemCaseSensitive(pushing, "command");
    assert(cJSON_IsString(command) && !strcmp(command->valuestring, "pushall"));
    assert(!cJSON_HasObjectItem(root, "print") && !cJSON_HasObjectItem(root, "system"));
    cJSON_Delete(root);
    sent_by_machine[index]++;
    return (int)++sent;
}

static void fire(esp_mqtt_event_id_t event, esp_mqtt_event_t *data)
{ assert(event_handler); event_handler(event_argument, "host-mqtt", event, data); }

static void exercise(int count)
{
    machine_count = count;
    connect_during_start = count == BAMBU_MACHINE_MAX;
    sent = subscriptions = version_requests = 0;
    memset(sent_by_machine, 0, sizeof(sent_by_machine));
    app_mqtt_start();
    assert(app_mqtt_machine_count() == count);
    assert(timer_period == 30000000ULL);
    if (!connect_during_start) {
        assert(sent == 0);
        fire(MQTT_EVENT_CONNECTED, NULL);
    }
    assert(subscriptions == (unsigned)count && version_requests == (unsigned)count && sent == (unsigned)count);
    bambu_state_t state;
    app_mqtt_snapshot(0, &state);
    assert(state.cloud == BAMBU_CLOUD_ONLINE);
    for (int i = 0; i < count; i++) app_mqtt_request_now(i);
    assert(sent == (unsigned)(count * 2));
    app_mqtt_request_now(-1);
    app_mqtt_request_now(count);
    app_mqtt_request_now(999);
    assert(sent == (unsigned)(count * 2));
    timer_args.callback(timer_args.arg);
    assert(sent == (unsigned)(count * 3));
    for (int i = 0; i < count; i++) assert(sent_by_machine[i] == 3);

    char topic[] = "device/CORE-HOST-1/report";
    char payload[] = "{\"print\":{\"gcode_state\":\"RUNNING\",\"mc_percent\":37,\"mc_remaining_time\":21,\"nozzle_temper\":210,\"bed_temper\":60,\"vir_slot\":[{\"id\":254,\"tray_type\":\"PLA\",\"tray_sub_brands\":\"Glow\",\"tray_color\":\"CCDDEEFF\",\"remain\":45}]}}";
    esp_mqtt_event_t data = {0};
    data.topic = topic;
    data.topic_len = (int)strlen(topic);
    data.data = payload;
    data.data_len = data.total_data_len = (int)strlen(payload);
    fire(MQTT_EVENT_DATA, &data);
    app_mqtt_snapshot(0, &state);
    assert(!strcmp(state.gcode_state, "RUNNING") && state.pct == 37);
    assert(state.nozzle_t == 210 && state.bed_t == 60 && state.remain_min == 21);
    assert(state.external_trays[0].pct_valid && state.external_trays[0].tray_pct == 45);
    assert(sent == (unsigned)(count * 3));
    if (count > 1) {
        app_mqtt_snapshot(1, &state);
        assert(state.pct == 0 && !state.external_trays[0].is_used);
    }
    fire(MQTT_EVENT_DISCONNECTED, NULL);
    app_mqtt_snapshot(0, &state);
    assert(state.cloud == BAMBU_CLOUD_CONNECTING);
    assert(!strcmp(state.external_trays[0].tray_name, "Glow"));
    assert(state.external_trays[0].tray_color == 0xccddee && !state.external_trays[0].pct_valid);
    assert(state.external_pending_mask == 1);
    app_mqtt_request_now(0);
    timer_args.callback(timer_args.arg);
    assert(sent == (unsigned)(count * 3));
    fire(MQTT_EVENT_CONNECTED, NULL);
    assert(sent == (unsigned)(count * 4) && subscriptions == (unsigned)(count * 2));
    app_mqtt_snapshot(0, &state);
    assert(state.cloud == BAMBU_CLOUD_ONLINE && state.external_pending_mask == 1 && !state.state_received);
    assert(!strcmp(state.external_trays[0].tray_name, "Glow"));
    char updated[] = "{\"print\":{\"vir_slot\":[{\"id\":254,\"tray_type\":\"TPU\",\"tray_color\":\"112233FF\",\"remain\":17}]}}";
    data.data = updated;
    data.data_len = data.total_data_len = (int)strlen(updated);
    fire(MQTT_EVENT_DATA, &data);
    app_mqtt_snapshot(0, &state);
    assert(!state.external_pending_mask && !strcmp(state.external_trays[0].tray_type, "TPU"));
    assert(state.external_trays[0].tray_color == 0x112233 && state.external_trays[0].tray_pct == 17);
    assert(version_requests == (unsigned)(count * 2));
    printf("PASS: %d printers, read-only requests and spool identity across reconnect\n", count);
}

int main(void)
{
    app_mqtt_request_now(0);
    assert(sent == 0);
    for (int count = 1; count <= 4; count++) exercise(count);
    return 0;
}
