#ifndef WDT_H
#define WDT_H

#define HWMHOST_TYPE_WDT    0x5000
#define HWM_TYPE_WDT        0x5010

#define HWM_WDT_REG_WDTConf     0xF5
#define HWM_WDT_REG_WDTCounter  0xF6
#define HWM_WDT_REG_WDTSTS      0xF7

int  wdt_init(void);
void wdt_exit(void);
int  wdt_probe(struct hwm_host *hwmhost, struct hwm_device hwmdev[]);
int  wdt_release(struct hwm_host *hwmhost);
int  wdt_read(struct hwm_device *phwmdev);
int  wdt_write(struct hwm_device *phwmdev, int value);

#endif
