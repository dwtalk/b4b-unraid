#ifndef	GPIOPCH_H
#define	GPIOPCH_H

#include "gpio.h"

#define LPC_VENDORID            0x8086
#define LPC_DEVICEID            0x3a18

#define	LPC_GPIO_BASEADDR		0x48

#define GPIO_OFFSET_USE_SEL     0x00
#define GPIO_OFFSET_IO_SEL      0x04
#define GPIO_OFFSET_LVL         0x0C
#define GPIO_OFFSET_BLINK       0x18
#define GPIO_OFFSET_SERBLINK    0x1C
#define GPIO_OFFSET_SBCMDSTS    0x20
#define GPIO_OFFSET_SBDATA      0x24
#define GPIO_OFFSET_NMI_EN      0x28
#define	GPIO_OFFSET_NMI_STS		0x2A
#define GPIO_OFFSET_INV         0x2C
#define GPIO_OFFSET_USE_SEL2    0x30
#define GPIO_OFFSET_IO_SEL2     0x34
#define GPIO_OFFSET_LVL2        0x38
#define GPIO_OFFSET_USE_SEL3    0x40
#define GPIO_OFFSET_IO_SEL3     0x44
#define GPIO_OFFSET_LVL3        0x48
#define GPIO_OFFSET_RST_SEL     0x60
#define GPIO_OFFSET_RST_SEL2    0x64
#define GPIO_OFFSET_RST_SEL3    0x68

int  pchgpio_init(void);
void pchgpio_exit(void);
int  pchgpdev_read (struct gpio_device *gpdev);
int  pchgpdev_write (struct gpio_device *gpdev, int value);
struct gpio_device *pchgphost_search(int index);
unsigned int pchoffset_read(int offset);
void pchoffset_write(int offset, unsigned int value);
void pchoffset_and(int offset, unsigned int value);
void pchoffset_or(int offset, unsigned int value);

#endif
