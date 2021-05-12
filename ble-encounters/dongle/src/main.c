#include <zephyr.h>
#include <sys/printk.h>

void main(void)
{
	printk("Starting %s on %s\n", CONFIG_BT_DEVICE_NAME, CONFIG_BOARD);
}
