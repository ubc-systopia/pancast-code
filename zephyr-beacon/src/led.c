#include <zephyr.h>
#include <devicetree.h>
#include <drivers/gpio.h>

#include "led.h"

#define LOG_LEVEL__INFO

#include <include/log.h>

#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#define LED0 DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS DT_GPIO_FLAGS(LED0_NODE, gpios)
#else
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED0 ""
#define PIN 0
#define FLAGS 0
#endif

const struct device *dev = NULL;

void configure_blinky(void)
{
  dev = device_get_binding(LED0);
  int ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
  if (ret < 0) {
    log_errorf("Error configuring LED %s pin %d flags %d ret %d\r\n",
        LED0, PIN, FLAGS, ret);
  }
}

void beacon_led_timeout_handler(struct k_timer *timer)
{
  gpio_pin_toggle(dev, PIN);
}
