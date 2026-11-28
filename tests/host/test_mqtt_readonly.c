// Actual app_mqtt.c with deterministic transport/config fixtures; no network.
#include "app_mqtt.h"
#include "app_config.h"
#include "app_notify.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "mbedtls/ssl.h"
#include "mqtt_client.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int machine_count;
static unsigned sent;
static unsigned subscriptions;
static unsigned sent_by_machine[4];
static unsigned char client_token;
static unsigned char mutex_token;
static esp_event_handler_t event_handler;
static void *event_argument;
static esp_timer_create_args_t timer_args;
static uint64_t timer_period;
static int64_t clock_us = 1000000;
static bool tls_fragment_configured;

void host_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
uint32_t esp_get_free_heap_size(void) { return 65536; }
int esp_crt_bundle_attach(void *configuration) { (void)configuration; return ESP_OK; }
int64_t esp_timer_get_time(void) { return clock_us; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "fixture error"; }
uint32_t esp_random(void) { return 0x123456; }
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type)
{
    static const uint8_t address[] = {0x02, 0, 0, 0, 0, 1};
    assert(type == ESP_MAC_WIFI_STA);
    memcpy(mac, address, sizeof(address));
    return ESP_OK;
}
int mbedtls_ssl_conf_max_frag_len(void *configuration, unsigned char length)
{
    assert(configuration == &client_token && length == MBEDTLS_SSL_MAX_FRAG_LEN_4096);
    tls_fragment_configured = true;
    return 0;
}
void app_notify_trigger(notify_kind_t kind)
{ assert(kind >= NOTIFY_FINISH && kind <= NOTIFY_PAUSE); }
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &mutex_token; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t lock, uint32_t timeout)
{ assert(lock == &mutex_token); (void)timeout; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t lock)
{ assert(lock == &mutex_token); return pdTRUE; }
esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *timer)
{ timer_args = *args; *timer = &timer_args; return ESP_OK; }
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us)
{ assert(timer == &timer_args); timer_period = period_us; return ESP_OK; }

esp_err_t esp_timer_stop(esp_timer_handle_t timer)
{ assert(timer == &timer_args); timer_period = 0; return ESP_OK; }

bool config_load_printers(device_config_t *config)
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
    assert(!strcmp(config->credentials.username, "u_123"));
    assert(!strcmp(config->credentials.client_id, "bbl_020000000001123456"));
    tls_fragment_configured = false;
    assert(config->broker.verification.crt_bundle_attach(&client_token) == ESP_OK);
    assert(tls_fragment_configured);
    assert(!strcmp(config->credentials.authentication.password, "host-fixture-token"));
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
{ assert(client == &client_token); return ESP_OK; }

esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client)
{ assert(client == &client_token); return ESP_OK; }

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

static bambu_state_t report(int index, const char *payload)
{
    char topic[48];
    snprintf(topic, sizeof(topic), "device/CORE-HOST-%d/report", index + 1);
    int len = (int)strlen(payload);
    esp_mqtt_event_t data = {.topic = topic, .topic_len = (int)strlen(topic),
                             .data = (char *)payload, .data_len = len, .total_data_len = len};
    fire(MQTT_EVENT_DATA, &data);
    bambu_state_t state;
    app_mqtt_snapshot(index, &state);
    return state;
}

static void exercise(int count)
{
    machine_count = count;
    sent = subscriptions = 0;
    memset(sent_by_machine, 0, sizeof(sent_by_machine));
    app_mqtt_start();
    assert(app_mqtt_machine_count() == count);
    assert(timer_period == 30000000ULL);
    assert(sent == 0);
    fire(MQTT_EVENT_CONNECTED, NULL);
    assert(subscriptions == (unsigned)count && sent == (unsigned)count);
    for (int i = 0; i < count; i++) app_mqtt_request_now(i);
    assert(sent == (unsigned)(count * 2));
    app_mqtt_request_now(-1);
    app_mqtt_request_now(count);
    app_mqtt_request_now(999);
    assert(sent == (unsigned)(count * 2));
    timer_args.callback(timer_args.arg);
    assert(sent == (unsigned)(count * 3));
    for (int i = 0; i < count; i++) assert(sent_by_machine[i] == 3);

    bambu_state_t state = report(0,
        "{\"print\":{\"gcode_state\":\"RUNNING\",\"mc_percent\":37,\"mc_remaining_time\":21,"
        "\"nozzle_temper\":210,\"bed_temper\":60}}");
    assert(!strcmp(state.gcode_state, "RUNNING") && state.pct == 37);
    assert(state.nozzle_t == 210 && state.bed_t == 60 && state.remain_min == 21);
    assert(sent == (unsigned)(count * 3));
    if (count > 1) { app_mqtt_snapshot(1, &state); assert(state.pct == 0); }
    fire(MQTT_EVENT_DISCONNECTED, NULL);
    app_mqtt_snapshot(0, &state);
    assert(state.cloud == BAMBU_CLOUD_CONNECTING);
    fire(MQTT_EVENT_CONNECTED, NULL);
    assert(sent == (unsigned)(count * 4) && subscriptions == (unsigned)(count * 2));
    app_mqtt_stop();
    assert(timer_period == 0);
    printf("PASS: %d printers, connect/manual/timer/reconnect emit only pushall; telemetry retained\n", count);
}

static void exercise_external_spools(void)
{
    machine_count = 2;
    app_mqtt_start();
    fire(MQTT_EVENT_CONNECTED, NULL);
    unsigned before = sent;

    bambu_state_t st = report(0,
        "{\"print\":{\"vt_tray\":{\"id\":\"255\",\"tray_id_name\":\"Black PLA\",\"tray_color\":\"000000FF\",\"remain\":-1}}}");
    assert(st.ams_count == 0 && st.external_trays[0].is_used && !st.external_trays[0].active);
    assert(!strcmp(st.external_trays[0].tray_name, "Black PLA"));
    assert(st.external_trays[0].color_valid && st.external_trays[0].tray_color == 0);
    assert(st.external_trays[0].tray_pct == -1 && !st.external_trays[1].is_used);

    st = report(0, "{\"print\":{\"ams\":{\"tray_now\":0,\"ams\":[{\"tray\":[{\"tray_type\":\"PETG\"}]}]},"
                   "\"vt_tray\":{\"remain\":\"42\",\"tray_weight\":\"1000\"}}}");
    assert(st.ams_count == 1 && st.ams_active && st.ams[0].tray[0].active);
    assert(st.external_trays[0].tray_remain == 420 && !st.external_trays[0].active);

    st = report(0, "{\"print\":{\"ams\":{\"tray_now\":\"254\"}}}");
    assert(st.ams_count == 1 && !st.ams_active && !st.ams[0].tray[0].active);
    assert(st.external_trays[0].active && st.external_trays[0].tray_remain == 420);

    st = report(0, "{\"print\":{\"vir_slot\":["
                   "{\"id\":\"254\",\"tray_type\":\"PLA\",\"tray_sub_brands\":\"PLA Glow\",\"tray_color\":\"C0DDEEFF\",\"remain\":null},"
                   "{\"id\":\"255\",\"tray_type\":\"\",\"tray_color\":\"000000FF\",\"remain\":-1}],"
                   "\"vt_tray\":{\"tray_type\":\"STALE\"}}}");
    assert(st.ams_count == 1 && st.external_trays[0].active && !st.external_trays[1].is_used);
    assert(!strcmp(st.external_trays[0].tray_name, "PLA Glow") && !strcmp(st.external_trays[0].tray_type, "PLA"));
    assert(st.external_trays[0].tray_color == 0xc0ddee && st.external_trays[0].tray_pct == -1);
    assert(st.external_trays[0].tray_weight == 0);

    st = report(0, "{\"print\":{\"vir_slot\":[{\"id\":65024,\"remain\":\"0\"},{\"id\":\"65280\","
                   "\"tray_type\":\"ASA\",\"tray_color\":\"112233FF\",\"remain\":65}],\"ams\":{\"tray_now\":65280}}}");
    assert(st.external_trays[0].tray_pct == 0 && !strcmp(st.external_trays[0].tray_name, "PLA Glow"));
    assert(!st.external_trays[0].active && st.external_trays[1].active && st.external_trays[1].tray_pct == 65);
    st = report(0, "{\"print\":{\"ams\":{\"tray_now\":255},\"vt_tray\":{\"tray_type\":\"STALE\"},\"mc_percent\":56}}");
    assert(!st.external_trays[0].active && !st.external_trays[1].active);
    assert(!strcmp(st.external_trays[1].tray_type, "ASA") && st.external_trays[1].tray_pct == 65);

    st = report(1, "{\"print\":{\"vir_slot\":[{\"id\":254,\"tray_type\":\"TPU\",\"remain\":-1}]}}");
    assert(st.ams_count == 0 && !st.material_slot_known && st.external_trays[0].is_used);
    assert(!st.external_trays[0].active && !strcmp(st.external_trays[0].tray_type, "TPU"));
    st = report(0, "{\"print\":{\"vir_slot\":[]}}");
    assert(st.ams_count == 1 && !st.external_trays[0].is_used && !st.external_trays[1].is_used);
    app_mqtt_snapshot(1, &st);
    assert(st.external_trays[0].is_used && !strcmp(st.external_trays[0].tray_type, "TPU"));
    assert(sent == before);
    app_mqtt_stop();
    puts("PASS: independent external spools, partial updates, slot selection and printer isolation");
}

int main(void)
{
    app_mqtt_request_now(0);
    assert(sent == 0);
    for (int count = 1; count <= 4; count++) exercise(count);
    exercise_external_spools();
    return 0;
}
