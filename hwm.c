/*
        hwm.c -  HWM module

        Copyright (c) 2011 - 2013

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

int hwm_probe (struct hwm_host *hwmhost, struct hwm_device hwmdev[]) {
	int	loop;
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
		phwdev->read = hwmd_read;
		phwdev->write = hwmd_write;
		phwdev->hwm_value = 0;
	}

	return 0;
}

int hwm_release (struct hwm_host *hwmhost) {
	return 0;
}

int hwmd_read (struct hwm_device *phwmdev) {
	int	res = 0;
	down(&phwmdev->host->lock);
	switch (phwmdev->host->type) {
		case	HWMHOST_TYPE_NCT677XF:
			{
				if ((res = ncthwmd_read(phwmdev)) != 0)
					phwmdev->hwm_value = -1;
			}
			break;
		default:
			phwmdev->hwm_value = -1;
			break;
	}
	up(&phwmdev->host->lock);
	return res;
}

int hwmd_write (struct hwm_device *phwmdev, int value) {
	int res;
	down(&phwmdev->host->lock);
	switch (phwmdev->host->type) {
		case	HWMHOST_TYPE_NCT677XF:
			{
				if ((res = ncthwmd_read(phwmdev)) != 0)
					phwmdev->hwm_value = -1;
			}
			break;
		default:
			phwmdev->hwm_value = -1;
			break;
	}
	up(&phwmdev->host->lock);
	return 0;
}

int hwm_init(void) {
	int res = 0;
	WIXPRINT("Initializing HWM Chip...\n");
	wdt_init();
	return res;
}

void hwm_exit(void) {
	WIXPRINT("Removing HWM Chip...\n");
	wdt_exit();
	return;
}
