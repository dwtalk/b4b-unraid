#ifndef GPIO_H
#define GPIO_H

#define GPIO_CMD_OFF            0
#define GPIO_CMD_ON             1
#define GPIO_CMD_BLINK          2
#define GPIO_CMD_NOBLINK        3
#define GPIO_CMD_INVERT_ON      4
#define GPIO_CMD_INVERT_OFF     5

#define GPIO_STS_INVERT_OFF     0
#define GPIO_STS_INVERT_ON      1

#define GPIO_DIRECTION_INPUT    0
#define GPIO_DIRECTION_OUTPUT   1

struct gpio_host {
    struct list_head    __gpios;
    struct semaphore    lock;
    int (* probe)       (struct gpio_host *);
    int (* release)     (struct gpio_host *);
    unsigned int        baseaddr;
    unsigned int        basedata;
    unsigned int        type;
};

struct gpio_device {
    struct list_head    list;
    struct gpio_host    *host;
    int (* read)        (struct gpio_device *);
    int (* write)       (struct gpio_device *, int);
    int gpio_index;
    int gpio_value;
    int gpio_direction;
};

#endif
