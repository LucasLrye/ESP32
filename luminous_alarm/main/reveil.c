#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c-lcd.h"
#include "wifi.h"
#include "btn_push.h"

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
#include <stdbool.h>

// Tag for LOG message
static const char *TAG = "REVEIL";
// Hour and minutes of the alarm --> good way is to use a struct but flemme
int *hours;
int *minutes;
// Alarm set or NOT, use semaphore there because it is use at different place to know if we have the alarm or not
bool alarm_set = false;
SemaphoreHandle_t alarmset_Mutex;
#define LED_TIME_BEFORE_ALARM 10
#define LED_TIME_AFTER_ALARM 5
/// To know if the LCD is on or off
bool lcd_off = false;
bool first_epoch = true;

#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL      /*!< GPIO number used for I2C master clock 22*/
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA      /*!< GPIO number used for I2C master data  21*/
#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
#define I2C_MASTER_FREQ_HZ          400000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000



#define TCP_SUCESS 1<<0
#define TCP_FAILURE 1 <<1
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600
//#define DAYLIGHT_OFFSET_SEC 3600


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

/// LCD Part
int display_time_on_monitor(void) {


    // test the connexion to the lcd
    if (lcd_put_cur(0, 0) == 1){
        return 1;
    }

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    localtime_r(&now, &timeinfo);
    int seconde_time = timeinfo.tm_sec;
    ESP_LOGI(TAG, "First epoch ?? %i", first_epoch);
    if (seconde_time != 0 && !first_epoch){
    ESP_LOGI(TAG, "Waiting delay %i", first_epoch);
        vTaskDelay((60000-(seconde_time*1000)) / portTICK_PERIOD_MS);  // Délai de 60-seconde
    }else if (first_epoch){
        lcd_init();
    }else{}

    lcd_clear();
    lcd_put_cur(0, 0);
    // Get the current time avec seconde à 00
    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(strftime_buf, sizeof(strftime_buf), "%A %d %B %Y, Week %U, %H:%M:%S", &timeinfo); // a afficher sur le lcd
    ESP_LOGI(TAG, "Current time: %s", strftime_buf);

    // Format the date string
    char date_buffer[64];
    strftime(date_buffer, sizeof(date_buffer), "%A %d/%m/%y", &timeinfo);
    ESP_LOGI(TAG, "check buffer %s", date_buffer);
    lcd_send_string(date_buffer);

    // Format the time string
    char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "wk%U   %H:%M", &timeinfo);
    lcd_put_cur(1, 0);
    ESP_LOGI(TAG, "check buffer time %s", time_buffer);
    lcd_send_string(time_buffer);

    //Console Part
    ESP_LOGI(TAG, "Date: %s", date_buffer);
    ESP_LOGI(TAG, "Time: %s", time_buffer);
    return 0;
    }


void obtain_time_task(void *pvParameters) {
    while (1) {
        obtain_time();
        vTaskDelay(86400000 / portTICK_PERIOD_MS);  // Attendre 24 heure = 3600 minutes avant la prochaine synchronisation
    }
}

void display_time_on_monitor_task(void *pvParameters) {
    while (1) {
        if (lcd_off == 0){
            lcd_off = display_time_on_monitor();
            if (lcd_off == 0 && !first_epoch){
                vTaskDelay(60000 / portTICK_PERIOD_MS);  // Attendre 1 minute entre les affichages
            }else{
                if (first_epoch) first_epoch = false;
                vTaskDelay(1000 / portTICK_PERIOD_MS); // Update every 1 seconds
            }
        }else{
            first_epoch = true;
            lcd_off = lcd_init();
            lcd_off = lcd_clear();
            ESP_LOGI(TAG, "LCD is off, testing each 1s to reconnect");
            vTaskDelay(1000 / portTICK_PERIOD_MS); // Update every 1 seconds
        }
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
//LCD init (autre partie dans affichage pour time)
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_NUM_0;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}
/////////////////////////////////////////////////////
// INIT
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





/////////////////////////////////////////////////////
/// LED
#define LED_PIN 25
#define LED_PIN_1 26

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

    /// LED 0
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

    /// LED 1
    ledc_channel_config_t ledc_channel1 = {
        .gpio_num = LED_PIN_1,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,  // Initial duty cycle (0 to 255)
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_channel1);
}

void set_led_brightness(uint8_t brightness) {
    // Configurer la luminosité de la LED en utilisant le canal LEDC
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, brightness);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, brightness);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1);
}

//Ajustement de la LED
void led_fade_task(void *pvParameters) {
    uint8_t brightness = 0;
    int8_t fade_direction = 1;  // 1 pour augmenter la luminosité, -1 pour diminuer

    while (1) {
        // If alarm is set to true
        xSemaphoreTake(alarmset_Mutex, portMAX_DELAY);
        if (alarm_set) {
            // on connait l'heure et la minute et la seconde, on veut lorsque on arrive à l'Heure aatendu brighness = 255
            //on prend l'Heure moins 255/60=4.25;4minutes et on augmente de +1 toutes les secondes
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            int seconde_time = timeinfo.tm_sec;
            int minute_time = timeinfo.tm_min;
            int hour_time = timeinfo.tm_hour;

            // Convertir l'heure attendue en secondes pour faciliter la comparaison
            int expected_time_in_seconds_before = (*hours * 3600) + ((*minutes - LED_TIME_BEFORE_ALARM) * 60);
            int expected_time_in_seconds_after = (*hours * 3600) + ((*minutes + LED_TIME_AFTER_ALARM) * 60);
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
            }else{
                if (brightness != 0){
                    brightness = 0;
                    set_led_brightness(brightness);
                }
            }

        }else{ // alarm set to deactivated
            if (brightness != 0){
                    brightness = 0;
                    set_led_brightness(brightness);
                }
        }
        xSemaphoreGive(alarmset_Mutex);
        vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay of 10s
    }
}
//////////////////////////////////////////////////////////////////

void app_main(void) {
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
    }else{
        //configuration server http
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        httpd_handle_t server;

        if (httpd_start(&server, &config)==ESP_OK){
            httpd_register_uri_handler(server, &root);
        }
    }



    // Configuration de les broches (25) en mode analogique pour la LED
    configure_ledc();
    // Configuration des boutons
    init_gpio();

    //////////////////////////////////////////////////////
    //initialise LCD
    ESP_LOGI(TAG, "initialized time variable");
    initialize_time_variables();
    update_time(12,25);
    ESP_ERROR_CHECK(i2c_master_init());

    ESP_LOGI(TAG, "I2C initialized");
    lcd_off = lcd_init();
    lcd_off = lcd_clear();
    ESP_LOGI(TAG, "Fin initialized LCD");
    ESP_LOGI(TAG, "LCD is %i", lcd_off);


    alarmset_Mutex = xSemaphoreCreateMutex();

    // Créer la tâche pour l'obtention du temps
    xTaskCreate(obtain_time_task, "ObtainTimeTask", 4096, NULL, 1, NULL);


    vTaskDelay(5000 / portTICK_PERIOD_MS); // Delay of 5s for test because i got first epoch and nothing is displaying might be too fast


    //Tache BTN
    xTaskCreate(bouton_task, "alarme setting", 4096, NULL, 2, NULL);

    // Créer la tâche pour l'affichage du temps sur le moniteur et LCD
    xTaskCreate(display_time_on_monitor_task, "DisplayTimeTask", 4096, NULL, 3, NULL);

    //Tache LED
    xTaskCreate(led_fade_task, "ledc_fade_task", 4096, NULL, 4, NULL);



	//////////////////////////////////////////////////////
    /*
    Make some lib to have separated fct because the reveil.c is kinda big

    Get the T° ??
    ajouter systme de son
    ajouter d'autre LED avec leurs resistance pour plus de lumière

    cleanup_time_variables();
    //////////////////////////////////////////////////////
    */

}
