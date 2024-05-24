/*
 * Export SMBIOS/DMI info via sysfs to userspace
 *
 * Copyright 2007, Lennart Poettering
 *
 * Licensed under GPLv2
 */
#include <linux/capability.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include "b4b.h"
#include "b4bm.h"
#include "nct677xf.h"
#include "wdt.h"
#include "led.h"
#include "smbus.h"
#include "sci.h"
#include "gpiopch.h"
//#include "wkthread.h"
#include "lcm.h"

#define DRV_VERSION  "1.0.2"

//MODULE_LICENSE("GPL");

int board_id=0; //board identify flag

struct semaphore global_lock; // global lock

//HWM Definition
struct hwm_host ncthwm_host = {
    .probe          = ncthwmh_probe,
    .release        = ncthwmh_release,
    .type           = HWMHOST_TYPE_NCT677XF,
};

struct hwm_device hwmdev[] = {
{.hwm_type = HWM_TYPE_FAN,   .hwm_index = 1, .hwm_value = 0},
{.hwm_type = HWM_TYPE_FAN,   .hwm_index = 2, .hwm_value = 0},
{.hwm_type = HWM_TYPE_FAN,   .hwm_index = 3, .hwm_value = 0},
{.hwm_type = HWM_TYPE_FAN,   .hwm_index = 4, .hwm_value = 0},
{.hwm_type = HWM_TYPE_VOLT,  .hwm_index = 1, .hwm_value = 0},
{.hwm_type = HWM_TYPE_VOLT,  .hwm_index = 4, .hwm_value = 0},
{.hwm_type = HWM_TYPE_VOLT,  .hwm_index = 5, .hwm_value = 0},
{.hwm_type = HWM_TYPE_VOLT,  .hwm_index = 7, .hwm_value = 0},
{.hwm_type = HWM_TYPE_VOLT,  .hwm_index = 8, .hwm_value = 0},
{.hwm_type = HWM_TYPE_VOLT,  .hwm_index = 9, .hwm_value = 0},
{.hwm_type = HWM_TYPE_THERM, .hwm_index = 1, .hwm_value = 0},
{.hwm_type = HWM_TYPE_THERM, .hwm_index = 2, .hwm_value = 0},
{.hwm_type = HWM_TYPE_OTHER, .hwm_index = 1, .hwm_value = 0},
{.hwm_type = -1, .hwm_index = -1},
};

struct hwm_host wdt_host = {
    .probe          = wdt_probe,
    .release        = wdt_release,
    .baseaddr       = SIO_ADDR,
    .basedata       = SIO_ADDR + 1,
    .type           = HWMHOST_TYPE_WDT,
};

struct hwm_device wdtdev[] = {
{.hwm_type = HWM_TYPE_WDT, .hwm_index = 1, .hwm_value = 0},
{.hwm_type = -1, .hwm_index = -1},
};


DEFINE_B4B_HWM_ATTR(sysfan1speed,   0644, &hwmdev[0]);
DEFINE_B4B_HWM_ATTR(sysfan2speed,   0644, &hwmdev[1]);
DEFINE_B4B_HWM_ATTR(sysfan3speed,   0644, &hwmdev[2]);
DEFINE_B4B_HWM_ATTR(sysfan4speed,   0644, &hwmdev[3]);
DEFINE_B4B_HWM_ATTR(vcore,   0444, &hwmdev[4]);
DEFINE_B4B_HWM_ATTR(12vin,   0444, &hwmdev[5]);
DEFINE_B4B_HWM_ATTR(5vin,    0444, &hwmdev[6]);
DEFINE_B4B_HWM_ATTR(ddr,     0444, &hwmdev[7]);
DEFINE_B4B_HWM_ATTR(3vin,    0444, &hwmdev[8]);
DEFINE_B4B_HWM_ATTR(vbat,    0444, &hwmdev[9]);
DEFINE_B4B_HWM_ATTR(hddtemp, 0444, &hwmdev[10]);
DEFINE_B4B_HWM_ATTR(cputemp, 0444, &hwmdev[11]);
DEFINE_B4B_HWM_ATTR(sysidentify,    0444, &hwmdev[12]);
DEFINE_B4B_HWM_ATTR(wdt,     0644, &wdtdev[0]);

static struct class b4b_hwm_class = {
    .name = "nasmonitor",
    .dev_release = (void(*)(struct device *)) kfree,
};

static ssize_t sys_b4b_hwm_show(struct class *class, struct class_attribute *attr, char *buf) {
    struct hwm_device *phwmdev = to_b4b_hwm_class_attr(attr)->hwmd;
    ssize_t len;
    int     res;

    if (phwmdev->read) {
        if ((res = phwmdev->read(phwmdev)) != 0)
            phwmdev->hwm_value = 0;
    }

    len = sprintf(buf, "%d\n", phwmdev->hwm_value);
    return len;
}

static ssize_t sys_b4b_hwm_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count) {
    struct hwm_device  *phwmdev = to_b4b_hwm_class_attr(attr)->hwmd;
    long  lvl = 0;
    int   value = 0;
    int   res;

    if ((res = kstrtol(buf, 10, &lvl)) != 0)
        return -EINVAL;

    value = simple_strtol(buf, NULL, 10);
    if (phwmdev->write) {
        if ((res = phwmdev->write(phwmdev, value)) != 0) {
            phwmdev->hwm_value = 0;
            return res;
        }
    }

    return count;
}

//LED Definition
struct led_host sysled_host = {
    .probe          = led_probe,
    .release        = led_release,
    .baseaddr       = 0,
    .basedata       = 0,
    .type           = LEDHOST_TYPE_SYSTEM,
};

struct led_device sysleddev[] = {
{.type = LEDDEV_TYPE_SIO, .index = 1,  .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 34, .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 35, .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 44, .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 45, .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 71, .value = 0},
#ifdef _BRIGHTNESS_ADJUST
{.type = LEDDEV_TYPE_SIO, .index = 2,  .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 3,  .value = 0},
{.type = LEDDEV_TYPE_SIO, .index = 4,  .value = 0},
#endif
{.type = -1, .index = -1},
};

struct led_host stshddled_host = {
    .probe          = led_probe,
    .release        = led_release,
    .baseaddr       = 0,
    .basedata       = 0,
    .type           = LEDHOST_TYPE_HDD_STS,
};

struct led_device stshddleddev[] = {
{.type = LEDDEV_TYPE_PCH, .index = 4, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 5, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 13, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 17, .value = 0},
{.type = -1, .index = -1},
};


struct led_host faulthddled_host = {
    .probe          = led_probe,
    .release        = led_release,
    .baseaddr       = 0,
    .basedata       = 0,
    .type           = LEDHOST_TYPE_HDD_FAULT,
};

struct led_device faulthddleddev[] = {
{.type = LEDDEV_TYPE_SIO, .index = 72, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 19, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 21, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 22, .value = 0},
{.type = LEDDEV_TYPE_PCH, .index = 36, .value = 0},
{.type = -1, .index = -1},
};

DEFINE_B4B_LED_ATTR(led_lcd_brightness, 0644, &sysleddev[0]);
DEFINE_B4B_LED_ATTR(lcd_backlight,  0644, &sysleddev[1]);
DEFINE_B4B_LED_ATTR(sys_id_led,     0644, &sysleddev[2]);
DEFINE_B4B_LED_ATTR(sys_blue_led,   0644, &sysleddev[3]);
DEFINE_B4B_LED_ATTR(pwr_led,        0644, &sysleddev[4]);
DEFINE_B4B_LED_ATTR(sys_red_led,    0644, &sysleddev[5]);
#ifdef _BRIGHTNESS_ADJUST
DEFINE_B4B_LED_ATTR(white_brightness, 0644, &sysleddev[6]);
DEFINE_B4B_LED_ATTR(blue_brightness,  0644, &sysleddev[7]);
DEFINE_B4B_LED_ATTR(red_brightness,   0644, &sysleddev[8]);
#endif

DEFINE_B4B_LED_ATTR(hdd1_blue_led,  0644, &stshddleddev[0]);
DEFINE_B4B_LED_ATTR(hdd2_blue_led,  0644, &stshddleddev[1]);
DEFINE_B4B_LED_ATTR(hdd3_blue_led,  0644, &stshddleddev[2]);
DEFINE_B4B_LED_ATTR(hdd4_blue_led,  0644, &stshddleddev[3]);

DEFINE_B4B_LED_ATTR(hdd_red_led,    0644, &faulthddleddev[0]);
DEFINE_B4B_LED_ATTR(hdd1_red_led,   0644, &faulthddleddev[1]);
DEFINE_B4B_LED_ATTR(hdd2_red_led,   0644, &faulthddleddev[2]);
DEFINE_B4B_LED_ATTR(hdd3_red_led,   0644, &faulthddleddev[3]);
DEFINE_B4B_LED_ATTR(hdd4_red_led,   0644, &faulthddleddev[4]);


static struct class b4b_led_class = {
    .name = "nasled",
    .dev_release = (void(*)(struct device *)) kfree,
};

static ssize_t sys_b4b_led_show(struct class *class, struct class_attribute *attr, char *buf) {
    struct led_device  *pleddev = to_b4b_led_class_attr(attr)->ledd;
    ssize_t  len;
    int      res;

    if (pleddev->read) {
        if ((res = pleddev->read(pleddev)) != 0)
            pleddev->value = 0;
    }

    len = sprintf(buf, "%d\n", pleddev->value);
    return len;
}

static ssize_t sys_b4b_led_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count) {
    struct led_device  *pleddev = to_b4b_led_class_attr(attr)->ledd;
    long    lvl = 0;
    int     value = 0;
    int     res;

    if ((res = kstrtol(buf, 10, &lvl)) != 0)
        return -EINVAL;

    value  = simple_strtol(buf, NULL, 10);
    if (pleddev->write) {
        if ((res = pleddev->write(pleddev, value)) != 0) {
            pleddev->value = 0;
            return res;
        }
    }

    return count;
}

struct hwm_device *searchHWM(int index) {
    int    loop;
    struct hwm_device  *dev = NULL;
    int    type = 0, id = 0;

    switch (index) {
        case    1:
            type = HWM_TYPE_FAN;
            id = 1;
            break;
        case    2:
            type = HWM_TYPE_FAN;
            id = 2;
            break;
        case    3:
            type = HWM_TYPE_FAN;
            id = 3;
            break;
        case    4:
            type = HWM_TYPE_FAN;
            id = 4;
            break;
        case    5:
            type = HWM_TYPE_THERM;
            id = 1;
            break;
        case    6:
        case    8:
            break;
        case    7:
        case    9:
            break;
        case    10:
        case    11:
            break;
        default:
            return NULL;
            break;
    }

    switch (index) {
        case    1:
        case    2:
        case    3:
        case    4:
        case    5:
            {
                for (loop = 0; hwmdev[loop].hwm_type != -1; loop++) {
                    if ((hwmdev[loop].hwm_type == type) && (hwmdev[loop].hwm_index == id)) {
                        dev = &hwmdev[loop];
                        break;
                    }
                }
            }
            break;
        case    6:
        case    7:
        case    10:
            break;
        case    8:
        case    9:
        case    11:
            break;
    }

    WIXPRINT("[Type %X]\n", dev->hwm_type);

    return dev;
}

static int __init b4b_init(void) {
    int  loop, ret;
    struct class_attribute *hwm[] = {
        &sys_b4b_hwm_sysfan1speed_attr.class_attr,
        &sys_b4b_hwm_sysfan2speed_attr.class_attr,
        &sys_b4b_hwm_sysfan3speed_attr.class_attr,
        &sys_b4b_hwm_sysfan4speed_attr.class_attr,
        &sys_b4b_hwm_vcore_attr.class_attr,
        &sys_b4b_hwm_12vin_attr.class_attr,
        &sys_b4b_hwm_5vin_attr.class_attr,
        &sys_b4b_hwm_ddr_attr.class_attr,
        &sys_b4b_hwm_3vin_attr.class_attr,
        &sys_b4b_hwm_vbat_attr.class_attr,
        &sys_b4b_hwm_cputemp_attr.class_attr,
        &sys_b4b_hwm_hddtemp_attr.class_attr,
        &sys_b4b_hwm_sysidentify_attr.class_attr,
        &sys_b4b_hwm_wdt_attr.class_attr,
        NULL};

    struct class_attribute *led[] = {
        &sys_b4b_led_led_lcd_brightness_attr.class_attr,
        &sys_b4b_led_lcd_backlight_attr.class_attr,
        &sys_b4b_led_sys_id_led_attr.class_attr,
        &sys_b4b_led_sys_blue_led_attr.class_attr,
        &sys_b4b_led_pwr_led_attr.class_attr,
        &sys_b4b_led_sys_red_led_attr.class_attr,
        #ifdef _BRIGHTNESS_ADJUST
        &sys_b4b_led_white_brightness_attr.class_attr,
        &sys_b4b_led_blue_brightness_attr.class_attr,
        &sys_b4b_led_red_brightness_attr.class_attr,
        #endif
        &sys_b4b_led_hdd1_blue_led_attr.class_attr,
        &sys_b4b_led_hdd2_blue_led_attr.class_attr,
        &sys_b4b_led_hdd3_blue_led_attr.class_attr,
        &sys_b4b_led_hdd4_blue_led_attr.class_attr,
        &sys_b4b_led_hdd_red_led_attr.class_attr,
        &sys_b4b_led_hdd1_red_led_attr.class_attr,
        &sys_b4b_led_hdd2_red_led_attr.class_attr,
        &sys_b4b_led_hdd3_red_led_attr.class_attr,
        &sys_b4b_led_hdd4_red_led_attr.class_attr,
        NULL};


    sema_init(&global_lock,0);
    up(&global_lock);
    /* SMBUS Initialization */
    if ((ret = smbus_init()) != 0) {
        return ret;
    }

    /* PCH GPIO Initialization */
    if ((ret = pchgpio_init()) != 0) {
        return ret;
    }

    /* LCM Initialization */
    if ((ret = wixlcm_init()) != 0) {
        return ret;
    }

    /* LED Initialization */
    if ((ret = led_init()) != 0) {
        return ret;
    }
    if ((ret = class_register(&b4b_led_class)) != 0) {
        return ret;
    }

    if (sysled_host.probe != NULL) {
        if ((ret = sysled_host.probe(&sysled_host, sysleddev)) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            return ret;
        }
    }

    if (stshddled_host.probe != NULL) {
        if ((ret = stshddled_host.probe(&stshddled_host, stshddleddev)) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            return ret;
        }
    }

    if (faulthddled_host.probe != NULL) {
        if ((ret = faulthddled_host.probe(&faulthddled_host, faulthddleddev)) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            return ret;
        }
    }

    for (loop = 0; led[loop] != NULL; loop++) {
        if ((ret = class_create_file(&b4b_led_class, led[loop])) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            return ret;
        }
    }

    /* HWM Initialization */
    if ((ret = hwm_init()) != 0) {
        return ret;
    }

    if ((ret = class_register(&b4b_hwm_class)) != 0) {
        class_unregister(&b4b_led_class);
        led_exit();
        hwm_exit();
        return ret;
    }

    if (ncthwm_host.probe != NULL) {
        if ((ret = ncthwm_host.probe(&ncthwm_host, hwmdev)) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            hwm_exit();
            return ret;
        }
    }

    if (wdt_host.probe != NULL) {
        if ((ret = wdt_host.probe(&wdt_host, wdtdev)) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            hwm_exit();
            return ret;
        }
    }

    for (loop = 0; hwm[loop] != NULL; loop++) {
        if ((ret = class_create_file(&b4b_hwm_class, hwm[loop])) != 0) {
            class_unregister(&b4b_led_class);
            led_exit();
            class_unregister(&b4b_hwm_class);
            hwm_exit();
            return ret;
        }
    }
    if ((ret = wixsci_init()) != 0) {
        class_unregister(&b4b_led_class);
        led_exit();
        class_unregister(&b4b_hwm_class);
        hwm_exit();
        pchgpio_exit();
        smbus_exit();
        return ret;
    }
#if 0
    if ((ret = wixkthread_init()) != 0) {
        wixsci_exit();
        class_unregister(&b4b_led_class);
        led_exit();
        class_unregister(&b4b_hwm_class);
        hwm_exit();
        pchgpio_exit();
        smbus_exit();
        return ret;
    }
#endif
    WIXVPRINT("Load B4B Driver, version %s...", DRV_VERSION);
    return 0;
}

static void __exit b4b_exit(void) {
    wixlcm_exit();
//    wixkthread_exit();
    wixsci_exit();
    class_unregister(&b4b_led_class);
    led_exit();
    class_unregister(&b4b_hwm_class);
    hwm_exit();
    pchgpio_exit();
    smbus_exit();
    return;
}

module_init(b4b_init);
module_exit(b4b_exit);

MODULE_DESCRIPTION("PX4-400D/PX4-400R device driver");
MODULE_AUTHOR("Wistron Inc.");
MODULE_VERSION(DRV_VERSION);
MODULE_LICENSE("GPL");
