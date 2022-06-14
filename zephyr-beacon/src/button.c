#include <zephyr.h>
#include <devicetree.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_vs.h>

#include <button.h>
#include <beacon.h>
#include <storage.h>
#include <log.h>
#include <settings.h>

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static struct gpio_callback button_gpio_cb;

void button_pressed(const struct device *dev, struct gpio_callback *cb,
    uint32_t pins)
{
  log_expf("pins: 0x%0x pressed\r\n", pins);
  _beacon_broadcast_(0, 1);
}

void button_init(void)
{
  if (!device_is_ready(button.port)) {
    log_errorf("button port: %s pin: %d not ready\r\n",
        button.port->name, button.pin);
    return;
  }

  int ret = gpio_pin_configure_dt(&button, GPIO_INPUT);
  if (ret != 0) {
    log_errorf("button port: %s pin: %d config err: %d\r\n",
        button.port->name, button.pin, ret);
    return;
  }

  ret = gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret != 0) {
    log_errorf("button port: %s pin: %d interrupt config err: %d\r\n",
        button.port->name, button.pin, ret);
    return;
  }

  gpio_init_callback(&button_gpio_cb, button_pressed, BIT(button.pin));
  gpio_add_callback(button.port, &button_gpio_cb);
  log_expf("button port: %s pin: %d set callback\r\n",
      button.port->name, button.pin);
}

