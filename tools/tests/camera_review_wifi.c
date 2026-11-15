// Exercise the actual driver-configuration boundary for saved and provisioned Wi-Fi.
#include "camera_review_common.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define WIFI_IF_STA 0
#define WIFI_FAST_SCAN 0
#define WIFI_ALL_CHANNEL_SCAN 1
#define WIFI_CONNECT_AP_BY_SIGNAL 0
#define CFG_WIFI_MAX 3
typedef struct { struct { uint8_t ssid[32],password[64]; int scan_method,sort_method; } sta; } wifi_config_t;
static char s_ssid[CFG_WIFI_MAX][33]={"Mesh network"};
static char s_pass[CFG_WIFI_MAX][65]={"synthetic-password"};
static int s_nets=1,s_cur;
static int sets,connects,disconnects,paused_ms;
static wifi_config_t observed;
static esp_err_t esp_wifi_set_config(int interface,const wifi_config_t *config) { assert(interface==WIFI_IF_STA);observed=*config;sets++;return ESP_OK; }
static esp_err_t esp_wifi_connect(void) { connects++;return ESP_OK; }
static esp_err_t esp_wifi_disconnect(void) { disconnects++;return ESP_OK; }
/* @STATION@ */
/* @CONNECT@ */
typedef struct { const char *body; char response[128]; } httpd_req_t;
static int read_body(httpd_req_t *req,char *out,int size) { strlcpy(out,req->body,size);return (int)strlen(req->body); }
static void send_json(httpd_req_t *req,const char *text) { strlcpy(req->response,text,sizeof(req->response)); }
static int wifi_store(const char *ssid,const char *password) { assert(!strcmp(ssid,s_ssid[0]) && !strcmp(password,s_pass[0]));return 0; }
static void app_wifi_pause_reconnect(uint32_t ms) { paused_ms=ms; }
/* @HTTP_HELPERS@ */
/* @JOIN@ */
int main(int argc,char **argv) {
    assert(argc==2);
    if (!strcmp(argv[1],"saved")) connect_current();
    else {
        httpd_req_t request={.body="ssid=Mesh+network&pass=synthetic-password"};
        assert(joinwifi_handler(&request)==ESP_OK && !strcmp(request.response,"{\"ok\":1}"));
        assert(disconnects==1 && paused_ms==20000);
    }
    assert(sets==1 && connects==1);
    assert(!strcmp((const char *)observed.sta.ssid,s_ssid[0]));
    assert(!strcmp((const char *)observed.sta.password,s_pass[0]));
    // Fast scan stops at the first matching AP; this boundary must request
    // an all-channel candidate list ordered by signal for a mesh SSID.
    assert(observed.sta.scan_method==WIFI_ALL_CHANNEL_SCAN);
    assert(observed.sta.sort_method==WIFI_CONNECT_AP_BY_SIGNAL);
    return 0;
}
