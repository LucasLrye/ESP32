#include <stdio.h>
#include "esp_log.h"

#include <string.h>
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"


/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Perso
#define EXAMPLE_ESP_WIFI_SSID      "freebox_BXHVPT"
#define EXAMPLE_ESP_WIFI_PASS      "*****"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5


void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data);

esp_err_t wifi_init_sta();
