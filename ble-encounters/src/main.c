#include <zephyr.h>
#include <sys/printk.h>

void main(void)
{
	printk("Testing! The platform is: %s\n", CONFIG_BOARD);
}
