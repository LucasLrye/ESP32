#/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "esp_http_server.h"

#include "time.h"
#include "lwip/apps/sntp.h"

#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      ""
#define EXAMPLE_ESP_WIFI_PASS      ""
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define TCP_SUCESS 1<<0
#define TCP_FAILURE 1 <<1


#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600
//#define DAYLIGHT_OFFSET_SEC 3600
#define LED_PIN 25 //LED

/*Global*/

/* FreeRTOS event group to signal when we are connected, contain status informations*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi_station_home";

static int s_retry_num = 0;


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    	ESP_LOGI(TAG, "Connecting to Ap ...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
       //Utilisé sous forme de fonction pour ip_event_handler
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);   
    }
}

esp_err_t wifi_init_sta()
{
	esp_err_t status = WIFI_FAIL_BIT;
    

//initialize the esp networks
    ESP_ERROR_CHECK(esp_netif_init());
//Dault event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
//Default wifi station in the driver
    esp_netif_create_default_wifi_sta();
//Wifi configuration setyp wifi station
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    //Event loop
    s_wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));

    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));


//Start the Wifi driver
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = **********,
            .pmf_cfg ={
            	.capable = true,
            	.required = false
            },
        },
    };

    //Set the Wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    //Set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    //start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS); //Impossible de cacher le mot de passe..
        status = WIFI_CONNECTED_BIT;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        status = WIFI_FAIL_BIT;
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = WIFI_FAIL_BIT;
    }
    /*The event will not be processed after unregister*/
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    return status;
}
////////////////////////////////////////////////////////////////////////
//connect to the server and return the result
esp_err_t connect_tcp_server(void){
	struct  sockaddr_in serverInfo = {0};
	char readBuffer[1024] = {0};

	//Coté serveur
	serverInfo.sin_family = AF_INET;
	serverInfo.sin_addr.s_addr = inet_addr("192.168.0.107");; //ordi = 192.168.56.1
	serverInfo.sin_port = htons(59900);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock<0){
		ESP_LOGE(TAG, "Failed to create a socket.. ?");
		return TCP_FAILURE;
	}

	//Coté Client de l'ESP32
	struct  sockaddr_in clientInfo = {0};
	clientInfo.sin_family = AF_INET;
	clientInfo.sin_addr.s_addr = INADDR_ANY;
	clientInfo.sin_port = htons(59901);

	if(bind(sock, (struct sockaddr *)&clientInfo, sizeof(clientInfo)) !=0){
		ESP_LOGE(TAG, "Socket bind failed%s!", inet_ntoa(clientInfo.sin_addr.s_addr));
		close(sock);
		return TCP_FAILURE;
	}

	//Serveur
	if(connect(sock, (struct sockaddr *)&serverInfo, sizeof(serverInfo)) !=0){
		ESP_LOGE(TAG, "Failed to connect to %s!", inet_ntoa(serverInfo.sin_addr.s_addr));
		close(sock);
		return TCP_FAILURE;
	}
	
	ESP_LOGI(TAG, "connected to TCP Server.");

	//Recoi un message
	while(1){
		bzero(readBuffer, sizeof(readBuffer));
		int r = read(sock, readBuffer, sizeof(readBuffer)-1);
		/*
		for (int i=0;i<r;i++){
			putchar(readBuffer[i]);
		}
		*/
		if (r>0){
			ESP_LOGI(TAG, "Message reçu: %s", readBuffer);
			if (strcmp(readBuffer, "exit")==0){
				ESP_LOGI(TAG, "Fin de connection par PC");
				break;
				}
        // Attendre jusqu'à ce qu'un nouveau message soit reçu
        	ESP_LOGI(TAG, "Attente d'un nouveau message...");
    }
        /*while (1) {
            vTaskDelay(1000 / portTICK_PERIOD_MS);  // Attendre pendant 1 seconde
            bzero(readBuffer, sizeof(readBuffer));
            int new_r = read(sock, readBuffer, sizeof(readBuffer) - 1);
            if (new_r > 0) {
            	ESP_LOGI(TAG, "Message reçu: %s", readBuffer);
                break;  // Sortir de la boucle si un nouveau message est reçu
            }
      
        }
        */
	}

	/* 
	//Envoi d'un message, fonctionne pas vraiment
	const char *message = "Hello from ESP32!";
	send(socket, message, strlen(message), 0);
	ESP_LOGI(TAG, "Message sent: %s", message);
	*/

	return TCP_SUCESS;
}
////////////////////////////////////////////////////////////////
//HTTP code pour page internet
static esp_err_t on_url_hit(httpd_req_t *req){
    char* resp_str = "ESP32 de Lucas, pour de grand projet";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}


static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = on_url_hit,
    .user_ctx = NULL
};
////////////////////////////////////////////////////////
//Récupère l'heure
void obtain_time(void){


    // Configurer le fuseau horaire pour la France (Paris)
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1);
    
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER);
    sntp_init();
    // wait for time to be set
    struct tm timeinfo;
    time_t now = 0;
    int retry = 0;
    const int retry_count = 10;
    while (retry < retry_count) {
        time(&now);
        localtime_r(&now, &timeinfo);
        if (timeinfo.tm_year >= (2023 - 1900)) {
            break;  // Sortir de la boucle si l'heure est correcte
        }
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        retry++;
    }
    ESP_LOGI(TAG, "System time is set");


}

void display_time_on_monitor(void) {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];

    time(&now);
    localtime_r(&now, &timeinfo);
    int seconde_time = timeinfo.tm_sec;
    if (seconde_time != 0){
        vTaskDelay((60000-(seconde_time*1000)) / portTICK_PERIOD_MS);  // Délai de 60-seconde
    }
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%A %d %B %Y, Week %U, %H:%M:%S", &timeinfo); // a afficher sur le lcd
        ESP_LOGI(TAG, "Current time: %s", strftime_buf);

}

void obtain_time_task(void *pvParameters) {
    while (1) {
        obtain_time();
        vTaskDelay(300000 / portTICK_PERIOD_MS);  // Attendre 5 minute avant la prochaine synchronisation
    }
}

void display_time_on_monitor_task(void *pvParameters) {
    while (1) {
        display_time_on_monitor();
        vTaskDelay(60000 / portTICK_PERIOD_MS);  // Attendre 1 minute entre les affichages
    }
}

/////////////////////////////////////////////////////
//Configuration
void configure_ledc() {
    // Configurer le canal LEDC
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 5000,  // Fréquence PWM (5 kHz)
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num = LED_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,  // Initial duty cycle (0 to 255)
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel);
}

void set_led_brightness(uint8_t brightness) {
    // Configurer la luminosité de la LED en utilisant le canal LEDC
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, brightness);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
}

//Ajustement de la LED
void led_fade_task(void) {
    uint8_t brightness = 0;
    int8_t fade_direction = 1;  // 1 pour augmenter la luminosité, -1 pour diminuer

    while (1) {
        set_led_brightness(brightness);

        // Augmenter ou diminuer la luminosité
        brightness += fade_direction;

        // Changer la direction si la luminosité atteint ses limites
        if (brightness == 255 || brightness == 0) {
            fade_direction = -fade_direction;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);  // Délai pour la transition en douceur
    }
}

////////////////////////////////////////////////////////////
//MAIN
void app_main(void)
{
	esp_err_t status = WIFI_FAIL_BIT;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    //Connected to Wireless AP
    status = wifi_init_sta();
    if (WIFI_CONNECTED_BIT != status){
    	ESP_LOGI(TAG, "Failed to associate Wifi");
    	return;
    }

    //Other connect with socket, action with user
    /*
    status = connect_tcp_server();
    if (TCP_SUCESS != status){
    	ESP_LOGI(TAG, "Failed to connected to remote server, dying...");
    	//return;
    }
    */

    //configuration server http
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server;

    if (httpd_start(&server, &config)==ESP_OK){
        httpd_register_uri_handler(server, &root);
    }

    // Configuration de les broches (25) en mode analogique pour la LED
    configure_ledc();
    //Tache LED
    led_fade_task();

    //Initialize NTP (network time protocol)
    // Créer la tâche pour l'obtention du temps
    obtain_time();
    //xTaskCreate(obtain_time_task, "ObtainTimeTask", 4096, NULL, 1, NULL);

    // Créer la tâche pour l'affichage du temps sur le moniteur
    //xTaskCreate(display_time_on_monitor_task, "DisplayTimeTask", 4096, NULL, 2, NULL);
    while (1) {
        display_time_on_monitor();
        vTaskDelay(60000 / portTICK_PERIOD_MS);  // Attendre 1 minute entre les affichages
    }
    

 
    //xTaskCreate(increaseBrightnessTask, "IncreaseTask", 4096, NULL, 1, NULL);
    

    // Démarrer le planificateur FreeRTOS
    vTaskStartScheduler();






    ESP_LOGI(TAG, "Fin");

}
