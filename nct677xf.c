/*
    nct677xf.c - NCT677XF SuperI/O module

    Copyright (c) 2012 - 2013 Wistron Corp.

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

MODULE_LICENSE("GPL");

extern int board_id;
extern struct semaphore global_lock;

//int board_id = DESKTOP;

int ncthwmh_probe (struct hwm_host *hwmhost, struct hwm_device hwmdev[]) {
    int loop;
    unsigned char   val1, val2;
    unsigned char   mfunc;
    int baseaddr;
    struct hwm_device   *phwdev;

    down(&global_lock);
    /* Enter Configuration Mode */
    outb_p(0x87, SIO_ADDR);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x07, SIO_ADDR);
    outb_p(0x0B, (SIO_ADDR + 1));
    outb_p(0x60, SIO_ADDR);
    val1 = inb_p((SIO_ADDR + 1));
    outb(0x61, SIO_ADDR);
    val2 = inb_p((SIO_ADDR + 1));

    hwmhost->baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
    hwmhost->basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

    // Set Multifcuntion MSCL/MSDA as I2C pin
    outb_p(0x1B, SIO_ADDR);
    mfunc = inb_p((SIO_ADDR + 1));
    outb_p(0x1B, SIO_ADDR);
    outb_p( (mfunc | 0x1), (SIO_ADDR + 1));

    // Get SMBus baseaddr
    outb_p(0x07, SIO_ADDR);
    outb_p(0x0B, SIO_ADDR + 1);
    outb_p(0x62, SIO_ADDR);
    val1 = inb_p(SIO_ADDR + 1);
    outb(0x63, SIO_ADDR);
    val2 = inb_p(SIO_ADDR + 1);
    outb_p(0xAA, SIO_ADDR);
    up(&global_lock);

    baseaddr = ((val1 << 0x08) | val2) & 0xFFFF;
    // Set SMbus TMP175 slave address and internal register addr
    outb( (unsigned char) 0x00, (unsigned short int)(baseaddr + 0x3));
    outb( (unsigned char) 0x02, (unsigned short int)(baseaddr + 0x4));
    outb( (unsigned char) 0x90, (unsigned short int)(baseaddr + 0x10));
    outb( (unsigned char) 0x00, (unsigned short int)(baseaddr + 0x11));
    outb( (unsigned char) 0x00, (unsigned short int)(baseaddr + 0x12));
    /* Exit Configuration Mode */
    outb_p(0xAA, SIO_ADDR);

    // Set HDDtemp sensor to current mode
    outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (hwmhost->baseaddr & 0xFFFF));
    outb((unsigned char) ( (HWM_DIODE_MODE >> 8) & 0xFF), (unsigned short int) (hwmhost->basedata & 0xFFFF));
    outb((unsigned char) HWM_DIODE_MODE, (unsigned short int) (hwmhost->baseaddr & 0xFFFF));
    outb((unsigned char) 0x6, (unsigned short int) (hwmhost->basedata & 0xFFFF));
    // Set System FAN out as PWM mode
    outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (hwmhost->baseaddr & 0xFFFF));
    outb((unsigned char) 0x0, (unsigned short int) (hwmhost->basedata & 0xFFFF));
    outb((unsigned char) 0x4, (unsigned short int) (hwmhost->baseaddr & 0xFFFF));
    outb((unsigned char) 0x0, (unsigned short int) (hwmhost->basedata & 0xFFFF));

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
        phwdev->read = ncthwmd_read;
        phwdev->write = ncthwmd_write;
        phwdev->hwm_value = 0;
    }

    return 0;
}

int ncthwmh_release (struct hwm_host *hwmhost) {
    return 0;
}

int ncthwmd_fan_read (struct hwm_device *phwmdev) {
    unsigned int hbyte, lbyte;
    unsigned int reghbyte, reglbyte;
    int    retry = 0;

    int value=0,bankaddr;

    switch (phwmdev->hwm_index) {
        case    1:
            {
                reghbyte = HWM_FAN1H;
                reglbyte = HWM_FAN1L;
            }
            break;
        case    2:
            {
                reghbyte = HWM_FAN2H;
                reglbyte = HWM_FAN2L;
            }
            break;
        case    3:
            {
                reghbyte = HWM_FAN3H;
                reglbyte = HWM_FAN3L;
            }
            break;
        case     4:
            {
                reghbyte = HWM_FAN4H;
                reglbyte = HWM_FAN4L;
            }
            break;
        default:
            phwmdev->hwm_value = 0;
            return -1;
    }

    do {
        if (retry > 0) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(100);
        }
        retry++;
        outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        outb((unsigned char) ((reghbyte >> 8) & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
        outb((unsigned char) (reghbyte & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        hbyte = inb((unsigned short int) (phwmdev->host->basedata & 0xFFFF));
        outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        outb((unsigned char) ((reglbyte >> 8) & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
        outb((unsigned char) (reglbyte & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        lbyte = inb((unsigned short int) (phwmdev->host->basedata & 0xFFFF));

        WIXPRINT("[%X %X][reg %X][0x%X][reg %X][0x%X]\n", phwmdev->host->baseaddr, phwmdev->host->basedata, reghbyte, hbyte, reglbyte, lbyte);

        phwmdev->hwm_value = (((hbyte << 8) & 0xFF00) | (lbyte & 0xFF));
        if (phwmdev->hwm_value > 0xF000)
            phwmdev->hwm_value = 0;
    } while ((phwmdev->hwm_value == 0) && (retry < 1));

    for (bankaddr = 1; bankaddr < 3; bankaddr++) {
        outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        outb((unsigned char) (bankaddr & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
        outb((unsigned char) 0x09, (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        value=inb((unsigned short int) (phwmdev->host->basedata & 0xFFFF));
    }

    return 0;
}

int ncthwmd_volt_read (struct hwm_device *phwmdev) {
    unsigned char hbyte;
    unsigned int reghbyte;
    int    retry = 0;

    switch (phwmdev->hwm_index) {
        case    1:
            reghbyte = HWM_VCORE;
            break;
        case    2:
            reghbyte = HWM_AVCC;
            break;
        case    3:
            reghbyte = HWM_3VCC;
            break;
        case    4:
            reghbyte = HWM_VIN0;
            break;
        case    5:
            reghbyte = HWM_VIN1;
            break;
        case    6:
            reghbyte = HWM_VIN2;
            break;
        case    7:
            reghbyte = HWM_VIN3;
            break;
        case    8:
            reghbyte = HWM_3VSB;
            break;
        case    9:
            reghbyte = HWM_VBAT;
            break;
        default:
            phwmdev->hwm_value = 0;
            return -1;
    }

    do {
        if (retry > 0) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(100);
        }
        retry++;
        outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        outb((unsigned char) ((reghbyte >> 8) & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
        outb((unsigned char) (reghbyte & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        hbyte = inb((unsigned short int) (phwmdev->host->basedata & 0xFFFF));
    } while ((hbyte == 0) && (retry < 1));

    switch (phwmdev->hwm_index) {
        case    1:
        case    2:
        case    3:
        case    6:
        case    7:
            phwmdev->hwm_value = hbyte * 8;
            break;
        case    8:
        case    9:
            phwmdev->hwm_value = hbyte * 8 * 2;
            break;
        case    4:
            phwmdev->hwm_value = hbyte * 8 * 16;
            break;
        case    5:
            phwmdev->hwm_value = hbyte * 8 * 4;
            break;
        default:
            phwmdev->hwm_value = 0;
            return -1;
    }

    return 0;
}

int ncthwmd_therm_read (struct hwm_device *phwmdev) {
    unsigned char hbyte;
    unsigned int reghbyte;
    int    retry = 0;


    switch (phwmdev->hwm_index) {
        case    1:
            reghbyte = HWM_SMIOVT1;
            break;
        case    2:
            reghbyte = HWM_BYTETEMP_H;
            //reghbyte = HWM_SMIOVT2;
            break;
        case    3:
            reghbyte = HWM_SMIOVT3;
            break;
        case    4:
            reghbyte = HWM_SMIOVT4;
            break;
        case    5:
            reghbyte = HWM_SMIOVT5;
            break;
        case    6:
            reghbyte = HWM_SMIOVT6;
            break;
        default:
            phwmdev->hwm_value = 0;
            return -1;
    }

    do {
        if (retry > 0) {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(100);
        }
        retry++;
        outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        outb((unsigned char) ((reghbyte >> 8) & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
        outb((unsigned char) (reghbyte & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
        hbyte = inb((unsigned short int) (phwmdev->host->basedata & 0xFFFF));
    } while ((hbyte == 0) && (retry < 1));

    if ((hbyte & 0x80) != 0) {
        phwmdev->hwm_value = (0xFFFFFF00 | (hbyte & 0xFF));
    } else {
        phwmdev->hwm_value = (hbyte & 0xFF);
    }

    return 0;
}

int ncthwmd_other_read (struct hwm_device *phwmdev) {

    switch (phwmdev->hwm_index) {
        case    1:
            phwmdev->hwm_value = board_id;
            break;
        default:
            phwmdev->hwm_value = 0;
            return -1;
    }
    return 0;
}

int ncthwmd_read (struct hwm_device *phwmdev) {
    int    res;

    if (phwmdev->host == NULL)
        return 0;

    down(&phwmdev->host->lock);
    switch (phwmdev->hwm_type) {
        case    HWM_TYPE_FAN:
            if ((res = ncthwmd_fan_read(phwmdev)) != 0)
                phwmdev->hwm_value = -1;
            break;
        case    HWM_TYPE_VOLT:
            if ((res = ncthwmd_volt_read(phwmdev)) != 0)
                phwmdev->hwm_value = -1;
            break;
        case    HWM_TYPE_THERM:
            if ((res = ncthwmd_therm_read(phwmdev)) != 0)
                phwmdev->hwm_value = -1;
            break;
        case    HWM_TYPE_OTHER:
            if ((res = ncthwmd_other_read(phwmdev)) != 0)
                phwmdev->hwm_value = -1;
            break;
        default:
            phwmdev->hwm_value = -1;
            break;
    }
    up(&phwmdev->host->lock);
    return 0;
}

int ncthwmd_write (struct hwm_device *phwmdev, int value) {
    unsigned int lbyte = 0;
    unsigned int bankaddr = 0;
    unsigned int sysfan_pwm = 0, cpufan_pwm = 0;

    down(&phwmdev->host->lock);
    switch (phwmdev->hwm_type) {
        case    HWM_TYPE_FAN:
            {
                switch (phwmdev->hwm_index) {
                    case    1:
                    case    2:
                        sysfan_pwm=value;
                        cpufan_pwm=0;
                        break;
                    case    3:
                    case    4:
                        sysfan_pwm=0;
                        cpufan_pwm=value;
                        break;
                    default:
                        break;
                }

                for (bankaddr = 1; bankaddr < 3; bankaddr++) {
                    outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
                    outb((unsigned char) (bankaddr & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
                    outb((unsigned char) 0x09, (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
                    if ( bankaddr == 1 ) {
                        outb((unsigned char) sysfan_pwm, (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
                    } else {
                        outb((unsigned char) cpufan_pwm, (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
                    }
                    outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
                    outb((unsigned char) (bankaddr & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
                    outb((unsigned char) 0x02, (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
                    lbyte = inb((unsigned short int) (phwmdev->host->basedata & 0xFFFF));
                    if (value == 0) {
                        switch (bankaddr) {
                            case    1:
                                //lbyte = 0x31;
                                lbyte &= 0x0F;  // Manual mode
                                break;
                            case    2:
                                //lbyte = 0x12;
                                lbyte &= 0x4F;  // SMART FAN IV mode.
                                break;
                            default:
                                break;
                        }
                    } else {
                        lbyte &= 0x0F;
                    }
                    outb((unsigned char) (HWM_BANK & 0xFF), (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
                    outb((unsigned char) (bankaddr & 0xFF), (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
                    outb((unsigned char) 0x02, (unsigned short int) (phwmdev->host->baseaddr & 0xFFFF));
                    outb((unsigned char) lbyte, (unsigned short int) (phwmdev->host->basedata & 0xFFFF));
               }
            }
            break;
        default:
            up(&phwmdev->host->lock);
            return 0;
            break;
    }
    up(&phwmdev->host->lock);
    return 0;
}

int nct677xf_init(void) {
    int res = 0;
    WIXPRINT("Initializing NCT677XF SuperI/O Chip...\n");
    return res;
}

void nct677xf_exit(void) {
    WIXPRINT("Removing NCT677XF SuperI/O Chip...\n");
    return;
}

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");
