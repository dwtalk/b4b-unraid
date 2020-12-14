/*
        WDT.c - WDT module

        Copyright (c) 2011 - 2012  Aldofo Lin <aldofo_lin@wistron.com.tw>

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
#include "hwm.h"
#include "nct677xf.h"
#include "wdt.h"


extern struct semaphore global_lock;

int wdt_probe (struct hwm_host *hwmhost, struct hwm_device hwmdev[]) {
    int    loop;
    struct hwm_device      *phwdev;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    sema_init(&hwmhost->lock, 0);
    #else
    init_MUTEX_LOCKED(&hwmhost->lock);
    #endif
    up(&hwmhost->lock);

    INIT_LIST_HEAD(&hwmhost->__hwms);
    for (loop = 0; hwmdev[loop].hwm_type != -1; loop++) {
        list_add_tail(&hwmdev[loop].list, &hwmhost->__hwms);
    }

    list_for_each_entry(phwdev, &hwmhost->__hwms, list) {
        phwdev->host = hwmhost;
        phwdev->read = wdt_read;
        phwdev->write = wdt_write;
        phwdev->hwm_value = 0;
    }

    return 0;
}

int wdt_release (struct hwm_host *hwmhost) {
    return 0;
}

int wdt_read (struct hwm_device *phwmdev) {
    int    res = 0;
    unsigned short val = 0;

    down(&global_lock);
    down(&phwmdev->host->lock);

    switch (phwmdev->hwm_type) {
        case HWM_TYPE_WDT:
            {
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x08, (SIO_ADDR + 1));
                outb_p(HWM_WDT_REG_WDTCounter, SIO_ADDR);
                val = inb_p((SIO_ADDR + 1));
                outb_p(0xAA, SIO_ADDR);
                phwmdev->hwm_value = val;
            }
            break;
        default:
            res = -EINVAL;
            break;
    }

    up(&phwmdev->host->lock);
    up(&global_lock);
    return res;
}

int wdt_write (struct hwm_device *phwmdev, int value) {
    int    res = 0;
    unsigned short val = 0;


    down(&global_lock);
    down(&phwmdev->host->lock);

    switch (phwmdev->hwm_type) {
        case HWM_TYPE_WDT:
            {
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x08, (SIO_ADDR + 1));
                outb_p(HWM_WDT_REG_WDTCounter, SIO_ADDR);
                outb_p((value & 0xFF), (SIO_ADDR + 1));
                // reset WDT status
                outb_p(HWM_WDT_REG_WDTSTS, SIO_ADDR);
                outb_p((val & 0xEF), (SIO_ADDR + 1));

                outb_p(0xAA, SIO_ADDR);
            }
            break;
        default:
            res = -EINVAL;
            break;
    }

    up(&phwmdev->host->lock);
    up(&global_lock);
    return 0;
}

int wdt_init(void) {
    int         res = 0;
    unsigned char    val;
    WIXPRINT("Initializing WDT...\n");

    down(&global_lock);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x07, SIO_ADDR);
    outb_p(0x08, (SIO_ADDR + 1));
    // Set GP73 as output
    outb_p(0x07, SIO_ADDR);
    outb_p(0x07, SIO_ADDR + 1);
    outb_p(0xE0, SIO_ADDR);
    val = inb_p((SIO_ADDR + 1));
    outb_p(0xE0, SIO_ADDR);
    outb_p((val & 0xF7), (SIO_ADDR + 1));
    // Set GP73 as WDTO alert pin
    outb_p(0x07, SIO_ADDR);
    outb_p(0x07, SIO_ADDR + 1);
    outb_p(0xEC, SIO_ADDR);
    val = inb_p((SIO_ADDR + 1));
    outb_p(0xEC, SIO_ADDR);
    outb_p((val | 0x8 ), (SIO_ADDR + 1));

    // Set WDT1 active
    outb_p(0x30, SIO_ADDR);
    val = inb_p((SIO_ADDR + 1));
    outb_p(0x30, SIO_ADDR);
    outb_p((val | 0x01), (SIO_ADDR + 1));
    // Set WDT in second mode.
    outb_p(HWM_WDT_REG_WDTConf, SIO_ADDR);
    val = inb_p((SIO_ADDR + 1));
    outb_p(HWM_WDT_REG_WDTConf, SIO_ADDR);
    outb_p((val & 0xF7), (SIO_ADDR + 1));
    // Disable WDT counter.
    outb_p(HWM_WDT_REG_WDTCounter, SIO_ADDR);
    outb_p( 0x0, (SIO_ADDR + 1));

    outb_p(0xAA, SIO_ADDR);
    up(&global_lock);
    return res;
}

void wdt_exit(void) {
    WIXPRINT("Removing WDT...\n");
    return;
}

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");
