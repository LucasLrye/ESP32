#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c-lcd.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/dns.h"
#include "time.h"
#include "lwip/apps/sntp.h"
#include <freertos/semphr.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <freertos/semphr.h>
#include <stdbool.h>

// Broches du bouton poussoir et de la LED
// Color Green -> modify alarm clock -> h, m, show now()
#define BOUTON_PIN_1 GPIO_NUM_2
// Color Blue -> To reset the LCD because backlight doesn't work, so we are using a interruptor
#define BOUTON_PIN_2 GPIO_NUM_4
// Color White -> activated/deactivated alarm clock
#define BOUTON_PIN_3 GPIO_NUM_5


// Function to handle what is happening when pressing a button
void bouton_alarme(void);

/// Task concerning Button
void bouton_task(void *pvParameters);
