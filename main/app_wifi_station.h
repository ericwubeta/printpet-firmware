#pragma once
#include <string.h>
#include "esp_wifi.h"

// A saved SSID may have several mesh/extender APs. Compare them before
// associating instead of stopping at the first matching beacon.
static inline void app_wifi_station_config(wifi_config_t *config, const char *ssid, const char *password)
{
    memset(config, 0, sizeof(*config));
    strlcpy((char *)config->sta.ssid, ssid, sizeof(config->sta.ssid));
    strlcpy((char *)config->sta.password, password, sizeof(config->sta.password));
    config->sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    config->sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
}
