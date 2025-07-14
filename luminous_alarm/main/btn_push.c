/// Bouton Poussoir

#include "btn_push.h"

// Tag for LOG message
static const char *TAG_BTN = "BTN_PUSH";

extern bool alarm_set;
extern SemaphoreHandle_t alarmset_Mutex;
extern bool lcd_off;
extern int *hours;
extern int *minutes;

// Button of the alarm
void bouton_alarme(void) {
    int etatBouton_1 = gpio_get_level(BOUTON_PIN_1);
    int etatBouton_2 = gpio_get_level(BOUTON_PIN_2);
    int etatBouton_3 = gpio_get_level(BOUTON_PIN_3);
    int cpt = 2;

    if (etatBouton_3 == 0){
        // Prendre le mutex avant de modifier la variable
        xSemaphoreTake(alarmset_Mutex, portMAX_DELAY);
        alarm_set = !alarm_set;
        ESP_LOGI(TAG_BTN, "Alarme is now set to  %s\n", alarm_set ? "true" : "false");
        //affiche sur LED
        lcd_clear();
        lcd_put_cur(0, 0);
        lcd_send_string("Alarm clock is : ");
        lcd_put_cur(1,0);
        char alarmset_txt[64];
        if (alarm_set){
            snprintf(alarmset_txt, sizeof(alarmset_txt), "ACTIVATED");
        }else{
            snprintf(alarmset_txt, sizeof(alarmset_txt), "NOT ACTIVATED");
        }
        lcd_send_string(alarmset_txt);
        xSemaphoreGive(alarmset_Mutex);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Attendre un court moment pour éviter les rebonds du bouton
        // Put the time again to the LCD screen
        if (lcd_clear() == 1){
            lcd_off = 1;
        }else{
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        // Format the date string
        char date_buffer[64];
        strftime(date_buffer, sizeof(date_buffer), "%A %d/%m/%y", &timeinfo);
        lcd_send_string(date_buffer);
        // Format the time string
        char time_buffer[64];
        strftime(time_buffer, sizeof(time_buffer), "Week %U, %H:%M", &timeinfo);
        lcd_put_cur(1, 0);
        lcd_send_string(time_buffer);
        }
    }

    // if we push blue button -> Reset LCD
    if (etatBouton_2 == 0){
        ESP_LOGI(TAG_BTN, "Reset LCD");
        lcd_off = lcd_init();
        if (lcd_off == 0){
        lcd_clear();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Attendre un court moment pour éviter les rebonds du bouton
        // Put the time again to the LCD screen
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        // Format the date string
        char date_buffer[64];
        strftime(date_buffer, sizeof(date_buffer), "%A %d/%m/%y", &timeinfo);
        lcd_send_string(date_buffer);
        // Format the time string
        char time_buffer[64];
        strftime(time_buffer, sizeof(time_buffer), "Week %U, %H:%M", &timeinfo);
        lcd_put_cur(1, 0);
        lcd_send_string(time_buffer);
        }
        ESP_LOGI(TAG_BTN, "LCD is now  %s\n", lcd_off ? "false" : "true");
    }


    if (etatBouton_1 == 0){ //declenche setting
        cpt = 0;
        ESP_LOGI(TAG_BTN, "compteur: %d", cpt);
        //affiche sur LED
        if (lcd_clear() == 1){
            lcd_off = 1;
            cpt = 3;
        }else{
            lcd_put_cur(0, 0);
            lcd_send_string("Alarm set to (h): ");
            lcd_put_cur(1,0);
            char alarme_txt[64];
            snprintf(alarme_txt, sizeof(alarme_txt), "%dh %dmin", *hours, *minutes);
            lcd_send_string(alarme_txt);
            vTaskDelay(pdMS_TO_TICKS(1000)); // Attendre un court moment pour éviter les rebonds du bouton
        }
    }


    while (cpt <2) {

        etatBouton_1 = gpio_get_level(BOUTON_PIN_1);
        etatBouton_2 = gpio_get_level(BOUTON_PIN_2);
        if(cpt ==0 && etatBouton_2 == 0){ //augmente heure
            *hours += 1;
            if(*hours >24){
                *hours = 00;
            }
            char alarme_txt[64];
            snprintf(alarme_txt, sizeof(alarme_txt), "%dh %dmin", *hours, *minutes);
            //ESP_LOGI(TAG_BTN, "alarme: %s", alarme_txt); // Log the buffer content
            //affiche sur LED
            lcd_put_cur(0, 0);
            lcd_send_string("Alarm set to (h) : ");
            lcd_put_cur(1,0);
            lcd_send_string(alarme_txt);
            vTaskDelay(pdMS_TO_TICKS(50)); // Attendre un court moment pour éviter les rebonds du bouton

        }else if(cpt == 1 && etatBouton_2 ==0){ //augmente minutes
            *minutes += 1;
            if (*minutes >60){
                *minutes = 00; //ne met pas vraiment 00 mais 0
            }
            char alarme_txt[64];
            snprintf(alarme_txt, sizeof(alarme_txt), "%dh %dmin", *hours, *minutes);
            //ESP_LOGI(TAG_BTN, "alarme: %s", alarme_txt);
            //affiche sur LED
            lcd_clear();
            lcd_put_cur(0, 0);
            lcd_send_string("Alarm set to (m): ");
            lcd_put_cur(1,0);
            lcd_send_string(alarme_txt);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if(etatBouton_1 == 0){ //change
            cpt +=1;
            ESP_LOGI(TAG_BTN, "compteur: %d", cpt);

            if (cpt == 1){
            char alarme_txt[64];
            snprintf(alarme_txt, sizeof(alarme_txt), "%dh %dmin", *hours, *minutes);
            //affiche sur LED
            lcd_clear();
            lcd_put_cur(0, 0);
            lcd_send_string("Alarm set to (m): ");
            lcd_put_cur(1,0);
            lcd_send_string(alarme_txt);
            } else if(cpt == 2){
                ESP_LOGI(TAG_BTN, "alarm is set to %dh %dmin", *hours, *minutes);
                lcd_clear();
                time_t now;
                struct tm timeinfo;
                time(&now);
                localtime_r(&now, &timeinfo);

                // Format the date string
                char date_buffer[64];
                strftime(date_buffer, sizeof(date_buffer), "%A %d/%m/%y", &timeinfo);
                lcd_send_string(date_buffer);

                // Format the time string
                char time_buffer[64];
                strftime(time_buffer, sizeof(time_buffer), "Week %U, %H:%M", &timeinfo);
                lcd_put_cur(1, 0);
                lcd_send_string(time_buffer);
        }

            vTaskDelay(pdMS_TO_TICKS(50));

        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }


}



/// Task concerning Button
void bouton_task(void *pvParameters)
{
    while (1)
    {
        bouton_alarme();
        vTaskDelay(100 / portTICK_PERIOD_MS); // Update every 1 seconds
    }
}
