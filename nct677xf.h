#ifndef    NCT677XF_H
#define    NCT677XF_H

#include "hwm.h"

#define HWMHOST_TYPE_NCT677XF    0x2000

#define HWM_TYPE_FAN    0x2010
#define HWM_TYPE_VOLT   0x2020
#define HWM_TYPE_THERM  0x2030
#define HWM_TYPE_OTHER  0x2050

#define HWM_VCORE       0x0480
#define HWM_VIN0        0x0484
#define HWM_AVCC        0x0482
#define HWM_3VCC        0x0483
#define HWM_VIN1        0x0481
#define HWM_VIN2        0x048C
#define HWM_VIN3        0x048D
#define HWM_3VSB        0x0487
#define HWM_VBAT        0x0488

#define HWM_FAN1        0x0028
#define HWM_FAN2        0x0029
#define HWM_FAN3        0x002A
#define HWM_FAN4        0x003F
#define HWM_BANK        0x004E
#define HWM_VID         0x004F
#define HWM_FAN1H       0x0658
#define HWM_FAN1L       0x0659
#define HWM_FAN2H       0x065A
#define HWM_FAN2L       0x065B
#define HWM_FAN3H       0x0656
#define HWM_FAN3L       0x0657
#define HWM_FAN4H       0x065C
#define HWM_FAN4L       0x065D
#define HWM_SMIOVT1     0x0027
#define HWM_SMIOVT2     0x0150
#define HWM_SMIOVT3     0x0250
#define HWM_SMIOVT4     0x0628
#define HWM_SMIOVT5     0x062C
#define HWM_SMIOVT6     0x062D

#define HWM_BYTETEMP_H  0x0419
#define HWM_BYTETEMP_L  0x041A


#define HWM_CTRL_REG    0x005D
#define HWM_DIODE_MODE  0x005E


int  nct677xf_init(void);
void nct677xf_exit(void);
int  ncthwmh_probe(struct hwm_host *hwmhost, struct hwm_device hwmdev[]);
int  ncthwmh_release(struct hwm_host *hwmhost);
int  ncthwmd_read(struct hwm_device *phwmdev);
int  ncthwmd_write(struct hwm_device *phwmdev, int value);
int ncthwmd_fan_read (struct hwm_device *phwmdev);
int ncthwmd_volt_read (struct hwm_device *phwmdev);
int ncthwmd_therm_read (struct hwm_device *phwmdev);
int ncthwmd_other_read (struct hwm_device *phwmdev);

#endif
