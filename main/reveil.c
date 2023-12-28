#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c-lcd.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
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
#include <freertos/semphr.h>

static const char *TAG = "REVEIL";

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL      /*!< GPIO number used for I2C master clock 22*/
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA      /*!< GPIO number used for I2C master data  21*/
#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000
int *hours;
int *minutes;

#define EXAMPLE_ESP_WIFI_SSID      ""
#define EXAMPLE_ESP_WIFI_PASS      ""
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

#define TCP_SUCESS 1<<0
#define TCP_FAILURE 1 <<1
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600
//#define DAYLIGHT_OFFSET_SEC 3600

#define LED_PIN 25 //LED
#define BP_4 5 //BP poussoire
#define BUTTON_PIN_4 GPIO_NUM_4


////////////////////////////////////////////////////////////
//WIFI
/* FreeRTOS event group to signal when we are connected, contain status informations*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
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
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
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

    // LCD
    lcd_clear();
    lcd_put_cur(0, 0);

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    localtime_r(&now, &timeinfo);
    int seconde_time = timeinfo.tm_sec;
    if (seconde_time != 0){
        vTaskDelay((60000-(seconde_time*1000)) / portTICK_PERIOD_MS);  // Délai de 60-seconde
    }

    // Get the current time avec seconde à 00
    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(strftime_buf, sizeof(strftime_buf), "%A %d %B %Y, Week %U, %H:%M:%S", &timeinfo); // a afficher sur le lcd
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);

    // Format the date string
    char date_buffer[64];
    strftime(date_buffer, sizeof(date_buffer), "%A %d/%m/%Y", &timeinfo);

    lcd_send_string(date_buffer);

    // Format the time string
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "Week %U, %H:%M", &timeinfo);

    lcd_put_cur(1, 0);
    lcd_send_string(time_buffer);

    //Console
    ESP_LOGI(TAG, "Date: %s", date_buffer);
    ESP_LOGI(TAG, "Time: %s", time_buffer);
    }


void obtain_time_task(void *pvParameters) {
    while (1) {
        obtain_time();
        vTaskDelay(3600000 / portTICK_PERIOD_MS);  // Attendre 60 minute avant la prochaine synchronisation
    }
}

void display_time_on_monitor_task(void *pvParameters) {
    while (1) {
        display_time_on_monitor();
        vTaskDelay(60000 / portTICK_PERIOD_MS);  // Attendre 1 minute entre les affichages
    }
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
//LCD
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_NUM_0;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}


void display_time_on_lcd(void)
{
    
    lcd_clear();
    lcd_put_cur(0, 0);

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    localtime_r(&now, &timeinfo);


    // Get the current time avec seconde à 00
    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(strftime_buf, sizeof(strftime_buf), "%A %d %B %Y, Week %U, %H:%M:%S", &timeinfo); // a afficher sur le lcd
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);

    // Format the date string
    char date_buffer[64];
    strftime(date_buffer, sizeof(date_buffer), "%A %d %B %Y", &timeinfo);

    lcd_send_string(date_buffer);

    // Determine the size of the buffer needed
    int buffer_size = snprintf(NULL, 0, "%02dh %02dmin", *hours, *minutes);
    // Allocate a buffer of the required size plus one for the null-terminating character
    char buffer[buffer_size +1];

    // Format the string into the buffer
    snprintf(buffer, sizeof(buffer), "%02dh %02dmin", *hours, *minutes);


    // Format the time string
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "Week %U, %H:%M", &timeinfo);

    lcd_put_cur(1, 0);
    lcd_send_string(time_buffer);
    ESP_LOGI(TAG, "ecriture sur LCD");  
}

void time_display_task(void *pvParameters)
{
    while (1)
    {    
        display_time_on_lcd();
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Update every 5 seconds
        //(*minutes) += 1;
    }
}
/////////////////////////////////////////////////////
//Btn poussoir
//défini alarme
// Broches du bouton poussoir et de la LED
#define BOUTON_PIN_1 GPIO_NUM_2 
#define BOUTON_PIN_2 GPIO_NUM_4  
#define BOUTON_PIN_3 GPIO_NUM_5  

void init_gpio() {
    gpio_config_t bouton_config_1 = {
        .pin_bit_mask = (1ULL << BOUTON_PIN_1),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    gpio_config(&bouton_config_1);

        gpio_config_t bouton_config_2 = {
        .pin_bit_mask = (1ULL << BOUTON_PIN_2),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    gpio_config(&bouton_config_2);

        gpio_config_t bouton_config_3 = {
        .pin_bit_mask = (1ULL << BOUTON_PIN_3),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_ANYEDGE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };

    gpio_config(&bouton_config_3);
}

void initialize_time_variables(void)
{
    // Allocate memory for hours and minutes
    hours = (int*)malloc(sizeof(int));
    minutes = (int*)malloc(sizeof(int));

    if (hours == NULL || minutes == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        // Handle memory allocation failure as needed
    }

    // Initialize values
    *hours = 00;
    *minutes = 00;
}

void update_time(int new_hours, int new_minutes)
{
    // Update the time values
    *hours = new_hours;
    *minutes = new_minutes;
}

void cleanup_time_variables(void)
{
    // Free allocated memory
    free(hours);
    free(minutes);
}

void bouton_alarme(void) {
    int etatBouton = gpio_get_level(BOUTON_PIN_1);
    int etatBouton_2 = gpio_get_level(BOUTON_PIN_2);
    int cpt = 2;
    if (etatBouton == 0){ //declenche setting
        cpt = 0;
        ESP_LOGI(TAG, "compteur: %d", cpt); 
        vTaskDelay(pdMS_TO_TICKS(1000)); // Attendre un court moment pour éviter les rebonds du bouton       
    }

    while (cpt <2) {

        etatBouton = gpio_get_level(BOUTON_PIN_1);
        etatBouton_2 = gpio_get_level(BOUTON_PIN_2);
        if(cpt ==0 && etatBouton_2 == 0){ //augmente heure
            *hours += 1;
            if(*hours >24){
                *hours = 00;
            }
            char alarme_txt[64];
            snprintf(alarme_txt, sizeof(alarme_txt), "%dh %dmin", *hours, *minutes);
            ESP_LOGI(TAG, "alarme: %s", alarme_txt); // Log the buffer content
            vTaskDelay(pdMS_TO_TICKS(50)); // Attendre un court moment pour éviter les rebonds du bouton

        }else if(cpt == 1 && etatBouton_2 ==0){ //augmente minutes
            *minutes += 1;
            if (*minutes >60){
                *minutes = 00; //ne met pas vraiment 00 mais 0
            }
            char alarme_txt[64];
            snprintf(alarme_txt, sizeof(alarme_txt), "%dh %dmin", *hours, *minutes);
            ESP_LOGI(TAG, "alarme: %s", alarme_txt);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if(etatBouton == 0){ //change
            cpt +=1;
            ESP_LOGI(TAG, "compteur: %d", cpt);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void bouton_task(void *pvParameters)
{
    while (1)
    {    
        bouton_alarme();
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Update every 1 seconds
    }
}

/////////////////////////////////////////////////////
#define LED_PIN 25 //LED
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
void led_fade_task(void *pvParameters) {
    uint8_t brightness = 0;
    int8_t fade_direction = 1;  // 1 pour augmenter la luminosité, -1 pour diminuer

    while (1) {
        

        // on connait l'heure et la minute et la seconde, on veut lorsque on arrive à l'Heure aatendu brighness = 255
        //on prend l'Heure moins 255/60=4.25;4minutes et on augmente de +1 toutes les secondes

        time_t now;
        struct tm timeinfo;
        char strftime_buf[64];
        time(&now);
        localtime_r(&now, &timeinfo);

        strftime(strftime_buf, sizeof(strftime_buf), "%H:%M:%S", &timeinfo); // a afficher sur le lcd
        int seconde_time = timeinfo.tm_sec;
        int minute_time = timeinfo.tm_min;
        int hour_time = timeinfo.tm_hour;
        
        // Convertir l'heure attendue en secondes pour faciliter la comparaison
        int expected_time_in_seconds_before = (*hours * 3600) + ((*minutes - 4) * 60);
        int expected_time_in_seconds_after = (*hours * 3600) + ((*minutes + 1) * 60);
        int actual_time_in_sec = (hour_time * 3600 + minute_time * 60 + seconde_time);

        // Vérifier si l'heure actuelle est dans la plage souhaitée
        if (actual_time_in_sec >= expected_time_in_seconds_before && actual_time_in_sec <= expected_time_in_seconds_after) {
        
            // Augmenter ou diminuer la luminosité
            set_led_brightness(brightness);
            brightness += fade_direction;

            // Changer la direction si la luminosité atteint ses limites
            if (brightness == 255 || brightness == 0) {
                fade_direction = -fade_direction;
            }

            vTaskDelay(10000 / portTICK_PERIOD_MS);  // Délai pour la transition en douceur 60000 = 1m
        }else{
            if (brightness != 0){
                brightness = 0;
                set_led_brightness(brightness);
            }
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }

    }

}
//////////////////////////////////////////////////////////////////

void app_main(void)
{

	//////////////////////////////////////////////////////
	//WIFI
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

    //configuration server http
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server;

    if (httpd_start(&server, &config)==ESP_OK){
        httpd_register_uri_handler(server, &root);
    }

    // Configuration de les broches (25) en mode analogique pour la LED
    configure_ledc();
    // Configuration des boutons
    init_gpio();

    //////////////////////////////////////////////////////
    //initialise LCD
    ESP_LOGI(TAG, "initialized alarm");
    initialize_time_variables();
    update_time(12,25);
    ESP_ERROR_CHECK(i2c_master_init());

    ESP_LOGI(TAG, "I2C initialized");
    lcd_init();
    lcd_clear();
    ESP_LOGI(TAG, "Fin initialized LCD");    

    // Créer la tâche pour l'obtention du temps
    xTaskCreate(obtain_time_task, "ObtainTimeTask", 4096, NULL, 1, NULL);

    //Afficher heure sur LCD
    //xTaskCreate(time_display_task, "time_display_task", 4096, NULL, 3, NULL);

    //Tache BTN
    xTaskCreate(bouton_task, "alarme setting", 4096, NULL, 2, NULL);

    // Créer la tâche pour l'affichage du temps sur le moniteur et LCD
    xTaskCreate(display_time_on_monitor_task, "DisplayTimeTask", 4096, NULL, 3, NULL);

    //Tache LED
    xTaskCreate(led_fade_task, "ledc_fade_task", 4096, NULL, 4, NULL);



	//////////////////////////////////////////////////////
    /*
    
    // Add cleanup logic as needed
    // cleanup_time_variables();
    //////////////////////////////////////////////////////
    */
}
