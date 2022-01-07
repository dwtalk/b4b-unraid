/*
    led.c -  LED module

    Copyright (c) 2011 - 2013 Wistron Corp.

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
#include "led.h"
#include "gpio.h"
#include "gpiopch.h"
#include "gpiosio.h"

MODULE_LICENSE("GPL");

int led_probe (struct led_host *ledhost, struct led_device leddev[]) {
    int    loop;
    struct led_device      *pleddev;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    sema_init(&ledhost->lock, 0);
    #else
    init_MUTEX_LOCKED(&ledhost->lock);
    #endif
    up(&ledhost->lock);

    INIT_LIST_HEAD(&ledhost->__leds);
    for (loop = 0; leddev[loop].type != -1; loop++) {
        list_add_tail(&leddev[loop].list, &ledhost->__leds);
    }

    list_for_each_entry(pleddev, &ledhost->__leds, list) {
        pleddev->host = ledhost;
        pleddev->read = ledd_read;
        pleddev->write = ledd_write;
        pleddev->value = 0;
    }

    return 0;
}

int led_release (struct led_host *ledhost) {
    return 0;
}

int ledd_read (struct led_device *pleddev) {
    int res;
    struct gpio_device *pgpdev = NULL;

    down(&pleddev->host->lock);
    switch(pleddev->host->type) {
        case    LEDHOST_TYPE_SYSTEM:
        case    LEDHOST_TYPE_HDD_STS:
        case    LEDHOST_TYPE_HDD_FAULT:
            break;
        default:
            up(&pleddev->host->lock);
            return -1;
            break;
    }
    switch(pleddev->type) {
        case    LEDDEV_TYPE_PCH:
            pgpdev = pchgphost_search(pleddev->index);
            break;
        case    LEDDEV_TYPE_SIO:
            pgpdev = siogphost_search(pleddev->index);
            break;
        default:
            up(&pleddev->host->lock);
            return -1;
            break;
    }
    if (pgpdev != NULL) {
        if (pgpdev->read != NULL) {
            if ((res = pgpdev->read(pgpdev)) != 0)
                pleddev->value = -1;
            else
                pleddev->value = pgpdev->gpio_value;
        }
    }
    up(&pleddev->host->lock);
    return 0;
}

int ledd_write (struct led_device *pleddev, int value) {
    int res;
    struct gpio_device *pgpdev = NULL;

    down(&pleddev->host->lock);
    switch(pleddev->host->type) {
        case    LEDHOST_TYPE_SYSTEM:
        case    LEDHOST_TYPE_HDD_STS:
        case    LEDHOST_TYPE_HDD_FAULT:
            break;
        default:
            up(&pleddev->host->lock);
            return -1;
            break;
    }
    switch(pleddev->type) {
        case    LEDDEV_TYPE_PCH:
            pgpdev = pchgphost_search(pleddev->index);
            break;
        case    LEDDEV_TYPE_SIO:
            pgpdev = siogphost_search(pleddev->index);
            break;
        default:
            up(&pleddev->host->lock);
            return -1;
            break;
    }
    if (pgpdev != NULL) {
        if (pgpdev->write != NULL) {
            if ((res = pgpdev->write(pgpdev, value)) != 0) {
                pleddev->value = -1;
                up(&pleddev->host->lock);
                return res;
            }
        }
    }

    up(&pleddev->host->lock);
    return 0;
}

int led_init(void) {
    int res = 0;
    WIXPRINT("Initializing LED Module...\n");
    if ((res = siogpio_init()) != 0)
        return res;
    return res;
}

void led_exit(void) {
    WIXPRINT("Removing LED Module...\n");
    siogpio_exit();
    return;
}

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");
