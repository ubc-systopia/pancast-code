#ifndef __BUTTON_H__
#define __BUTTON_H__
/*
 * button.h
 *
 *     author: aasthakm
 * created on: Jun 14, 2022
 *
 * handlers for various buttons on nordic beacon board
 */

#include <device.h>
#include <drivers/gpio.h>

void button_init(void);
void button_pressed(const struct device *dev, struct gpio_callback *cb,
    uint32_t pins);

#endif /* __BUTTON_H__ */
