#ifndef NAS_H
#define NAS_H

#define BTN_NAME                "wixbtn"
#define WIX_NO_BTN              0x0000
#define WIX_BTN_POWER           0x0001
#define WIX_BTN_RESET_BRIEFLY   0x0002
#define WIX_BTN_RESET_LONG      0x0003
#define WIX_BTN_LCM_SELECT      0x0004
#define WIX_BTN_LCM_SCROLL      0x0005

#define READ_BUTTON_NONBLOCKING _IOWR('c', 150,int)
#define READ_BUTTON_BLOCKING    _IOWR('c', 151,int)

#define READ_HWE_NONBLOCKING    _IOWR('c', 152,int)
#define READ_HWE_BLOCKING       _IOWR('c', 153,int)

#define LCM_IOC_MAGIC           0xDD
#define WIX_LCM_CMD_PON         _IO(LCM_IOC_MAGIC,  7)
#define WIX_LCM_CMD_POFF        _IO(LCM_IOC_MAGIC,  8)
#define WIX_LCM_CMD_RESET       _IO(LCM_IOC_MAGIC,  9)

#define IOCTL_DISPLAY_COMMAND   _IO(LCM_IOC_MAGIC,  1)
#define IOCTL_SCHEDULE_POWERON  _IO(LCM_IOC_MAGIC,  2)

typedef struct lcm_info{
    int ctrl;       // LCM control command
    int page;       // page 0~7
    int column;     // column 0~127
    int size;       // the actual size of display data
    char data[128]; // display data, maximum 128 byte for one page
}lcm_structure;

#endif
