/*
 * The following license idents are currently accepted as indicating free
 * software modules
 *
 *	"GPL v2"			[GNU Public License v2]
 *
 * There are dual licensed components, but when running with Linux it is the
 * GPL that is relevant so this is a non issue. Similarly LGPL linked with GPL
 * is a GPL combined work.
 *
 * This exists for several reasons
 * 1.	So modinfo can show license info for users wanting to vet their setup
 *	is free
 * 2.	So the community can ignore bug reports including proprietary modules
 * 3.	So vendors can do likewise based on their own policies
 */

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/rtc.h>
#include <linux/sched.h>
#include "nas.h"
#include "b4b.h"
#include "lcm.h"

/* pin definition */
#define LCM_BUS_A0          0x0102  //GP12 for A0
#define LCM_BUS_CS          0x0100  //GP10 for CS
#define LCM_BUS_E           0x0104  //GP14 for E
#define LCM_BUS_RS          0x0101  //GP11 for RS
#define LCM_BUS_RW          0x0103  //GP13 for R/W
#define D0                  0x0105  //GP15 for D0
#define D1                  0x0106  //GP16 for D1
#define D2                  0x0107  //GP17 for D2
#define D3                  0x0501  //GP51 for D3
#define D4                  0x0503  //GP53 for D4
#define D5                  0x0504  //GP54 for D5
#define D6                  0x0506  //GP56 for D6
#define D7                  0x0507  //GP57 for D7

#define LCM_DATA_BITS       8
#define PIN_LOW             0x00
#define PIN_HIGH            0x01

/* LCM CMD */
#define A0_DAT              0x01
#define A0_CMD              0x00
#define PIN_IN              0x01
#define PIN_OUT             0x00
#define RS_WRITE            0x00
#define RS_READ             0x01

#define LCM_OFF             0xAE
#define LCM_ON              0xAF

/* LCM DATA Bus mode */
#define LCM_DATABUS_OUTPUT  0x00
#define LCM_DATABUS_INPUT   0x01
#define LCM_DATABUS_UNKNOW  0xFF

/* define Marco */
#define GPP_GROUP(PIN_DEF)  (PIN_DEF >> 0x8)
#define GPP_BIT(PIN_DEF)    (PIN_DEF & 0x00FF)

#define WIX_LCM_VERSION     "0.1.4"
#define LCM_DEVICE_NAME     "lcm"

#define BIT_REVERSE(VAL)    ( ((VAL& 0x80)>>7) | ((VAL&0x40)>>5) | ((VAL&0x20)>>3) | ((VAL&0x10)>>1) | ((VAL&0x8)<<1) | ((VAL&0x4)<<3) | ((VAL&0x2)<<5) | ((VAL&0x1)<<7)  )

//hardcode for now
//int board_id = DESKTOP;
extern int board_id;

extern struct semaphore global_lock;

// Data pin mapping
static int data_pin_mapping[8] = {D0, D1, D2, D3, D4, D5, D6, D7};

struct lcm_device
{
    struct semaphore  lock;
    int data_bus_mode;//D0-D7 set to in or out
    unsigned int baseaddr;
    unsigned int basedata;
};

static struct lcm_device    lcm_device;
static char is_opened =0;

#define INIT_ST7565R_SIZE 13
#define INIT_UC1604C_SIZE 11

unsigned char  init_st7565r_pkg[INIT_ST7565R_SIZE]= {
    0xae,    //1
    0xa2,    //2
    0xa0,    //3
    0xc8,    //4
    0xa6,    //5
    0x40,    //6    //orig:0x41
    0x23,    //7    //orig:0x22
    0x81,    //8
    0x37,    //9    //orig:0x3f
    0xf8,    //10
    0x00,    //11
    0xa4,    //12
    0x2c,    //13
};

unsigned char  init_uc1604c_pkg[INIT_UC1604C_SIZE]= {
    0xe2,    //1
    0xf1,    //2
    0x3f,    //3
    0xf2,    //4
    0x10,    //5
    0xf3,    //6
    0x2f,    //7
    0x85,    //8
    0xa0,    //9
    0xe8,    //10
    0x2f,    //11
};


char nctGppValueGet(unsigned char grp_id, unsigned char gp_offset);
void read_databus_status(void);


void nctGppValueSet(unsigned char grp_id, unsigned char gp_offset, unsigned char value) {

    struct lcm_device *lcm = &lcm_device;
    unsigned char logic_dev_num=0, org_val, data, data_reg;

    down(&global_lock);
    down(&lcm->lock);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x07, SIO_ADDR);
    switch(grp_id) {
        case 0x00:
            logic_dev_num = 0x08;
            data_reg = 0xE1;
            break;
        case 0x01:
            logic_dev_num = 0x08;
            data_reg = 0xF1;
            break;
        case 0x02:
            logic_dev_num = 0x09;
            data_reg = 0xE1;
            break;
        case 0x04:
            logic_dev_num = 0x09;
            data_reg = 0xF1;
            break;
        case 0x05:
            logic_dev_num = 0x09;
            data_reg = 0xF5;
            break;
        case 0x08:
            logic_dev_num = 0x07;
            data_reg = 0xE5;
            break;
        default:
            outb_p(0xAA, SIO_ADDR);
            up(&lcm->lock);
            return;
            break;
    }
    outb_p(logic_dev_num, SIO_ADDR + 1);
    outb_p(data_reg, SIO_ADDR);
    org_val = inb(SIO_ADDR + 1);

    data = 1 << gp_offset;
    if( value == 1){
        data = org_val | data;
    } else {
        data = org_val & (~data&0xFF);
    }
    outb_p(data_reg, SIO_ADDR);
    outb_p(data, SIO_ADDR + 1);
    outb_p(0xAA, SIO_ADDR);
    up(&lcm->lock);
    up(&global_lock);
    WIXPRINT("GID=%d, bit=%d, value=%x, wr_data=0x%02X\n",grp_id, gp_offset, value, data);
    //nctGppValueGet(grp_id, gp_offset);
    WIXPRINT("-----------------------");
}

char nctGppValueGet(unsigned char grp_id, unsigned char gp_offset) {

    struct lcm_device *lcm = &lcm_device;
    unsigned char logic_dev_num=0, org_val, data_reg;
    unsigned char data;

    down(&global_lock);
    down(&lcm->lock);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x07, SIO_ADDR);
    switch(grp_id) {
        case 0x00:
            logic_dev_num = 0x08;
            data_reg = 0xE1;
            break;
        case 0x05:
            logic_dev_num = 0x09;
            data_reg = 0xF5;
            break;
       default:
            outb_p(0xAA, SIO_ADDR);
            up(&lcm->lock);
            up(&global_lock);
            return -1;
            break;
    }
    outb_p(logic_dev_num, SIO_ADDR + 1);
    outb_p(data_reg, SIO_ADDR);
    org_val = inb(SIO_ADDR + 1);

    outb_p(0xAA, SIO_ADDR);
    up(&lcm->lock);
    up(&global_lock);
    data = (org_val >> gp_offset) & 0x1;
    WIXPRINT("GID=%d, bit=%d, value=%x, wr_data=0x%02X\n",grp_id, gp_offset, data, org_val);

    return data;
}


void set_databus_mode(int mode) {
    struct lcm_device *lcm = &lcm_device;
    int val;

    if ( lcm->data_bus_mode != mode) {
        down(&global_lock);
        down(&lcm->lock);
        outb_p(0x87, SIO_ADDR);
        outb_p(0x87, SIO_ADDR);
        switch(mode){
            case LCM_DATABUS_OUTPUT:
                /* set D0,D1,D2(GP15,GP16,GP17) as output */
                outb_p(0x07, SIO_ADDR);
                outb_p(0x08, SIO_ADDR + 1);
                outb_p(0xF0, SIO_ADDR);
                val = inb(SIO_ADDR + 1);
                outb_p(0xF0, SIO_ADDR);
                outb_p((val & 0xF8), SIO_ADDR +1);
               /* set D3,D4,D5,D6,D7(GP51,GP53,GP54,GP56,GP57) as output */
                outb_p(0x07, SIO_ADDR);
                outb_p(0x09, SIO_ADDR + 1);
                outb_p(0xF4, SIO_ADDR);
                val = inb(SIO_ADDR + 1);
                outb_p(0xF4, SIO_ADDR);
                outb_p((val & 0xA4), SIO_ADDR +1);
                break;
            case LCM_DATABUS_INPUT:
                /* set D0,D1,D2(GP15,GP16,GP17) as input */
                outb_p(0x07, SIO_ADDR);
                outb_p(0x08, SIO_ADDR + 1);
                outb_p(0xF0, SIO_ADDR);
                val = inb(SIO_ADDR + 1);
                outb_p(0xF0, SIO_ADDR);
                outb_p((val | 0x07), SIO_ADDR +1);
                /* set D3,D4,D5,D6,D7(GP51,GP53,GP54,GP56,GP57) as input */
                outb_p(0x07, SIO_ADDR);
                outb_p(0x09, SIO_ADDR + 1);
                outb_p(0xF4, SIO_ADDR);
                val = inb(SIO_ADDR + 1);
                outb_p(0xF4, SIO_ADDR);
                outb_p((val | 0x5B), SIO_ADDR +1);
                break;
            default:
                break;
        }
        outb_p(0xAA, SIO_ADDR);
        up(&lcm->lock);
        up(&global_lock);
        lcm->data_bus_mode = mode;
    }
}

void read_databus_status(void) {

    struct lcm_device *lcm = &lcm_device;
    int val;
    down(&global_lock);
    down(&lcm->lock);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x87, SIO_ADDR);
    /* read D0,D1,D2(GP15,GP16,GP17) I/O directory  */
    outb_p(0x07, SIO_ADDR);
    outb_p(0x08, SIO_ADDR + 1);
    outb_p(0xF0, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    WIXPRINT("GPIO1 -> I/O value = 0x%02X\n", val);
   /* read D3,D4,D5,D6,D7(GP87,GP86,GP84,GP81,GP80) I/O directory */
    outb_p(0x07, SIO_ADDR);
    outb_p(0x09, SIO_ADDR + 1);
    outb_p(0xF4, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    WIXPRINT("GPIO5 -> I/O value = 0x%02X\n", val);
    outb_p(0xAA, SIO_ADDR);
    up(&lcm->lock);
    up(&global_lock);
    WIXPRINT("LCM DATA I/O mode=%d\n", lcm->data_bus_mode);

}


int set_databus_data(int data){
    int i;
    unsigned char bit_data;
    WIXPRINT("write data=0x%02X to LCM\n", data);

    for( i=0; i<LCM_DATA_BITS; i++) {
        bit_data = data & 0x01;
        switch(data_pin_mapping[i] ) {
            case D0:
            case D1:
            case D2:
            case D3:
            case D4:
            case D5:
            case D6:
            case D7:
                nctGppValueSet(GPP_GROUP(data_pin_mapping[i]), GPP_BIT(data_pin_mapping[i]), bit_data);
                break;
            default:
                break;
        }
        data = data >> 1;
    }
    return 0;
}

int set_ctrlpin_data(int ctrl_pin, unsigned char data){

    switch(ctrl_pin) {
        case LCM_BUS_A0:
        case LCM_BUS_CS:
        case LCM_BUS_E:
        case LCM_BUS_RS:
        case LCM_BUS_RW:
            nctGppValueSet( GPP_GROUP(ctrl_pin), GPP_BIT(ctrl_pin), data);
            break;
        default:
            break;
    }
    return 0;
}


void write_lcm(int A0type,unsigned char value) {

    set_databus_mode(LCM_DATABUS_OUTPUT);

    //set E pin down
    set_ctrlpin_data( LCM_BUS_E, PIN_LOW);
    //prepare data
    set_databus_data(value);
    //prepare A0
    switch(A0type) {
        case A0_CMD:
            set_ctrlpin_data(LCM_BUS_A0, PIN_LOW);
            break;
        case A0_DAT:
        default:
            set_ctrlpin_data(LCM_BUS_A0, PIN_HIGH);
            break;
    }

    //prepare R/W
    set_ctrlpin_data(LCM_BUS_RW, RS_WRITE);
    //set CS pin down
    set_ctrlpin_data(LCM_BUS_CS, PIN_LOW);
    //issue E
    set_ctrlpin_data(LCM_BUS_E, PIN_HIGH);

    //delay for it completed
    udelay(100);

    //de-issue E
    set_ctrlpin_data(LCM_BUS_E, PIN_LOW);
    //set CS high
    set_ctrlpin_data(LCM_BUS_CS, PIN_HIGH);

    //delay for it completed
    udelay(100);

    return;
}

int initialize_lcm(void) {
    int    i, init_size;
    unsigned char *init_p;

    if(board_id == DESKTOP){
        WIXVPRINT("Initial destop LCM module.\n");
        init_size = INIT_ST7565R_SIZE;
        init_p=init_st7565r_pkg;
    } else {
        WIXVPRINT("Initial rackmount LCM module.\n");
        init_size = INIT_UC1604C_SIZE;
        init_p=init_uc1604c_pkg;
    }

    for (i = 0; i<init_size ; i++){
        write_lcm(A0_CMD, init_p[i]);
    }
    mdelay(200);//delay 200ms
    write_lcm(A0_CMD,0x2e);
    mdelay(200);//delay 200ms
    write_lcm(A0_CMD,0x2f);
    mdelay(400);//delay 400ms
    write_lcm(A0_CMD,0xaf);
    return 0;
}


int lcmdev_open(struct inode *inode, struct file *file) {

    if(is_opened) {
        WIXVPRINT("LCM has been used by another program\n");
        return -EFAULT;
    }

    is_opened = 1;
    return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long lcmdev_ioctl(struct file *filp,unsigned int cmd,unsigned long arg) {
#else
int lcmdev_ioctl( struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg){
#endif

    lcm_structure Info;
    unsigned char tmp;
    switch(cmd) {
        case IOCTL_DISPLAY_COMMAND:
            if(is_opened != 1) {
                WIXPRINT("LCM device is_opend \n");
                return -1;
            }
            if ( copy_from_user(&Info, (lcm_structure*)arg,sizeof(lcm_structure)) ) {
                WIXPRINT("lcm_ioctl could not copy user space data\n");
                return -1;
            }

            WIXPRINT("ctrl=%d,page=%d,col=%d,size=%d\n",Info.ctrl,Info.page,Info.column,Info.size);
            switch ( Info.ctrl ) {
                case WIX_LCM_CMD_PON:
                    write_lcm(A0_CMD,0xAF);
                    break;
                case WIX_LCM_CMD_POFF:
                    write_lcm(A0_CMD,0xAE);
                    break;
                case WIX_LCM_CMD_RESET:
                    if(board_id == DESKTOP){
                        if(Info.page<0 || Info.page>7 || Info.column <0 || Info.column>127 || Info.size<0 || Info.size>128 || sizeof(Info.data)>128){
                            WIXPRINT("wix_lcm_ioctl param error\n");
                            return -EFAULT;
                        }
                        if(Info.column + Info.size > 128) {
                            WIXPRINT("wix_lcm_ioctl column add size over 128 \n");
                            return -EFAULT;
                        }
                    } else {
                        if(Info.page<0 || Info.page>4 || Info.column <0 || Info.column>127 || Info.size<0 || Info.size>128 || sizeof(Info.data)>128){
                            WIXPRINT("wix_lcm_ioctl param error\n");
                            return -EFAULT;
                        }
                        if(Info.column + Info.size > 128) {
                            WIXPRINT("wix_lcm_ioctl column add size over 128 \n");
                            return -EFAULT;
                        }
                        //Info.page ^= 0x3;   //reverse PAGE number
                        Info.page += 2;     //shift PAGE
                    }
                    //set page number
                    tmp = Info.page;
                    write_lcm(A0_CMD, tmp|0xb0);
                    //set most signification column number
                    tmp = Info.column;
                    write_lcm(A0_CMD, ( (tmp>>4)&0x0f)|0x10 );
                    //set least signification column number
                    write_lcm(A0_CMD,  (tmp&0x0f) );

                    for(tmp=0;tmp<Info.size;tmp++) {
                        write_lcm(A0_DAT, Info.data[tmp] );
                    }
                    break;
                default:
                    break;
            }
            break;
        default:
            WIXPRINT("Unknow IOCTL----,cmd=%d, arg=%ld", cmd, arg);
            return -1;
    }

    return 0;
}

int lcmdev_release(struct inode *inode, struct file *file) {

    // initial LCM variable.
    is_opened =0;
    WIXPRINT("Release LCM device.");
    return 0;
}

static struct file_operations lcmdev_fops = {
    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
    .unlocked_ioctl = lcmdev_ioctl,
    #else
    .ioctl = lcmdev_ioctl,
    #endif
    .open = lcmdev_open,
    .release = lcmdev_release,
};

static struct miscdevice lcmdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = LCM_DEVICE_NAME,
    .fops = &lcmdev_fops,
};

int wixlcm_init(void) {
    int val;
    struct lcm_device *lcm = &lcm_device;

    //Init LCM device
    memset(lcm, 0, sizeof(struct lcm_device));

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    sema_init(&lcm->lock, 0);
    #else
    init_MUTEX_LOCKED(&lcm->lock);
    #endif
    up(&lcm->lock);
    /* Register lcm device */
    if (misc_register(&lcmdev)) {
        WIXVPRINT("lcmdev: registration of /dev/lcm failed\n");
        return -1;
    }
    down(&global_lock);
    down(&lcm->lock);
    //=======================================================================
    /* Enter Configuration Mode */
    outb_p(0x87, SIO_ADDR);
    outb_p(0x87, SIO_ADDR);
    outb_p(0x20, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    if (val != 0xC5) {
        WIXVPRINT("Can't find SIO device.val=0x%x\n", val);
        outb_p(0xAA, SIO_ADDR);
        return -1;
    }


    //set CS, A0, E, RW, RS pin to output mode
    outb_p(0x07, SIO_ADDR);
    outb_p(0x08, SIO_ADDR + 1);
    outb_p(0xF0, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    outb_p(0xF0, SIO_ADDR);
    outb_p((val & 0x7), SIO_ADDR +1);
    outb_p(0xF0, SIO_ADDR);
    val = inb(SIO_ADDR + 1);
    outb_p(0xAA, SIO_ADDR);
    up(&lcm->lock);
    up(&global_lock);
    //set E pin to LOW
    set_ctrlpin_data(LCM_BUS_E, PIN_LOW);

    lcm->data_bus_mode = -1; //unknow mode
    initialize_lcm();

    WIXVPRINT("WIXLCM driver initialization.(Version %s)", WIX_LCM_VERSION);
    return 0;
}

void wixlcm_exit(void) {

    misc_deregister(&lcmdev);
    WIXVPRINT("WIXLCM driver unloaded.");
}

//module_init(wixlcm_init);
//module_exit(wixlcm_exit);

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");
