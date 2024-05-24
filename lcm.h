#ifndef _WIXLCM_H_
#define _WIXLCM_H_

#include <linux/version.h>
#include <linux/kernel.h>

int  wixlcm_init(void);
void wixlcm_exit(void);
int  initialize_lcm(void);
void nctGppValueSet(unsigned char grp_id, unsigned char gp_offset, unsigned char value);
void set_databus_mode(int mode);
int set_databus_data(int data);
int set_ctrlpin_data(int ctrl_pin, unsigned char data);
void write_lcm(int A0type,unsigned char value);
int lcmdev_open(struct inode *inode, struct file *file);
int lcmdev_release(struct inode *inode, struct file *file);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long lcmdev_ioctl(struct file *filp,unsigned int cmd,unsigned long arg);
#else
int lcmdev_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
#endif



#endif //_WIXLCM_H_
