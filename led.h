#ifndef __LED_H__
#define __LED_H__

#define LEDHOST_TYPE_SYSTEM     0x4100
#define LEDHOST_TYPE_HDD_STS    0x4200
#define LEDHOST_TYPE_HDD_FAULT  0x4300

#define LEDDEV_TYPE_PCH         0x4010
#define LEDDEV_TYPE_SIO         0x4060

#define LEDDEV_STS_OFF          0
#define LEDDEV_STS_ON           1
#define LEDDEV_STS_BLINKING1    2
#define LEDDEV_STS_BLINKING2    3

struct led_device;
struct led_host {
    struct list_head    __leds;
    struct semaphore    lock;
    int (* probe)       (struct led_host *, struct led_device *);
    int (* release)     (struct led_host *);
    unsigned int        baseaddr;
    unsigned int        basedata;
    unsigned int        type;
};

struct led_device {
    struct list_head    list;
    struct led_host     *host;
    int (* read)        (struct led_device *);
    int (* write)       (struct led_device *, int value);
    int type;
    int index;
    int value;
};

int led_probe   (struct led_host *ledhost, struct led_device leddev[]);
int led_release (struct led_host *ledhost);
int ledd_read   (struct led_device *pleddev);
int ledd_write  (struct led_device *pleddev, int value);
int led_init    (void);
void led_exit   (void);

#endif  // __LED_H__ endif
