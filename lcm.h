#ifndef _WIXLCM_H_
#define _WIXLCM_H_

#include <linux/version.h>
#include <linux/kernel.h>

int  wixlcm_init(void);
void wixlcm_exit(void);
int  initialize_lcm(void);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long lcmdev_ioctl(struct file *filp,unsigned int cmd,unsigned long arg);
#else
int lcmdev_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif



#endif //_WIXLCM_H_
