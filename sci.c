/*
        sci.c - General SCI module

        Copyright (c) 2008 - 2011  Wistron Corp.

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
#include <linux/version.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
//#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35)
    #include <linux/smp_lock.h>
#endif
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/reboot.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/kmod.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <asm/signal.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgalloc.h>
#include <linux/delay.h>
#include <linux/acpi.h>
#include <linux/cdev.h>
#include "b4b.h"
#include "gpiopch.h"
#include "sci.h"
#include "hwm.h"

//MODULE_LICENSE("GPL");

#define DRV_NAME            "sci"
#define SCI_DRV_VERSION     "0.6"

#define LPC_OFFSET_PMBASE       0x40
#define LPC_OFFSET_GPIOBASE     0x48

#define ACPI_OFFSET_PM1_STS     0x00
#define ACPI_OFFSET_PM1_EN      0x02
#define ACPI_OFFSET_PM1_CNT     0x04
#define ACPI_OFFSET_PM1_TMR     0x08
#define ACPI_OFFSET_PROC_CNT    0x10
#define ACPI_OFFSET_LV2         0x14
#define ACPI_OFFSET_LV3         0x15
#define ACPI_OFFSET_LV4         0x16
#define ACPI_OFFSET_LV5         0x17
#define ACPI_OFFSET_LV6         0x18
#define ACPI_OFFSET_GPE0_STS    0x20
#define ACPI_OFFSET_GPE0_EN     0x28
#define ACPI_OFFSET_SMI_EN      0x30
#define ACPI_OFFSET_SMI_STS     0x34

#define WIXSCI_DEV_BTN          0x01
#define WIXSCI_DEV_HWE          0x02


// Because PX4-400D and PX4-400R scroll and select button define has reversed.
#define BTN_WORKAROUND 1

#if BTN_WORKAROUND
extern int board_id;

int WIXSCI_OFFSET_BTN=0;
int WIXSCI_OFFSET_LCM_SEL=0;
int WIXSCI_OFFSET_LCM_SCR=0;
int WIXSCI_OFFSET_PWR=0;
#else
#define WIXSCI_OFFSET_BTN       0
#define WIXSCI_OFFSET_LCM_SEL   2
#define WIXSCI_OFFSET_LCM_SCR   3
#define WIXSCI_OFFSET_PWR       10
#endif

typedef struct {
    struct list_head    list;
    int                 event;
} wixEventList;

typedef struct {
    struct semaphore    lock;
    int                 type;
    int                 index;
    unsigned long       jiffies;
    struct work_struct  workqueue;
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    void (*tasklet)     (struct work_struct *work);
    #else
    void (*tasklet)     (unsigned long sci_index);
    #endif
} wixSCIHandler;

typedef struct {
    struct semaphore    lock;
    int                 type;
    struct file_operations  fops;
    struct class            *wclass;
    struct cdev             wcdev;
    struct device           *wdev;
    dev_t                   wdevt;
    struct list_head        elist;
    wait_queue_head_t       wq;
} wixSCIDev;

/* For ACPI GPE-Type Interrupts */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
unsigned int wix_acpi_gpe_handler(acpi_handle gpe_device, u32 gpe_number, void *context);
#else
unsigned int wix_acpi_gpe_handler(void *context);
#endif

#if BTN_WORKAROUND
int    acpi_idx_table[] = {0,0,0,0,-1}; // initial value
#else
int    acpi_idx_table[] = {WIXSCI_OFFSET_BTN, WIXSCI_OFFSET_LCM_SEL, WIXSCI_OFFSET_LCM_SCR, WIXSCI_OFFSET_PWR, -1};
#endif

/* Base FOPS function */
ssize_t wix_base_read(struct file * file, char * buf, size_t count, loff_t *ppos, wixSCIDev *wdev);
ssize_t wix_base_write(struct file * file, const char * buf, size_t count, loff_t *ppos, wixSCIDev *wdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long wix_base_ioctl( struct file *file, unsigned int cmd, unsigned long arg, wixSCIDev *wdev);
#else
int  wix_base_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg, wixSCIDev *wdev);
#endif

/* Button Device */
ssize_t wix_btn_read(struct file * file, char * buf, size_t count, loff_t *ppos);
ssize_t wix_btn_write(struct file * file, const char * buf, size_t count, loff_t *ppos);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long wix_btn_ioctl( struct file *file, unsigned int cmd, unsigned long arg);
#else
int wix_btn_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif

wixSCIDev wixDevBtn = {
    .type   = WIXSCI_DEV_BTN,
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    .fops   = {.owner = THIS_MODULE, .read = wix_btn_read, .write = wix_btn_write, .unlocked_ioctl = wix_btn_ioctl},
    #else
    .fops   = {.owner = THIS_MODULE, .read = wix_btn_read, .write = wix_btn_write, .ioctl = wix_btn_ioctl},
    #endif
    .wq     = __WAIT_QUEUE_HEAD_INITIALIZER(wixDevBtn.wq),
    .elist  = LIST_HEAD_INIT(wixDevBtn.elist),
};

/* HWM Device */
ssize_t wix_hwe_read(struct file * file, char * buf, size_t count, loff_t *ppos);
ssize_t wix_hwe_write(struct file * file, const char * buf, size_t count, loff_t *ppos);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long wix_hwe_ioctl( struct file *file, unsigned int cmd, unsigned long arg);
#else
int wix_hwe_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif

wixSCIDev wixDevHwe = {
    .type   = WIXSCI_DEV_HWE,
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    .fops   = {.owner = THIS_MODULE, .read = wix_hwe_read, .write = wix_hwe_write, .unlocked_ioctl = wix_hwe_ioctl},
    #else
    .fops   = {.owner = THIS_MODULE, .read = wix_hwe_read, .write = wix_hwe_write, .ioctl = wix_hwe_ioctl},
    #endif
    .wq     = __WAIT_QUEUE_HEAD_INITIALIZER(wixDevHwe.wq),
    .elist  = LIST_HEAD_INIT(wixDevHwe.elist),
};

/* Handler ~ BTN */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_btn_tasklet (struct work_struct *work);
#else
void wix_btn_tasklet (unsigned long sci_index);
#endif

#if BTN_WORKAROUND
wixSCIHandler wixHndBtn;
#else
wixSCIHandler wixHndBtn = {
    .type   = WIXSCI_OFFSET_BTN,
    .index  = WIXSCI_OFFSET_BTN,
    .tasklet= wix_btn_tasklet,
};
#endif

/* Handler ~ PowerButton */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_powerbtn_tasklet (struct work_struct *work);
#else
void wix_powerbtn_tasklet (unsigned long sci_index);
#endif

#if BTN_WORKAROUND
wixSCIHandler wixHndPwr;
#else
wixSCIHandler wixHndPwr = {
    .type   = WIXSCI_OFFSET_PWR,
    .index  = WIXSCI_OFFSET_PWR,
    .tasklet= wix_powerbtn_tasklet,
};
#endif

/* Handler ~ LCM SelectButton */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_selbtn_tasklet (struct work_struct *work);
#else
void wix_selbtn_tasklet (unsigned long sci_index);
#endif

#if BTN_WORKAROUND
wixSCIHandler wixHndSel;
#else
wixSCIHandler wixHndSel = {
    .type   = WIXSCI_OFFSET_LCM_SEL,
    .index  = WIXSCI_OFFSET_LCM_SEL,
    .tasklet= wix_selbtn_tasklet,
};
#endif

/* Handler ~ LCM ScrollButton */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_scrbtn_tasklet (struct work_struct *work);
#else
void wix_scrbtn_tasklet (unsigned long sci_index);
#endif

#if BTN_WORKAROUND
wixSCIHandler wixHndScr;
#else
wixSCIHandler wixHndScr = {
    .type   = WIXSCI_OFFSET_LCM_SCR,
    .index  = WIXSCI_OFFSET_LCM_SCR,
    .tasklet= wix_scrbtn_tasklet,
};
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
static irqreturn_t wixsci_isr(int irq, void *dev_id);
#else
static irqreturn_t wixsci_isr(int irq, void *dev_id, struct pt_regs *reg);
#endif

unsigned int        acpi_base;
unsigned int        gpio_base;
int                 killed;
spinlock_t          slock;
struct semaphore    glock;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
unsigned int wix_acpi_gpe_handler(acpi_handle gpe_device, u32 gpe_number, void *context) {
#else
unsigned int wix_acpi_gpe_handler(void *context) {
#endif
    int *index = (int *) context;
    switch (*index)
    {
        case 0:     //WIXSCI_OFFSET_BTN
        case 10:    //WIXSCI_OFFSET_PWR
        case 2:     //WIXSCI_OFFSET_LCM_SEL or WIXSCI_OFFSET_LCM_SCR
        case 3:     //WIXSCI_OFFSET_LCM_SCR or WIXSCI_OFFSET_LCM_SEL
            break;
        default:
            break;
    }
    return ACPI_INTERRUPT_HANDLED;
}

ssize_t wix_base_read(struct file * file, char * buf, size_t count, loff_t *ppos, wixSCIDev *wdev) {
    unsigned long   left, written = 0;
    wixEventList    *next;
    int             *event;

    if (!count)
        return 0;
    if (!access_ok(buf, count))
        return -EFAULT;

    left = count;

    down(&wdev->lock);

    while ((!list_empty(&wdev->elist)) && (left >= sizeof(int))) {
        next = list_entry(wdev->elist.next, wixEventList, list);
        event = &next->event;
        if (copy_to_user(buf, event, sizeof(int))) {
            up(&wdev->lock);
            return -EFAULT;
        }
        written += sizeof(int);
        buf += sizeof(int);
        left -= sizeof(int);
        list_del(&next->list);
        kfree(next);
    }

    up(&wdev->lock);

    return written;
}

ssize_t wix_base_write(struct file * file, const char * buf, size_t count, loff_t *ppos, wixSCIDev *wdev) {
    unsigned long   left, read = 0;
    wixEventList    *next;

    if (!count)
        return 0;
    if (!access_ok(buf, count))
        return -EFAULT;
    left = count;
    if (left < sizeof(int)) {
        return left;
    }
    down(&wdev->lock);
    while (left >= sizeof(int)) {
        if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
            WIXVPRINT("Unable to allocate memory !\n");
            up(&wdev->lock);
            return -ENOMEM;
        }
        if (copy_from_user(&next->event, buf, sizeof(int))) {
            up(&wdev->lock);
            return -EFAULT;
        }
        read += sizeof(int);
        buf += sizeof(int);
        left -= sizeof(int);
        list_add_tail(&next->list, &wdev->elist);
        wake_up_interruptible(&wdev->wq);
    }
    up(&wdev->lock);
    return read;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long wix_base_ioctl( struct file *file, unsigned int cmd, unsigned long arg, wixSCIDev *wdev) {
#else
int wix_base_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg, wixSCIDev *wdev) {
#endif
    int             event = -1;
    wixEventList    *next;

    if (!access_ok((void __user *)arg, sizeof(int)))
        return -EFAULT;

    switch (cmd)
    {
        case READ_BUTTON_BLOCKING:
        case READ_HWE_BLOCKING:
            {
                if (list_empty(&wdev->elist))    {
                    if (wait_event_interruptible(wdev->wq, !list_empty(&wdev->elist))) {
                        return -ERESTARTSYS;
                    }
                }
                if (killed == 1)
                    return -ERESTARTSYS;
            }
            break;
        case READ_BUTTON_NONBLOCKING:
            {
                if (list_empty(&wdev->elist))    {
                    if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
                        WIXVPRINT("Unable to allocate memory !\n");
                        return -ERESTARTSYS;
                    }
                    next->event = WIX_NO_BTN;
                    down(&wdev->lock);
                     list_add_tail(&next->list, &wdev->elist);
                    wake_up_interruptible(&wdev->wq);
                    up(&wdev->lock);
                }
            }
            break;
        case READ_HWE_NONBLOCKING:
            break;
        default:
            return -1;
    }
    down(&wdev->lock);
    if (!list_empty(&wdev->elist)) {
        next = list_entry(wdev->elist.next, wixEventList, list);
            if (copy_to_user((int *) arg, &next->event, sizeof(int))) {
                    up(&wdev->lock);
                    return -EFAULT;
                }
        list_del(&next->list);
        kfree(next);
    } else {
        if (copy_to_user((int *) arg, &event, sizeof(int))) {
            up(&wdev->lock);
            return -EFAULT;
        }
    }
    up(&wdev->lock);
    return 0;
}


ssize_t wix_btn_read(struct file * file, char * buf, size_t count, loff_t *ppos) {
    if (!count)
        return 0;
    if (!access_ok(buf, count))
        return -EFAULT;
    return wix_base_read(file, buf, count, ppos, &wixDevBtn);
}

ssize_t wix_btn_write(struct file * file, const char * buf, size_t count, loff_t *ppos) {
    if (!count)
        return 0;
    if (!access_ok(buf, count))
        return -EFAULT;
    return wix_base_write(file, buf, count, ppos, &wixDevBtn);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long wix_btn_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
#else
int wix_btn_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
#endif
    if (!access_ok((void __user *)arg, sizeof(int)))
        return -EFAULT;
    switch (cmd) {
        case READ_BUTTON_NONBLOCKING:
        case READ_BUTTON_BLOCKING:
            break;
        default:
            return -EINVAL;
    }
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    return wix_base_ioctl(file, cmd, arg, &wixDevBtn);
    #else
    return wix_base_ioctl(inode, file, cmd, arg, &wixDevBtn);
    #endif
}

ssize_t wix_hwe_read(struct file * file, char * buf, size_t count, loff_t *ppos) {
    if (!count)
        return 0;
    if (!access_ok(buf, count))
        return -EFAULT;
    return wix_base_read(file, buf, count, ppos, &wixDevHwe);
}

ssize_t wix_hwe_write(struct file * file, const char * buf, size_t count, loff_t *ppos) {
    if (!count)
        return 0;
    if (!access_ok(buf, count))
        return -EFAULT;
    return wix_base_write(file, buf, count, ppos, &wixDevHwe);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long wix_hwe_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
#else
int wix_hwe_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) {
#endif
    if (!access_ok((void __user *)arg, sizeof(int)))
        return -EFAULT;
    switch (cmd) {
        case READ_HWE_NONBLOCKING:
        case READ_HWE_BLOCKING:
            break;
        default:
            return -EINVAL;
    }
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    return wix_base_ioctl(file, cmd, arg, &wixDevHwe);
    #else
    return wix_base_ioctl(inode, file, cmd, arg, &wixDevHwe);
    #endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_btn_tasklet (struct work_struct *work)
#else
void wix_btn_tasklet (unsigned long event)
#endif
{
    wixEventList    *next;
    unsigned long   ptime;
    wixSCIDev       *wdev = (wixSCIDev *) &wixDevBtn;
    wixSCIHandler   *whandler = (wixSCIHandler *) &wixHndBtn;
    int             state = 0;

    WIXPRINT("Enter [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));

    if (((inl(acpi_base + ACPI_OFFSET_GPE0_STS) >> (WIXSCI_OFFSET_BTN + 16)) & 0x01) == 0) {
        return;
    }

    ptime = jiffies;

    down(&glock);
    if (((inl(gpio_base + GPIO_OFFSET_INV) >> WIXSCI_OFFSET_BTN) & 0x01) == 0) {
        outl((inl(gpio_base + GPIO_OFFSET_INV) | (1 << WIXSCI_OFFSET_BTN)), (gpio_base + GPIO_OFFSET_INV));
        state = 1;
    } else {
        outl((inl(gpio_base + GPIO_OFFSET_INV) & (~(1 << WIXSCI_OFFSET_BTN))), (gpio_base + GPIO_OFFSET_INV));
        state = 2;
    }
    up(&glock);

    down(&whandler->lock);

    outl((0x01 << (WIXSCI_OFFSET_BTN + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));

    switch (state) {
        case 1:
            {
                if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
                    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_BTN + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                    WIXVPRINT("Unable to allocate memory !\n");
                    up(&whandler->lock);
                    return;
                }

                if (time_before(ptime, whandler->jiffies))
                    next->event = WIX_BTN_RESET_BRIEFLY;
                else
                    next->event = WIX_BTN_RESET_LONG;

                down(&wdev->lock);
                list_add_tail(&next->list, &wdev->elist);
                wake_up_interruptible(&wdev->wq);
                up(&wdev->lock);

            }
            break;
        case 2:
            {
                whandler->jiffies = ptime + (5 * HZ);
            }
            break;
        default:
            break;
    }

    WIXPRINT("Exit [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));
    outl((0x01 << (WIXSCI_OFFSET_BTN + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));
    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_BTN + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
    up(&whandler->lock);

    return;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_powerbtn_tasklet (struct work_struct *work)
#else
void wix_powerbtn_tasklet (unsigned long sci_index)
#endif
{
    wixEventList    *next;
    wixSCIDev       *wdev = (wixSCIDev *) &wixDevBtn;
    wixSCIHandler   *whandler = (wixSCIHandler *) &wixHndPwr;
    int             state = 0;

    WIXPRINT("Enter [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));

    if (((inl(acpi_base + ACPI_OFFSET_GPE0_STS) >> (WIXSCI_OFFSET_PWR + 16)) & 0x01) == 0) {
        return;
    }

    down(&glock);
    if (((inl(gpio_base + GPIO_OFFSET_INV) >> WIXSCI_OFFSET_PWR) & 0x01) == 0) {
        outl((inl(gpio_base + GPIO_OFFSET_INV) | (1 << WIXSCI_OFFSET_PWR)), (gpio_base + GPIO_OFFSET_INV));
        state = 1;
    } else {
        outl((inl(gpio_base + GPIO_OFFSET_INV) & (~(1 << WIXSCI_OFFSET_PWR))), (gpio_base + GPIO_OFFSET_INV));
        state = 2;
    }
    up(&glock);

    down(&whandler->lock);
    outl((0x01 << (WIXSCI_OFFSET_PWR + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));

    if (state == 2) {
        if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_PWR + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
            WIXVPRINT("Unable to allocate memory !\n");
            up(&whandler->lock);
            return;
        }

        next->event = WIX_BTN_POWER;
        down(&wdev->lock);
        list_add_tail(&next->list, &wdev->elist);
        wake_up_interruptible(&wdev->wq);
        up(&wdev->lock);
    }

    outl((0x01 << (WIXSCI_OFFSET_PWR + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));
    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_PWR + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
    WIXPRINT("Exit [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));
    up(&whandler->lock);

    return;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_selbtn_tasklet (struct work_struct *work)
#else
void wix_selbtn_tasklet (unsigned long sci_index)
#endif
{
    wixEventList    *next;
    wixSCIDev       *wdev = (wixSCIDev *) &wixDevBtn;
    wixSCIHandler   *whandler = (wixSCIHandler *) &wixHndSel;
    int             state = 0;

    WIXPRINT("Enter [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));

    if (((inl(acpi_base + ACPI_OFFSET_GPE0_STS) >> (WIXSCI_OFFSET_LCM_SEL + 16)) & 0x01) == 0) {
        return;
    }

    down(&glock);
    if (((inl(gpio_base + GPIO_OFFSET_INV) >> WIXSCI_OFFSET_LCM_SEL) & 0x01) == 0) {
        outl((inl(gpio_base + GPIO_OFFSET_INV) | (1 << WIXSCI_OFFSET_LCM_SEL)), (gpio_base + GPIO_OFFSET_INV));
        state = 1;
    } else {
        outl((inl(gpio_base + GPIO_OFFSET_INV) & (~(1 << WIXSCI_OFFSET_LCM_SEL))), (gpio_base + GPIO_OFFSET_INV));
        state = 2;
    }
    up(&glock);

    down(&whandler->lock);
    outl((0x01 << (WIXSCI_OFFSET_LCM_SEL + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));

    if (state == 2) {
        if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_LCM_SEL + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
            WIXVPRINT("Unable to allocate memory !\n");
            up(&whandler->lock);
            return;
        }

        next->event = WIX_BTN_LCM_SELECT;
        down(&wdev->lock);
        list_add_tail(&next->list, &wdev->elist);
        wake_up_interruptible(&wdev->wq);
        up(&wdev->lock);
    }

    outl((0x01 << (WIXSCI_OFFSET_PWR + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));
    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_LCM_SEL + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
    WIXPRINT("Exit [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));
    up(&whandler->lock);

    return;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
void wix_scrbtn_tasklet (struct work_struct *work)
#else
void wix_scrbtn_tasklet (unsigned long sci_index)
#endif
{
    wixEventList    *next;
    wixSCIDev       *wdev = (wixSCIDev *) &wixDevBtn;
    wixSCIHandler   *whandler = (wixSCIHandler *) &wixHndScr;
    int             state = 0;

    WIXPRINT("Enter [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));

    if (((inl(acpi_base + ACPI_OFFSET_GPE0_STS) >> (WIXSCI_OFFSET_LCM_SCR + 16)) & 0x01) == 0) {
        return;
    }

    down(&glock);
    if (((inl(gpio_base + GPIO_OFFSET_INV) >> WIXSCI_OFFSET_LCM_SCR) & 0x01) == 0) {
        outl((inl(gpio_base + GPIO_OFFSET_INV) | (1 << WIXSCI_OFFSET_LCM_SCR)), (gpio_base + GPIO_OFFSET_INV));
        state = 1;
    } else {
        outl((inl(gpio_base + GPIO_OFFSET_INV) & (~(1 << WIXSCI_OFFSET_LCM_SCR))), (gpio_base + GPIO_OFFSET_INV));
        state = 2;
    }
    up(&glock);

    down(&whandler->lock);
    outl((0x01 << (WIXSCI_OFFSET_LCM_SCR + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));

    if (state == 2) {
        if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_LCM_SCR + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
            WIXVPRINT("Unable to allocate memory !\n");
            up(&whandler->lock);
            return;
        }

        next->event = WIX_BTN_LCM_SCROLL;
        down(&wdev->lock);
        list_add_tail(&next->list, &wdev->elist);
        wake_up_interruptible(&wdev->wq);
        up(&wdev->lock);
    }

    outl((0x01 << (WIXSCI_OFFSET_PWR + 16)), (acpi_base + ACPI_OFFSET_GPE0_STS));
    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (0x01 << (WIXSCI_OFFSET_LCM_SCR + 16))), (acpi_base + ACPI_OFFSET_GPE0_EN));
    WIXPRINT("Exit [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));
    up(&whandler->lock);

    return;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
static irqreturn_t wixsci_isr(int irq, void *dev_id)
#else
static irqreturn_t wixsci_isr(int irq, void *dev_id, struct pt_regs *reg)
#endif
{
    irqreturn_t     res = IRQ_NONE;
    int             loop;
    unsigned int    acpi_gpe_sts;
    int             sci_offset[] = {WIXSCI_OFFSET_BTN, WIXSCI_OFFSET_LCM_SEL, WIXSCI_OFFSET_LCM_SCR, WIXSCI_OFFSET_PWR, -1};

    spin_lock(&slock);

    WIXPRINT("Enter [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));
    WIXPRINT("[PM1_STS 0x%X][PM1_EN 0x%X][PM1_CNT 0x%X]\n", inw(acpi_base + ACPI_OFFSET_PM1_STS), inw(acpi_base + ACPI_OFFSET_PM1_EN), inl(acpi_base + ACPI_OFFSET_PM1_CNT));
    WIXPRINT("[LVL 0x%X]\n", inl(gpio_base + GPIO_OFFSET_LVL));

    acpi_gpe_sts = inl(acpi_base + ACPI_OFFSET_GPE0_STS);

    for (loop = 0; sci_offset[loop] != -1; loop++) {
        if ((acpi_gpe_sts >> (sci_offset[loop] + 16)) & 0x01) {
            res = IRQ_HANDLED;
            switch (sci_offset[loop])
            {
                case 0: //WIXSCI_OFFSET_BTN:
                    {
                        outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_BTN + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                        schedule_work(&wixHndBtn.workqueue);
                    }
                    break;
                case 2: //px4:WIXSCI_OFFSET_LCM_SEL rackmount:WIXSCI_OFFSET_LCM_SCR
                    {
                    #if BTN_WORKAROUND
                        if( board_id == DESKTOP){
                            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_LCM_SEL + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                            schedule_work(&wixHndSel.workqueue);
                        } else {
                            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_LCM_SCR + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                            schedule_work(&wixHndScr.workqueue);
                        }
                    #else
                        outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_LCM_SEL + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                        schedule_work(&wixHndSel.workqueue);
                    #endif
                    }
                    break;
                case 3: //px4:WIXSCI_OFFSET_LCM_SCR rackmount:WIXSCI_OFFSET_LCM_SEL
                    {
                    #if BTN_WORKAROUND
                        if( board_id == DESKTOP){
                            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_LCM_SCR + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                            schedule_work(&wixHndScr.workqueue);
                        } else {
                            outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_LCM_SEL + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                            schedule_work(&wixHndSel.workqueue);
                        }
                    #else
                        outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_LCM_SCR + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                        schedule_work(&wixHndScr.workqueue);
                    #endif
                    }
                    break;
                case 10:    //WIXSCI_OFFSET_PWR:
                    {
                        outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (WIXSCI_OFFSET_PWR + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
                        schedule_work(&wixHndPwr.workqueue);
                    }
                    break;
                default:
                    break;
            }
        }
    }

    WIXPRINT("Exit [GPES 0x%X][GPEE 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS), inl(acpi_base + ACPI_OFFSET_GPE0_EN));

    spin_unlock(&slock);

    return res;
}

int addhweevent (int eventid) {
    wixSCIDev       *wdev;
    wixEventList    *next;

    wdev = (wixSCIDev *) &wixDevHwe;

    if ((next = kmalloc(sizeof(wixEventList), GFP_ATOMIC)) == NULL) {
        WIXVPRINT("Unable to allocate memory !\n");
        return -ENOMEM;
    }

    down(&wdev->lock);

    next->event = eventid;
    list_add_tail(&next->list, &wdev->elist);
    wake_up_interruptible(&wdev->wq);

    up(&wdev->lock);

    return 0;
}

int wixsci_init(void) {
    struct pci_dev  *pcidev;
    int             rc;
    acpi_status     astatus;
    int             loop, loop2;
    wixSCIHandler   *shnd[] = {&wixHndBtn, &wixHndSel, &wixHndScr, &wixHndPwr, NULL};
    //wixSCIDev       *sdev[] = {&wixDevBtn, &wixDevHwe, NULL};
    wixSCIDev       *sdev[] = {&wixDevBtn, NULL};
    unsigned int    grouting;

    #if BTN_WORKAROUND
    WIXVPRINT("BTN workaround.\n");
    if( board_id == DESKTOP ){
        WIXSCI_OFFSET_BTN=0;
        WIXSCI_OFFSET_LCM_SEL=2;
        WIXSCI_OFFSET_LCM_SCR=3;
        WIXSCI_OFFSET_PWR=10;

    }else{  // Rackmount
        WIXSCI_OFFSET_BTN=0;
        WIXSCI_OFFSET_LCM_SEL=3;
        WIXSCI_OFFSET_LCM_SCR=2;
        WIXSCI_OFFSET_PWR=10;
    }
    wixHndBtn = (wixSCIHandler) {
        .type   = WIXSCI_OFFSET_BTN,
        .index  = WIXSCI_OFFSET_BTN,
        .tasklet= wix_btn_tasklet,
    };

    wixHndPwr = (wixSCIHandler) {
        .type   = WIXSCI_OFFSET_PWR,
        .index  = WIXSCI_OFFSET_PWR,
        .tasklet= wix_powerbtn_tasklet,
    };
    wixHndSel = (wixSCIHandler) {
        .type   = WIXSCI_OFFSET_LCM_SEL,
        .index  = WIXSCI_OFFSET_LCM_SEL,
        .tasklet= wix_selbtn_tasklet,
    };
    wixHndScr = (wixSCIHandler) {
        .type   = WIXSCI_OFFSET_LCM_SCR,
        .index  = WIXSCI_OFFSET_LCM_SCR,
        .tasklet= wix_scrbtn_tasklet,
    };
    acpi_idx_table[0] = WIXSCI_OFFSET_BTN;
    acpi_idx_table[1] = WIXSCI_OFFSET_LCM_SEL;
    acpi_idx_table[2] = WIXSCI_OFFSET_LCM_SCR;
    acpi_idx_table[3] = WIXSCI_OFFSET_PWR;
    acpi_idx_table[4] = -1;
    #endif

    if ((pcidev = pci_get_device(LPC_VENDORID, LPC_DEVICEID, NULL)) == NULL) {
        WIXVPRINT("No LPC device!!\n");
        return -1;
    }

    if ((rc = pci_read_config_dword(pcidev, LPC_OFFSET_PMBASE, (unsigned int *) &acpi_base)) < 0) {
        WIXVPRINT("pci_read_config_dword\n");
        return -1;
    }

    if ((acpi_base & 0x01) == 0) {
        WIXVPRINT("ACPI Can't support I/O space.\n");
        return -1;
    }

    acpi_base &= 0x0000FF80;

    if ((rc = pci_read_config_dword(pcidev, LPC_OFFSET_GPIOBASE, (unsigned int *) &gpio_base)) < 0) {
        WIXVPRINT("pci_read_config_dword\n");
        return -1;
    }

    if ((gpio_base & 0x01) == 0) {
        WIXVPRINT("GPIO Can't support I/O space.\n");
        return -1;
    }

    gpio_base &= 0x0000FF80;

    //Register ACPI Handler
    for (loop = 0; acpi_idx_table[loop] != -1; loop++)
    {
        astatus = acpi_install_gpe_handler(NULL, (0x10 + acpi_idx_table[loop]), ACPI_GPE_LEVEL_TRIGGERED, &wix_acpi_gpe_handler, &acpi_idx_table[loop]);
        if (astatus != AE_OK) {
            for (loop2 = 0; loop2 < loop; loop2++)
                acpi_remove_gpe_handler(NULL, (0x10 + acpi_idx_table[loop2]), &wix_acpi_gpe_handler);
            WIXVPRINT("Unable to claim ACPI GPE [0x%X]\n", acpi_idx_table[loop]);
            return -EINVAL;
        }
    }

    //Initial PCH LPC
    outl((inl(gpio_base + GPIO_OFFSET_USE_SEL) | ((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR))), (gpio_base + GPIO_OFFSET_USE_SEL));
    outl((inl(gpio_base + GPIO_OFFSET_IO_SEL) | ((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR))), (gpio_base + GPIO_OFFSET_IO_SEL));
    outl((inl(gpio_base + GPIO_OFFSET_INV) | ((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR))), (gpio_base + GPIO_OFFSET_INV));
    outl((inl(gpio_base + GPIO_OFFSET_LVL) & (~((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR)))), (gpio_base + GPIO_OFFSET_LVL));
    outl((((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR)) << 16), (acpi_base + ACPI_OFFSET_GPE0_STS));
//    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR)) << 16)), (acpi_base + ACPI_OFFSET_GPE0_EN));
    outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) | (((1 << WIXSCI_OFFSET_BTN) | (1 << WIXSCI_OFFSET_LCM_SEL) | (1 << WIXSCI_OFFSET_LCM_SCR) | (1 << WIXSCI_OFFSET_PWR)) << 16)), (acpi_base + ACPI_OFFSET_GPE0_EN));
    outw((inw(acpi_base + ACPI_OFFSET_PM1_EN) & 0xFEFF), (acpi_base + ACPI_OFFSET_PM1_EN));
    outl((inl(acpi_base + ACPI_OFFSET_PM1_CNT) | 0x01), (acpi_base + ACPI_OFFSET_PM1_CNT));

    WIXPRINT("[GPIO 0x%X][ACPI 0x%X]\n", gpio_base, acpi_base);
    WIXPRINT("[SEL 0x%X]\n", inl(gpio_base + GPIO_OFFSET_USE_SEL));
    WIXPRINT("[IO 0x%X]\n", inl(gpio_base + GPIO_OFFSET_IO_SEL));
    WIXPRINT("[LVL 0x%X]\n", inl(gpio_base + GPIO_OFFSET_LVL));
    WIXPRINT("[INV 0x%X]\n", inl(gpio_base + GPIO_OFFSET_INV));
    WIXPRINT("[GPESTS 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_STS));
    WIXPRINT("[GPEEN 0x%X]\n", inl(acpi_base + ACPI_OFFSET_GPE0_EN));
    WIXPRINT("[PM1_STS 0x%X][PM1_EN 0x%X][PM1_CNT 0x%X]\n", inw(acpi_base + ACPI_OFFSET_PM1_STS), inw(acpi_base + ACPI_OFFSET_PM1_EN), inl(acpi_base + ACPI_OFFSET_PM1_CNT));

    //Initial wixSCIHandler & wixSCIDev
    killed = 0;
    spin_lock_init(&slock);

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    sema_init(&glock, 1);
    #else
    init_MUTEX(&glock);
    #endif

    for (loop=0; shnd[loop]!=NULL; loop++) {
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
        sema_init(&shnd[loop]->lock, 1);
        #else
        init_MUTEX(&shnd[loop]->lock);
        #endif

        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
        INIT_WORK(&shnd[loop]->workqueue, (void *) shnd[loop]->tasklet);
        #else
        INIT_WORK(&shnd[loop]->workqueue, (void (*)(void *)) shnd[loop]->tasklet, 0);
        #endif
    }
    for (loop=0; sdev[loop]!=NULL; loop++) {
        #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
        sema_init(&sdev[loop]->lock, 1);
        #else
        init_MUTEX(&sdev[loop]->lock);
        #endif
        switch (sdev[loop]->type) {
            case WIXSCI_DEV_BTN:
                {
                    sdev[loop]->wclass = class_create(THIS_MODULE, "wixbtn");
                    if (IS_ERR(sdev[loop]->wclass)) {
                        rc = PTR_ERR(sdev[loop]->wclass);
                        return rc;
                    }
                    rc = alloc_chrdev_region(&sdev[loop]->wdevt, 0, 128, "wixbtn");
                    if (rc < 0) {
                        class_destroy(sdev[loop]->wclass);
                        return rc;
                    }
                    cdev_init(&sdev[loop]->wcdev, &sdev[loop]->fops);
                    sdev[loop]->wcdev.owner = THIS_MODULE;
                    rc = cdev_add(&sdev[loop]->wcdev, sdev[loop]->wdevt, 128);
                    if (rc < 0) {
                        unregister_chrdev_region(sdev[loop]->wdevt, 128);
                        class_destroy(sdev[loop]->wclass);
                        return rc;
                    }
                    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                    sdev[loop]->wdev = device_create(sdev[loop]->wclass, NULL, MKDEV(MAJOR(sdev[loop]->wdevt), 0), NULL, BTN_NAME);
                    #else
                    sdev[loop]->wdev = device_create(sdev[loop]->wclass, NULL, MKDEV(MAJOR(sdev[loop]->wdevt), 0), BTN_NAME);
                    #endif
                    if (IS_ERR(sdev[loop]->wdev)) {
                        cdev_del(&sdev[loop]->wcdev);
                        unregister_chrdev_region(sdev[loop]->wdevt, 128);
                        class_destroy(sdev[loop]->wclass);
                        return rc;
                    }
                }
                break;
            default:
                break;
        }
    }

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,29)
    if (request_irq(9, wixsci_isr, IRQF_SHARED, "wixsci", wixsci_isr))
    #elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
    if (request_irq(9, wixsci_isr, IRQF_DISABLED | IRQF_SHARED, "wixsci", wixsci_isr))
    #else
    if (request_irq(9, wixsci_isr, SA_INTERRUPT | SA_SHIRQ, "wixsci", wixsci_isr))
    #endif
    {
        WIXVPRINT("Can't request IRQ(%d)\n", 9);
        for (loop = 0; acpi_idx_table[loop] != -1; loop++)
            acpi_remove_gpe_handler(NULL, (0x10 + acpi_idx_table[loop]), &wix_acpi_gpe_handler);
        for (loop = 0; sdev[loop] != NULL; loop++) {
            device_destroy(sdev[loop]->wclass, sdev[loop]->wdevt);
            cdev_del(&sdev[loop]->wcdev);
            unregister_chrdev_region(sdev[loop]->wdevt, 128);
            class_destroy(sdev[loop]->wclass);
        }
        return -1;
    }

    //Initial PCH LPC
    if ((rc = pci_read_config_dword(pcidev, 0xB8, (unsigned int *) &grouting)) < 0) {
        WIXVPRINT("pci_read_config_dword\n");
        return -1;
    }

    grouting &= (~((3 << (WIXSCI_OFFSET_BTN * 2)) | (3 << (WIXSCI_OFFSET_LCM_SEL * 2)) | (3 << (WIXSCI_OFFSET_LCM_SCR * 2)) | (3 << (WIXSCI_OFFSET_PWR * 2))));        //0xFC3FFFFF
    grouting |= ((2 << (WIXSCI_OFFSET_BTN * 2)) | (2 << (WIXSCI_OFFSET_LCM_SEL * 2)) | (2 << (WIXSCI_OFFSET_LCM_SCR * 2)) | (2 << (WIXSCI_OFFSET_PWR * 2)));        //0x02800000

    if ((rc = pci_write_config_dword(pcidev, 0xB8, grouting)) < 0) {
        WIXVPRINT("pci_write_config_dword\n");
        return -1;
    }

    return 0;
}

void wixsci_exit(void)
{
    struct pci_dev  *pcidev;
    wixSCIHandler   *shnd[] = {&wixHndBtn, &wixHndSel, &wixHndScr, &wixHndPwr, NULL};

    wixSCIDev       *sdev[] = {&wixDevBtn, &wixDevHwe, NULL};
    wixEventList    *next;
    int             loop;
    int             rc;

    free_irq(9, wixsci_isr);
    killed = 1;

    if ((pcidev = pci_get_device(LPC_VENDORID, LPC_DEVICEID, NULL)) == NULL) {
        WIXVPRINT("No LPC device!!\n");
        return;
    }

    if ((rc = pci_write_config_dword(pcidev, 0xB8, 0x00)) < 0) {
        WIXVPRINT("pci_write_config_dword\n");
    }

    for (loop = 0; shnd[loop]!=NULL; loop++) {
        outl((inl(acpi_base + ACPI_OFFSET_GPE0_EN) & (~(0x01 << (shnd[loop]->index + 16)))), (acpi_base + ACPI_OFFSET_GPE0_EN));
    }

    for (loop = 0; acpi_idx_table[loop] != -1; loop++)
        acpi_remove_gpe_handler(NULL, (0x10 + acpi_idx_table[loop]), &wix_acpi_gpe_handler);

    for (loop = 0; sdev[loop] != NULL; loop++) {
        wake_up_interruptible(&sdev[loop]->wq);
        down(&sdev[loop]->lock);
        device_destroy(sdev[loop]->wclass, sdev[loop]->wdevt);
        cdev_del(&sdev[loop]->wcdev);
        unregister_chrdev_region(sdev[loop]->wdevt, 128);
        class_destroy(sdev[loop]->wclass);
        while (!list_empty(&sdev[loop]->elist)) {
            next = list_entry(sdev[loop]->elist.next, wixEventList, list);
            list_del(&next->list);
            kfree(next);
        }
        up(&sdev[loop]->lock);
    }

    return;
}
