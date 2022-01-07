/*
    gpiosio.c - SuperI/O GPIO module

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
#include <linux/timer.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "b4b.h"
#include "gpio.h"
#include "gpiosio.h"

MODULE_LICENSE("GPL");

extern struct semaphore global_lock;
extern int    board_id;
//int    board_id = DESKTOP;

typedef struct legacy_timer_emu {
		struct timer_list t;
		void (*function)(unsigned long);
		unsigned long data;
} _timer;

struct semaphore led_lock;
static struct legacy_timer_emu sys_red_led_timer, hdd_red_led_timer;
static struct legacy_timer_emu sys_id_led_timer;

int siogphost_probe(struct gpio_host *gphost);
int siogphost_release(struct gpio_host *gphost);
int siogpdev_read(struct gpio_device *gpdev);
int siogpdev_write(struct gpio_device *gpdev, int value);

static void sys_led_timer_proc(unsigned long data);
static void hdd_led_timer_proc(unsigned long data);
static void sys_id_led_timer_proc(unsigned long data);
//static void empty_led_timer_proc(struct timer_list *timerReturn);

typedef struct {
    char name[20];
    unsigned int bit_mask;
    unsigned char status;   //0==>OFF 1==>ON 2==>2Hz 3==>1Hz 4==>0.5Hz
    unsigned int blinking;  //0==>non-blinking 1==>blinking
    unsigned int wait_sync; //0==>no wait for blink sync 1==>wait blink sync
} shift_led_t;

static shift_led_t lcm_backlight = {"lcm_backlight", 34, 1, 0, 0};
static shift_led_t sys_id_led  = {"sys_id",  35, 0, 0, 0};
static shift_led_t sys_red_led = {"sys_red", 71, 0, 0, 0};
static shift_led_t hdd_red_led = {"hdd_red", 72, 0, 0, 0};


struct gpio_host gpiosio_host = {
    .probe      = siogphost_probe,
    .release    = siogphost_release,
};

struct gpio_device sio_gpdev[] = {
{.gpio_index = 1, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 34, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 35, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 44, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 45, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 71, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 72, .gpio_direction = GPIO_DIRECTION_OUTPUT},
#ifdef _BRIGHTNESS_ADJUST
{.gpio_index = 2, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 3, .gpio_direction = GPIO_DIRECTION_OUTPUT},
{.gpio_index = 4, .gpio_direction = GPIO_DIRECTION_OUTPUT},
#endif
{.gpio_index = -1},
};

int siogphost_probe (struct gpio_host *gphost) {
    struct gpio_device *pgpdev;
    int loop;
    unsigned char val;
    unsigned char val1, val2;
    /* Enter Configuration Mode */
    outb_p(0x87, SIO_ADDR );
    outb_p(0x87, SIO_ADDR );
    outb_p(0x20, SIO_ADDR );
    val = inb((SIO_ADDR + 1));
    if (val != 0xC5) { //Global register CR20H
        WIXVPRINT("Can't find SIO device.val=0x%x\n", val);
        outb_p(0xAA, SIO_ADDR);
        return -1;
    }
    //set GP44,GP45 as multifunc mode.
    outb_p(0x1B, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    outb_p(0x1B, SIO_ADDR);
    outb_p( val | 0x40, SIO_ADDR + 1);

    outb_p(0x07, SIO_ADDR);
    outb_p(0x0B, SIO_ADDR + 1);
    outb_p(0xF7, SIO_ADDR);
    outb_p(0x07, SIO_ADDR + 1);
    outb_p(0xF8, SIO_ADDR);
    outb_p(0x05, SIO_ADDR + 1);

    //active GPIO1/3/5/7
    outb_p(0x07, SIO_ADDR);
    outb_p(0x09, SIO_ADDR + 1);
    outb_p(0x30, SIO_ADDR);
    val = inb_p(SIO_ADDR + 1);
    outb_p(0x07, SIO_ADDR);
    outb_p(0x09, SIO_ADDR + 1);
    outb_p(0x30, SIO_ADDR);
    outb_p(val | 0xaa, SIO_ADDR + 1);
    //set GP34,GP35 as GPIO mode.
    outb_p(0x07, SIO_ADDR);
    outb_p(0x09, SIO_ADDR + 1);
    outb_p(0xE4, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    outb_p(0xE4, SIO_ADDR);
    outb_p(val & 0xCF, SIO_ADDR +1);
    //set GP34,GP35 as GPIO pin.
    outb_p(0x07, SIO_ADDR);
    outb_p(0x09, SIO_ADDR + 1);
    outb_p(0xEA, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    outb_p(0xEA, SIO_ADDR);
    outb_p(val & 0xCF, SIO_ADDR +1);
    //set GP71,GP72 as GPIO mode.
    outb_p(0x07, SIO_ADDR);
    outb_p(0x07, SIO_ADDR + 1);
    outb_p(0xE0, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    outb_p(0xE0, SIO_ADDR);
    outb_p(val & 0xF9, SIO_ADDR +1);
    //get GPIO Base address
    outb_p(0x07, SIO_ADDR);
    outb_p(0x08, SIO_ADDR + 1);
    outb_p(0x60, SIO_ADDR);
    val1 = inb_p(SIO_ADDR + 1);
    outb_p(0x61, SIO_ADDR);
    val2 = inb_p(SIO_ADDR + 1);
    //enable GPIO base address mode
    outb_p(0x07, SIO_ADDR);
    outb_p(0x08, SIO_ADDR + 1);
    outb_p(0x30, SIO_ADDR);
    val = inb_p(SIO_ADDR + 1);
    outb_p(0x07, SIO_ADDR);
    outb_p(0x08, SIO_ADDR + 1);
    outb_p(0x30, SIO_ADDR);
    outb_p(val | 0x8, SIO_ADDR + 1);

    outb_p(0xAA, SIO_ADDR);

    gphost->baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x00;
    gphost->basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x02;
    /* Exit Configuration Mode */

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    sema_init(&gphost->lock, 0);
    #else
    init_MUTEX_LOCKED(&gphost->lock);
    #endif
    up(&gphost->lock);

    INIT_LIST_HEAD(&gphost->__gpios);

    for (loop = 0; sio_gpdev[loop].gpio_index != -1; loop++) {
        list_add_tail(&sio_gpdev[loop].list, &gphost->__gpios);
    }

    list_for_each_entry(pgpdev, &gphost->__gpios, list) {
        pgpdev->host  = gphost;
        pgpdev->read  = siogpdev_read;
        pgpdev->write = siogpdev_write;
    }

    return 0;
}

int siogphost_release (struct gpio_host *gphost) {
    return 0;
}

int siogpdev_read (struct gpio_device *gpdev) {
    unsigned char   val;
    int             retval = -1;

    down(&global_lock);
    down(&gpdev->host->lock);

    switch (gpdev->gpio_index) {
        case    1: //led_lcm_brightness
            {
                unsigned char val1,val2, hbyte;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                regbyte = AUXFANOUT0_RD;
                // Blue color
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                hbyte = inb((unsigned short int) (basedata & 0xFFFF));
                if(board_id == DESKTOP){
                    switch( hbyte ) {
                        case 25:
                            retval = 0;
                            break;
                        case 70:
                            retval = 1;
                            break;
                        case 255:
                            retval = 2;
                            break;
                        default:
                            retval = hbyte;
                            break;
                    }
                }else{//Rackmount
                    switch( hbyte ) {
                        case 30:
                            retval = 0;
                            break;
                        case 90:
                            retval = 1;
                            break;
                        case 255:
                            retval = 2;
                            break;
                        default:
                            retval = hbyte;
                            break;
                    }

                }
            }
            break;
    #ifdef _BRIGHTNESS_ADJUST
        case    2: //white_brightness
            {
                unsigned char val1,val2;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                regbyte = AUXFANOUT1_RD;

                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                retval = inb((unsigned short int) (basedata & 0xFFFF));
            }
            break;
        case    3: //blue_brightness
            {
                unsigned char val1,val2;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                regbyte = AUXFANOUT0_RD;

                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                retval = inb((unsigned short int) (basedata & 0xFFFF));
            }
            break;
        case    4: //red_brightness
            {
                unsigned char val1,val2;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                regbyte = AUXFANOUT2_RD;

                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                retval = inb((unsigned short int) (basedata & 0xFFFF));
            }
            break;
    #endif //end _BRIGHTNESS_ADJUST
        case    34: //lcm_backlight
            retval = lcm_backlight.status;
            break;
        case    35: //system_id_led
            retval = sys_id_led.status;
            break;

        case    44: //sys_blue
            {
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, (SIO_ADDR + 1));
                outb_p(0xF7, SIO_ADDR);
                val = inb(SIO_ADDR+1);
                outb_p(0xAA, SIO_ADDR);

                switch (val & 0x0F) {
                    case    7:
                        retval = 0;
                        break;
                    case    0:
                        retval = 1;
                        break;
                    case    6:
                        retval = 4;
                        break;
                    case    5:
                        retval = 3;
                        break;
                    case    4:
                        retval = 2;
                        break;
                    default:
                        retval = 0;
                }
            }
            break;
        case    45: //pwr
            {
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, (SIO_ADDR + 1));
                outb_p(0xF8, SIO_ADDR);
                val = inb((SIO_ADDR+1));
                outb_p(0xAA, SIO_ADDR);

                switch (val & 0x0F) {
                    case    7:
                        retval = 0;
                        break;
                    case    0:
                        retval = 1;
                        break;
                    case    6:
                        retval = 4;
                        break;
                    case    5:
                        retval = 3;
                        break;
                    case    4:
                        retval = 2;
                        break;
                    default:
                        retval = 0;
                }
            }
            break;
        case    71: //sys_led_red
            retval = sys_red_led.status;
            break;
        case    72: //hdd_led_red
            retval = hdd_red_led.status;
            break;
        default:
            retval = 0;
            break;
    }

    gpdev->gpio_value = retval;
    up(&gpdev->host->lock);
    up(&global_lock);

    return 0;
}

int siogpdev_write (struct gpio_device *gpdev, int value) {
    unsigned char   reg, val;
    int             res = 0;

    res = 0;
    down(&global_lock);
    down(&gpdev->host->lock);

    switch (gpdev->gpio_index) {
        case    1:
            {
                unsigned char val1,val2;
                unsigned char w_hbyte, b_hbyte, r_hbyte; //white, blue, red color hbyte.
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb_p(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                switch (value) {
                    case    0: //low
                        if(board_id == DESKTOP){
                            w_hbyte = 35;
                            b_hbyte = 25;
                            r_hbyte = 25;
                        }else{
                            w_hbyte = 40;
                            b_hbyte = 30;
                            r_hbyte = 30;
                        }
                        break;
                    case    1: //med
                        if(board_id == DESKTOP){
                            w_hbyte = 80;
                            b_hbyte = 70;
                            r_hbyte = 70;
                        }else{
                            w_hbyte = 100;
                            b_hbyte = 90;
                            r_hbyte = 90;
                        }
                        break;
                    case    2: //high
                        w_hbyte = 255;
                        b_hbyte = 255;
                        r_hbyte = 255;
                        break;
                    default:
                        w_hbyte = 255;
                        b_hbyte = 255;
                        r_hbyte = 255;
                        res = -EINVAL;
                        break;
                }

                // program Blue color brightness
                regbyte = AUXFANOUT0_WR;
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb(b_hbyte, (unsigned short int) (basedata & 0xFFFF));

                // program White color brightness
                regbyte = AUXFANOUT1_WR;
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb(w_hbyte, (unsigned short int) (basedata & 0xFFFF));

                // program Red color brightness
                regbyte = AUXFANOUT2_WR;
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb(r_hbyte, (unsigned short int) (basedata & 0xFFFF));
            }
            break;
    #ifdef _BRIGHTNESS_ADJUST
        case    2:
            {
                unsigned char val1,val2, hbyte;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb_p(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                hbyte=value;
                // program White color brightness
                regbyte = AUXFANOUT1_WR;
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb(hbyte, (unsigned short int) (basedata & 0xFFFF));

           }
            break;
        case    3:
            {
                unsigned char val1,val2, hbyte;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb_p(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                hbyte = value;
                // program Blue color brightness
                regbyte = AUXFANOUT0_WR;
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb(hbyte, (unsigned short int) (basedata & 0xFFFF));

            }
            break;
        case    4:
            {
                unsigned char val1,val2, hbyte;
                unsigned int baseaddr, basedata, regbyte;

                /* Enter Configuration Mode */
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p(0x60, SIO_ADDR);
                val1 = inb_p(SIO_ADDR + 1);
                outb_p(0x61, SIO_ADDR);
                val2 = inb_p(SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);

                baseaddr = (((val1 << 0x08) | val2) & 0xFFFF) + 0x05;
                basedata = (((val1 << 0x08) | val2) & 0xFFFF) + 0x06;

                hbyte=value;
                // program Red color brightness
                regbyte = AUXFANOUT2_WR;
                outb((unsigned char) (BANK_SEL & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb((unsigned char) ((regbyte >> 8) & 0xFF), (unsigned short int) (basedata & 0xFFFF));
                outb((unsigned char) (regbyte & 0xFF), (unsigned short int) (baseaddr & 0xFFFF));
                outb(hbyte, (unsigned short int) (basedata & 0xFFFF));
            }
            break;
    #endif //end _BRIGHTNESS_ADJUST
         case    34:
            {
                switch(value) {
                    case    0:
                        lcm_backlight.status = value;
                        outb( 0x03, gpdev->host->baseaddr);  // select GPIO3
                        val = ( inb(gpdev->host->basedata) & 0xEF);
                        outb( val, gpdev->host->basedata);
                        break;
                    case    1:
                        lcm_backlight.status = value;
                        outb( 0x03, gpdev->host->baseaddr);  // select GPIO3
                        val = ( inb(gpdev->host->basedata) | 0x10);
                        outb( val, gpdev->host->basedata);
                        break;
                    default:
                        up(&gpdev->host->lock);
                        up(&global_lock);
                        return -EINVAL;
                }
            }
            break;
        case    35:
            {
                switch(value) {
                    case     0:
                    case     1:
                    case     2:
                    case     3:
                    case     4:
                        sys_id_led.status = value;
                        break;
                    default:
                        up(&gpdev->host->lock);
                        up(&global_lock);
                        return -EINVAL;
                }
            }
            break;
        case    44:
            {
                reg = 0xF7;
                switch(value) {
                    case    0:
                        val = 0x07;
                        break;
                    case    1:
                        val = 0x00;
                        break;
                    case    2:
                        val = 0x04;
                        break;
                    case    3:
                        val = 0x05;
                        break;
                    case    4:
                        val = 0x06;
                        break;
                    default:
                        up(&gpdev->host->lock);
                        up(&global_lock);
                        return -EINVAL;
                }
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p((unsigned char) (reg & 0xFF), SIO_ADDR);
                outb_p(val, SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);
            }
            break;
        case    45:
            {
                reg = 0xF8;
                switch(value) {
                    case    0:
                        val = 0x07;
                        break;
                    case    1:
                        val = 0x00;
                        break;
                    case    2:
                        val = 0x04;
                        break;
                    case    3:
                        val = 0x05;
                        break;
                    case    4:
                        val = 0x06;
                        break;
                    default:
                        up(&gpdev->host->lock);
                        up(&global_lock);
                        return -EINVAL;
                }
                outb_p(0x87, SIO_ADDR);
                outb_p(0x87, SIO_ADDR);
                outb_p(0x07, SIO_ADDR);
                outb_p(0x0B, SIO_ADDR + 1);
                outb_p((unsigned char) (reg & 0xFF), SIO_ADDR);
                outb_p(val, SIO_ADDR + 1);
                outb_p(0xAA, SIO_ADDR);
            }
            break;
        case    71:
            {
                switch(value) {
                    case    0:
                    case    1:
                    case    2:
                    case    3:
                    case    4:
                        sys_red_led.status = value;
                        break;
                    default:
                        up(&gpdev->host->lock);
                        up(&global_lock);
                        return -EINVAL;
                }
            }
            break;
        case    72:
            {
                switch(value) {
                    case     0:
                    case     1:
                    case     2:
                    case     3:
                    case     4:
                        hdd_red_led.status = value;
                        break;
                    default:
                        up(&gpdev->host->lock);
                        up(&global_lock);
                        return -EINVAL;
                }
            }
            break;
        default:
            break;
    }

    up(&gpdev->host->lock);
    up(&global_lock);
    return res;
}

struct gpio_device *siogphost_search(int index) {

    struct gpio_device *pgpdev, *target = NULL;

    if (!list_empty(&gpiosio_host.__gpios))
    {
        list_for_each_entry(pgpdev, &gpiosio_host.__gpios, list)
        {
            if (pgpdev->gpio_index == index)
                target = pgpdev;
        }
    }

    return target;
}

static void legacy_timer_emu_func(struct timer_list *t)
{
	struct legacy_timer_emu *lt = from_timer(lt, t, t);
	lt->function(lt->data);
}

static void sys_led_timer_proc(unsigned long data)
{

    unsigned char val;
    struct gpio_host *gphost = (struct gpio_host*) data;

    outb( 0x07, gphost->baseaddr);  //select GPIO7
    if(sys_red_led.blinking==1 || sys_red_led.status == 0){
        val = ( inb(gphost->basedata) & 0xFD);
        sys_red_led.blinking=0;
    }else if( sys_red_led.status == 1) {
        val = ( inb(gphost->basedata) | 0x02);
        sys_red_led.blinking=0;
    }else {
        val = ( inb(gphost->basedata) | 0x02);
        sys_red_led.blinking=1;
    }
    outb( val, gphost->basedata);

    switch(sys_red_led.status) {
        case    0:
        case    1:
        case    4:
            mod_timer(&sys_red_led_timer.t, jiffies + (HZ/4));  //0.25 sec
            break;
        case    3:
            mod_timer(&sys_red_led_timer.t, jiffies + (HZ/2));  //0.5 sec
            break;
        case    2:
            mod_timer(&sys_red_led_timer.t, jiffies + (HZ*1));  //1 sec
            break;
    }
}

static void hdd_led_timer_proc(unsigned long data)
{
    unsigned char val;
    struct gpio_host *gphost = (struct gpio_host*) data;

    outb( 0x07, gphost->baseaddr);  // select GPIO7
    if(hdd_red_led.blinking==1 || hdd_red_led.status == 0){
        val = ( inb(gphost->basedata) & 0xFB);
        hdd_red_led.blinking=0;
    }else if( hdd_red_led.status == 1) {
        val = ( inb(gphost->basedata) | 0x04);
        hdd_red_led.blinking=0;
    }else {
        val = ( inb(gphost->basedata) | 0x04);
        hdd_red_led.blinking=1;
    }
    outb( val, gphost->basedata);

    switch(hdd_red_led.status) {
        case    0:
        case    1:
        case    4:
            mod_timer(&hdd_red_led_timer.t, jiffies + (HZ/4));  //0.25 sec
            break;
        case    3:
            mod_timer(&hdd_red_led_timer.t, jiffies + (HZ/2));  //0.5 sec
            break;
        case    2:
            mod_timer(&hdd_red_led_timer.t, jiffies + (HZ*1));  //1 sec
            break;
    }
}

static void sys_id_led_timer_proc(unsigned long data)
{
    unsigned char val;
    struct gpio_host *gphost = (struct gpio_host*) data;

    outb( 0x03, gphost->baseaddr);  // select GPIO3
    if(sys_id_led.blinking==1 || sys_id_led.status == 0){
        val = ( inb(gphost->basedata) & 0xDF);
        sys_id_led.blinking=0;
    }else if( sys_id_led.status == 1) {
        val = ( inb(gphost->basedata) | 0x20);
        sys_id_led.blinking=0;
    }else {
        val = ( inb(gphost->basedata) | 0x20);
        sys_id_led.blinking=1;
    }
    outb( val, gphost->basedata);

    switch(sys_id_led.status) {
        case    0:
        case    1:
        case    4:
            mod_timer(&sys_id_led_timer.t, jiffies + (HZ/4));  //0.25 sec
            break;
        case    3:
            mod_timer(&sys_id_led_timer.t, jiffies + (HZ/2));  //0.5 sec
            break;
        case    2:
            mod_timer(&sys_id_led_timer.t, jiffies + (HZ*1));  //1 sec
            break;
    }
}

// static void empty_led_timer_proc(struct timer_list *timerReturn)
// {
//
// }

int siogpio_init(void) {
    int retval = 0;
    struct gpio_host *gphost = &gpiosio_host;

    if ((retval = gphost->probe(gphost)) != 0){
        WIXVPRINT("SuperI/O GPIO Function doesn't support registered.\n");
    }

    sys_red_led_timer.function = sys_led_timer_proc;
    sys_red_led_timer.data = (unsigned long) &gpiosio_host;
    //add timer for sys_red_led
    timer_setup(&sys_red_led_timer.t, legacy_timer_emu_func, 0);
    add_timer(&sys_red_led_timer.t);
    mod_timer(&sys_red_led_timer.t, jiffies + (HZ*1));

    hdd_red_led_timer.function = hdd_led_timer_proc;
    hdd_red_led_timer.data = (unsigned long)&gpiosio_host;
    //add timer for hdd_red_led
    timer_setup(&hdd_red_led_timer.t, legacy_timer_emu_func, 0);
    add_timer(&hdd_red_led_timer.t);
    mod_timer(&hdd_red_led_timer.t, jiffies + (HZ*1));

    sys_id_led_timer.function = sys_id_led_timer_proc;
    sys_id_led_timer.data = (unsigned long)&gpiosio_host;
    //add timer for sys_id_led
    timer_setup(&sys_id_led_timer.t, legacy_timer_emu_func, 0);
    add_timer(&sys_id_led_timer.t);
    mod_timer(&sys_id_led_timer.t, jiffies + (HZ*1));


    return retval;
}

void siogpio_exit(void) {
    int retval = 0;
    struct gpio_host *gphost = &gpiosio_host;

    WIXPRINT("Removing SuperI/O GPIO driver...\n");
    del_timer_sync(&hdd_red_led_timer.t);
    del_timer_sync(&sys_red_led_timer.t);
    del_timer_sync(&sys_id_led_timer.t);

    if (gphost->release != NULL) {
        if ((retval = gphost->release(gphost)) != 0)
            WIXVPRINT("SuperI/O GPIO Function doesn't support unregistered. [%d]\n", retval);
    }
    return;
}
