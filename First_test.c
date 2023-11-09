#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

//extern "C"{  // SI on a un code en .cpp
//	void app_main();
//}

void app_main(void)
{
  gpio_reset_pin(25);     // GPIO 2 est généralement associé à la LED intégrée
  gpio_set_direction(25, GPIO_MODE_OUTPUT);

  while (1) {
    gpio_set_level(25, 1);  // Allumer la LED
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    gpio_set_level(25, 0);  // Éteindre la LED
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
