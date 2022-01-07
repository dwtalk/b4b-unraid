#ifndef __B4B_H__
#define __B4B_H__

#include "b4b.h"
#include "hwm.h"

struct b4b_hwm_class_attribute {
    struct class_attribute  class_attr;
    struct hwm_device       *hwmd;
};
#define to_b4b_hwm_class_attr(_class_attr) container_of(_class_attr, struct b4b_hwm_class_attribute, class_attr)

static ssize_t sys_b4b_hwm_show(struct class *class, struct class_attribute *attr, char *buf);
static ssize_t sys_b4b_hwm_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count);

#define B4B_HWM_ATTR(_name, _mode, _show, _store, _hwmd) { .class_attr = __ATTR(_name, _mode, _show, _store), .hwmd = _hwmd}

#define DEFINE_B4B_HWM_ATTR(_name, _mode, _hwmd) static struct b4b_hwm_class_attribute sys_b4b_hwm_##_name##_attr = B4B_HWM_ATTR(_name, _mode, sys_b4b_hwm_show, sys_b4b_hwm_store, _hwmd);

struct b4b_led_class_attribute {
    struct class_attribute  class_attr;
    struct led_device       *ledd;
};
#define to_b4b_led_class_attr(_class_attr) container_of(_class_attr, struct b4b_led_class_attribute, class_attr)

static ssize_t sys_b4b_led_show(struct class *class, struct class_attribute *attr, char *buf);
static ssize_t sys_b4b_led_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count);

#define B4B_LED_ATTR(_name, _mode, _show, _store, _ledd) { .class_attr = __ATTR(_name, _mode, _show, _store), .ledd = _ledd}

#define DEFINE_B4B_LED_ATTR(_name, _mode, _ledd) static struct b4b_led_class_attribute sys_b4b_led_##_name##_attr = B4B_LED_ATTR(_name, _mode, sys_b4b_led_show, sys_b4b_led_store, _ledd);

#endif
