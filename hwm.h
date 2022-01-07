#ifndef	HWM_H
#define	HWM_H

struct hwm_device;
struct hwm_host {
	struct list_head        __hwms;
	struct semaphore	lock;
	int (* probe)		(struct hwm_host *, struct hwm_device *);
	int (* release)		(struct hwm_host *);
	unsigned int		baseaddr;
	unsigned int		basedata;
	unsigned int		type;
};

struct hwm_device {
	struct list_head 	list;
	struct hwm_host 	*host;
	int (* read)		(struct hwm_device *);
	int (* write)		(struct hwm_device *, int value);
	int			hwm_type;
	int			hwm_index;
	int			hwm_value;
};

int hwm_probe 	(struct hwm_host *hwmhost, struct hwm_device hwmdev[]);
int hwm_release	(struct hwm_host *hwmhost);
int hwmd_read 	(struct hwm_device *phwmdev);
int hwmd_write	(struct hwm_device *phwmdev, int value);
int hwm_init	(void);
void hwm_exit	(void);

struct hwm_device *searchHWM(int index);

#endif
