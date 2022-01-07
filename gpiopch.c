/*
	gpiopch.c - Intel PCH GPIO GPIO module

	Copyright (c) 2008 - 2009  Aldofo Lin <aldofo_lin@wistron.com.tw>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/reboot.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "b4b.h"
#include "gpio.h"
#include "gpiopch.h"

MODULE_LICENSE("GPL");

#define	GPIO_TYPE_GPI           0x01
#define	GPIO_TYPE_GPO           0x02
#define	GPIO_TYPE_GPO_BLINKING  0x03
#define	GPIO_STS_DISABLE        0x00
#define	GPIO_STS_ENABLE         0x01
#define	GPIO_STS_BLINKING       0x02
#define	GPIO_STS_NOBLINKING     0x03

//extern int board_id;
//int board_id = DESKTOP;
extern int    board_id;

int pchgphost_probe     (struct gpio_host *gphost);
int pchgphost_release   (struct gpio_host *gphost);
int pchgpdev_read       (struct gpio_device *gpdev);
int pchgpdev_write      (struct gpio_device *gpdev, int value);

struct gpio_host gpiopch_host = {
	.probe			= pchgphost_probe,
	.release		= pchgphost_release,
	.baseaddr		= 0,
};

struct gpio_device gpdev[] = {
{.gpio_index = 0, .gpio_direction = GPIO_DIRECTION_INPUT},
{.gpio_index = 1, .gpio_direction = GPIO_DIRECTION_INPUT},
{.gpio_index = 2, .gpio_direction = GPIO_DIRECTION_INPUT},
{.gpio_index = 3, .gpio_direction = GPIO_DIRECTION_INPUT},
{.gpio_index = 4, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 5, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 10, .gpio_direction = GPIO_DIRECTION_INPUT},
{.gpio_index = 13, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 17, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 19, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 21, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 22, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 36, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = -1},
};

int pchgphost_probe (struct gpio_host *gphost) {
	struct pci_dev 		*gpiopcidev;
	struct gpio_device      *pgpdev;
	int			loop;

	gpiopcidev = pci_get_device(LPC_VENDORID, LPC_DEVICEID, NULL);
	if (gpiopcidev == NULL) {
		gpiopcidev = pci_get_device(LPC_VENDORID, LPC_DEVICEID, NULL);
		if (gpiopcidev == NULL)
		{
			WIXVPRINT("Can't find PCH LPC device.\n");
			return -1;
		}
	}

	if (pci_read_config_dword(gpiopcidev, LPC_GPIO_BASEADDR, (u32 *) &gphost->baseaddr) < 0) {
		WIXVPRINT("pci_read_config_dword\n");
		return -1;
	}

	if ((gphost->baseaddr & 0x00000001) == 0x00) {
		WIXVPRINT("Can't support I/O space.\n");
		return -1;
	}

	gphost->baseaddr &= 0x0000FF80;
	outl((inl(gphost->baseaddr + GPIO_OFFSET_USE_SEL2) | 0x03), (gphost->baseaddr + GPIO_OFFSET_USE_SEL2));
	outl((inl(gphost->baseaddr + GPIO_OFFSET_IO_SEL2) | 0x03), (gphost->baseaddr + GPIO_OFFSET_IO_SEL2));

    // set ICH GP4,GP5,GP13,GP17,GP19,GP21,GP22 as output
	outl((inl(gphost->baseaddr + GPIO_OFFSET_USE_SEL) | 0x6A2030), (gphost->baseaddr + GPIO_OFFSET_USE_SEL));
	outl((inl(gphost->baseaddr + GPIO_OFFSET_IO_SEL)  & 0xFF95DFCF), (gphost->baseaddr + GPIO_OFFSET_IO_SEL));
	//outl((inl(gphost->baseaddr + GPIO_OFFSET_LVL) | 0x6A2030), (gphost->baseaddr + GPIO_OFFSET_LVL));

    // set ICH GP36 as output
	outl((inl(gphost->baseaddr + GPIO_OFFSET_USE_SEL2) | 0x10), (gphost->baseaddr + GPIO_OFFSET_USE_SEL2));
	outl((inl(gphost->baseaddr + GPIO_OFFSET_IO_SEL2)  & 0xFFFFFFEF), (gphost->baseaddr + GPIO_OFFSET_IO_SEL2));
   //outl((inl(gphost->baseaddr + GPIO_OFFSET_LVL2) | 0x10), (gphost->baseaddr + GPIO_OFFSET_LVL2));

    // Get board strip pin value
    board_id = (~(inl(gphost->baseaddr + GPIO_OFFSET_LVL) >> 1)) & 0x1;
    WIXVPRINT("Mainboard type is \"%s\"",(board_id)?"1U4bay":"Desktop");

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
	sema_init(&gphost->lock, 0);
    #else
	init_MUTEX_LOCKED(&gphost->lock);
    #endif
	up(&gphost->lock);

	INIT_LIST_HEAD(&gphost->__gpios);

	for (loop = 0; gpdev[loop].gpio_index != -1; loop++)
	{
		list_add_tail(&gpdev[loop].list, &gphost->__gpios);
	}

	list_for_each_entry(pgpdev, &gphost->__gpios, list) {
		pgpdev->host = gphost;
		pgpdev->read = pchgpdev_read;
		pgpdev->write = pchgpdev_write;
		pgpdev->gpio_value = pgpdev->read(pgpdev);
	}
	return 0;
}

int pchgphost_release (struct gpio_host *gphost) {
	return 0;
}

int pchgpdev_read (struct gpio_device *gpdev) {
	int retval, tmp;
	WIXPRINT("LVL: 0x%X, LVL2: 0x%X\n", inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL), inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL2));
	WIXPRINT("SEL: 0x%X, SEL2: 0x%X\n", inl(gpdev->host->baseaddr + GPIO_OFFSET_USE_SEL), inl(gpdev->host->baseaddr + GPIO_OFFSET_USE_SEL2));
	WIXPRINT("IO: 0x%X, IO2: 0x%X\n", inl(gpdev->host->baseaddr + GPIO_OFFSET_IO_SEL), inl(gpdev->host->baseaddr + GPIO_OFFSET_IO_SEL2));
	WIXPRINT("baseaddr: 0x%X, index: 0x%X\n", gpdev->host->baseaddr, gpdev->gpio_index);

	down(&gpdev->host->lock);

	if ((gpdev->gpio_index >= 32) && (gpdev->gpio_index <= 63)) {
		tmp = (inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL2) >> (gpdev->gpio_index - 32)) & 0x01;
		retval = (~(tmp)) & 0x01;
	} else if (gpdev->gpio_index >= 64) {
		tmp = (inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL3) >> (gpdev->gpio_index - 64)) & 0x01;
		retval = (~(tmp)) & 0x01;
	} else {
		tmp = (inl(gpdev->host->baseaddr + GPIO_OFFSET_BLINK) >> (gpdev->gpio_index)) & 0x01;
		switch (tmp) {
			case	0:
				retval = (inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL) >> (gpdev->gpio_index)) & 0x01;
				retval = (~(retval)) & 0x01;
				break;
			case	1:
				retval = 2;
				break;
			default:
				retval = -127;
				break;
		}
	}

	WIXPRINT("tmp: 0x%X, retval: 0x%X\n", tmp, retval);
	gpdev->gpio_value = retval;
	up(&gpdev->host->lock);

	return 0;
}

int pchgpdev_write (struct gpio_device *gpdev, int value) {
	if (gpdev->gpio_direction == GPIO_DIRECTION_INPUT)
		return 0;

	down(&gpdev->host->lock);
	if (gpdev->gpio_index >= 32) {
		switch (value) {
			case	0:
				if (gpdev->gpio_index >= 64) {
					outl(inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL3) | (0x01 << (gpdev->gpio_index - 64)), (gpdev->host->baseaddr + GPIO_OFFSET_LVL3));
				} else {
					outl(inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL2) | (0x01 << (gpdev->gpio_index - 32)), (gpdev->host->baseaddr + GPIO_OFFSET_LVL2));
				}
				break;
			case	1:
				if (gpdev->gpio_index >= 64) {
					outl(inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL3) & ~(0x01 << (gpdev->gpio_index - 64)), (gpdev->host->baseaddr + GPIO_OFFSET_LVL3));
				} else {
					outl(inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL2) & ~(0x01 << (gpdev->gpio_index - 32)), (gpdev->host->baseaddr + GPIO_OFFSET_LVL2));
				}
				break;
			default:
				up(&gpdev->host->lock);
				return -EINVAL;
				break;
		}
	} else if (gpdev->gpio_index >= 0) {
		switch (value) {
			case	0:
				outl(inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL) | (0x01 << (gpdev->gpio_index)), (gpdev->host->baseaddr + GPIO_OFFSET_LVL));
				outl((inl(gpdev->host->baseaddr + GPIO_OFFSET_BLINK) & ~(0x01 << gpdev->gpio_index)), (gpdev->host->baseaddr + GPIO_OFFSET_BLINK));
				break;
			case	1:
				outl(inl(gpdev->host->baseaddr + GPIO_OFFSET_LVL) & ~(0x01 << (gpdev->gpio_index)), (gpdev->host->baseaddr + GPIO_OFFSET_LVL));
				outl((inl(gpdev->host->baseaddr + GPIO_OFFSET_BLINK) & ~(0x01 << gpdev->gpio_index)), (gpdev->host->baseaddr + GPIO_OFFSET_BLINK));
				break;
			case	2:
				outl((inl(gpdev->host->baseaddr + GPIO_OFFSET_BLINK) | (0x01 << gpdev->gpio_index)), (gpdev->host->baseaddr + GPIO_OFFSET_BLINK));
				break;
			default:
				up(&gpdev->host->lock);
				return -EINVAL;
				break;
		}
	} else {
		up(&gpdev->host->lock);
		return -EINVAL;
	}

	gpdev->gpio_value = value;
	up(&gpdev->host->lock);
	return 0;
}

struct gpio_device *pchgphost_search(int index) {

	struct gpio_device *pgpdev, *target = NULL;

	if (!list_empty(&gpiopch_host.__gpios))
	{
		list_for_each_entry(pgpdev, &gpiopch_host.__gpios, list)
		{
			if (pgpdev->gpio_index == index)
				target = pgpdev;
		}
	}

	return target;
}

unsigned int pchoffset_read(int offset) {
	unsigned int value;
	down(&gpiopch_host.lock);
	value = inl(gpiopch_host.baseaddr + offset);
	up(&gpiopch_host.lock);
	return value;
}

void pchoffset_write(int offset, unsigned int value) {
	down(&gpiopch_host.lock);
	outl(value, (gpiopch_host.baseaddr + offset));
	up(&gpiopch_host.lock);
	return;
}

void pchoffset_and(int offset, unsigned int value) {
	down(&gpiopch_host.lock);
	outl((inl(gpiopch_host.baseaddr + offset) & value), (gpiopch_host.baseaddr + offset));
	up(&gpiopch_host.lock);
	return;
}

void pchoffset_or(int offset, unsigned int value) {
	down(&gpiopch_host.lock);
	outl((inl(gpiopch_host.baseaddr + offset) | value), (gpiopch_host.baseaddr + offset));
	up(&gpiopch_host.lock);
	return;
}

int pchgpio_init(void) {
	int retval = 0;
	struct gpio_host *gphost = &gpiopch_host;

	WIXPRINT("Initializing PCH GPIO driver...\n");
	if (gphost->probe != NULL) {
		if ((retval = gphost->probe(gphost)) != 0)
			WIXVPRINT("PCH GPIO Function doesn't support registered.\n");
	}

	return retval;
}

void pchgpio_exit(void) {
	int retval = 0;
	struct gpio_host *gphost = &gpiopch_host;

	WIXPRINT("Removing PCH GPIO driver...\n");
	if (gphost->release != NULL) {
		if ((retval = gphost->release(gphost)) != 0)
			WIXVPRINT("PCH GPIO Function doesn't support unregistered. [%d]\n", retval);
	}
	return;
}

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");
