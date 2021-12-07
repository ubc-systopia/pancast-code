#ifndef __LED_H__
#define __LED_H__

#include <device.h>
#include <bluetooth/bluetooth.h>

void configure_blinky(void);
void beacon_led_timeout_handler(struct k_timer *timer);

#endif /* __LED_H__ */
