/*
        smbus.c - SMBus control module for hardware monitoring

        Copyright (c) 2004 - 2005  Aldofo Lin <aldofo_lin@wistron.com.tw>

        This program is free software; you can redistribute it and/or modify
        it under the terms of the GNU General Public License as published by
        the Free Software Foundation; either version 2 of the License, or
        (at your option) any later version.

        This program is distributed in the hope that it will be useful,
        but WITHOUT ANY WARRANTY; without even the implied warranty of
        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        GNU General Public License for more details.

        You should have received a copy of the GNU General Public License
        along with this program; if not, write to the Free Software
        Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/reboot.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "b4b.h"
#include "smbus.h"

MODULE_LICENSE("GPL");

#define DRV_NAME            "smbus"
#define SMB_DRV_VERSION     "1.00"

#define SMBUS_VENDOR    0x8086
#define SMBUS_DEVICE    0x3a30  //Intel SM BUS DID
#define MAX_TIMEOUT     500

/* PCI Address Constants */
#define SMBUS_PCI_REV           0x008
#define SMBUS_PCI_BASEADDR      0x020
#define SMBUS_PCI_HSTCFG        0x040

/* Host configuration bits for SMBHSTCFG */
#define SMBUS_PCI_HSTCFG_HST_EN         0x01
#define SMBUS_PCI_HSTCFG_SMB_SMI_EN     0x02
#define SMBUS_PCI_HSTCFG_I2C_EN         0x04

#define SMBUS_HST_STS         0x00
#define SMBUS_HST_CNT         0x02
#define SMBUS_HST_CMD         0x03
#define SMBUS_XMIT_SLVA       0x04
#define SMBUS_HST_D0          0x05
#define SMBUS_HST_D1          0x06
#define SMBUS_HOST_BLOCK_DB   0x07
#define SMBUS_PEC             0x08
#define SMBUS_RCV_SLVA        0x09
#define SMBUS_SLV_DATA        0x0A
#define SMBUS_AUX_STS         0x0C
#define SMBUS_AUX_CTL         0x0D
#define SMBUS_SMLINK_PIN_CTL  0x0E
#define SMBUS_SMBUS_PIN_CTL   0x0F
#define SMBUS_SLV_STS         0x10
#define SMBUS_SLV_CMD         0x11
#define SMBUS_NOTIFY_DADDR    0x14
#define SMBUS_NOTIFY_DLOW     0x16
#define SMBUS_NOTIFY_DHIGH    0x17

#define SMBUS_HST_STS_DS            0x80
#define SMBUS_HST_STS_INUSE_STS     0x40
#define SMBUS_HST_STS_SMBALERT_STS  0x20
#define SMBUS_HST_STS_FAILED        0x10
#define SMBUS_HST_STS_BUS_ERR       0x08
#define SMBUS_HST_STS_DEV_ERR       0x04
#define SMBUS_HST_STS_INTR          0x02
#define SMBUS_HST_STS_HOST_BUSY     0x01

#define SMBUS_HST_CNT_PEC_EN                0x80
#define SMBUS_HST_CNT_START                 0x40
#define SMBUS_HST_CNT_LAST_BYTE             0x20
#define SMBUS_HST_CNT_SMB_CMD_Quick         0x00
#define SMBUS_HST_CNT_SMB_CMD_Byte          0x04
#define SMBUS_HST_CNT_SMB_CMD_ByteData      0x08
#define SMBUS_HST_CNT_SMB_CMD_WordData      0x0C
#define SMBUS_HST_CNT_SMB_CMD_PEC_Byte      0x84
#define SMBUS_HST_CNT_SMB_CMD_PEC_ByteData  0x88
#define SMBUS_HST_CNT_SMB_CMD_PEC_WordData  0x8C
#define SMBUS_HST_CNT_SMB_CMD_ProcessCall   0x10
#define SMBUS_HST_CNT_SMB_CMD_Block         0x14
#define SMBUS_HST_CNT_I2C_CMD_Block         0x15
#define SMBUS_HST_CNT_SMB_CMD_I2CRead       0x18
#define SMBUS_HST_CNT_SMB_CMD_BlockProcess  0x1C
#define SMBUS_HST_CNT_KILL                  0x02
#define SMBUS_HST_CNT_INTERN                0x01

#define SMBUS_AUX_CTL_E32B      0x02

#define SMBUS_XMIT_SLVA_WRITE   0x00
#define SMBUS_XMIT_SLVA_READ    0x01

struct pci_dev *smbdev;
struct semaphore smb_lock, smbus_semaphore;
u32 smbus_base;

/* Return -1 on error. */
static int smbus_access(u16 addr, u8 read_write, u8 command, u8 type, union smbus_data *value);
static int smbus_transaction(unsigned int xact);
static int smbus_block_transaction(u8 read_write, u8 type, union smbus_data *value);

/* Tiny delay function used by the SMbus drivers */
static inline void smbus_delay(long timeout)
{
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(timeout);
}

static inline void smbus_wait(void)
{
    int timeout = 0, tmp;

    if (((tmp = inb_p(smbus_base + SMBUS_HST_STS)) & (SMBUS_HST_STS_FAILED | SMBUS_HST_STS_BUS_ERR | SMBUS_HST_STS_DEV_ERR | SMBUS_HST_STS_INTR | SMBUS_HST_STS_HOST_BUSY)) == 0x00)
        return;

    do {
        smbus_delay(1);
        tmp = inb_p(smbus_base + SMBUS_HST_STS);
    } while ((tmp & SMBUS_HST_STS_HOST_BUSY) && (timeout++ < MAX_TIMEOUT));

    outb_p(inb(smbus_base + SMBUS_HST_STS), (smbus_base + SMBUS_HST_STS));

    return;
}

extern void smbus_lock(void)
{
    down(&smb_lock);
}

extern void smbus_unlock(void)
{
    up(&smb_lock);
}

extern int smbus_read_byte_pec (u16 addr, u8 *value, u8 *pec)
{
    int res;
    union smbus_data data;

    if ((value == NULL) || (pec == NULL))
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, 0x00, SMBUS_HST_CNT_SMB_CMD_PEC_Byte, &data);
    up(&smbus_semaphore);

    *value = (data.byte & 0xFF);
    *pec = (data.block[SMBUS_DATA_PEC] & 0xFF);
    return res;
}

extern int smbus_write_byte_pec (u16 addr, u8 command, u8 pec)
{
    int res;
    union smbus_data data;

    data.block[SMBUS_DATA_PEC] = pec;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_PEC_Byte, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_read_byte_data_pec (u16 addr, u8 command, u8 *value, u8 *pec)
{
    int res;
    union smbus_data data;

    if ((value == NULL) || (pec == NULL))
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, command, SMBUS_HST_CNT_SMB_CMD_PEC_ByteData, &data);
    up(&smbus_semaphore);

    *value = (data.byte & 0xFF);
    *pec = (data.block[SMBUS_DATA_PEC] & 0xFF);
    return res;
}

extern int smbus_write_byte_data_pec (u16 addr, u8 command, u8 value, u8 pec)
{
    int res;
    union smbus_data data;

    data.byte = value;
    data.block[SMBUS_DATA_PEC] = pec;
    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_PEC_ByteData, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_read_word_data_pec (u16 addr, u8 command, u16 *value, u8 *pec)
{
    int res;
    union smbus_data data;

    if ((value == NULL) || (pec == NULL))
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, command, SMBUS_HST_CNT_SMB_CMD_PEC_WordData, &data);
    up(&smbus_semaphore);

    *value = (data.word & 0xFFFF);
    *pec = (data.block[SMBUS_DATA_PEC] & 0xFF);
    return res;
}

extern int smbus_write_word_data_pec (u16 addr, u8 command, u16 value, u8 pec)
{
    int res;
    union smbus_data data;

    data.word = value;
    data.block[SMBUS_DATA_PEC] = pec;
    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_PEC_WordData, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_quick (u16 addr)
{
    int res;
    union smbus_data data;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, 0x00, SMBUS_HST_CNT_SMB_CMD_Quick, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_read_byte (u16 addr, u8 *value)
{
    int res;
    union smbus_data data;

    if (value == NULL)
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, 0x00, SMBUS_HST_CNT_SMB_CMD_Byte, &data);
    up(&smbus_semaphore);

    *value = (data.byte & 0xFF);
    return res;
}

extern int smbus_write_byte (u16 addr, u8 command)
{
    int res;
    union smbus_data data;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_Byte, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_read_byte_data (u16 addr, u8 command, u8 *value)
{
    int res;
    union smbus_data data;

    if (value == NULL)
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, command, SMBUS_HST_CNT_SMB_CMD_ByteData, &data);
    up(&smbus_semaphore);

    *value = (data.byte & 0xFF);
    return res;
}

extern int smbus_write_byte_data (u16 addr, u8 command, u8 value)
{
    int res;
    union smbus_data data;

    data.byte = value;
    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_ByteData, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_read_word_data (u16 addr, u8 command, u16 *value)
{
    int res;
    union smbus_data data;

    if (value == NULL)
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, command, SMBUS_HST_CNT_SMB_CMD_WordData, &data);
    up(&smbus_semaphore);

    *value = (data.word & 0xFFFF);
    return res;
}

extern int smbus_write_word_data (u16 addr, u8 command, u16 value)
{
    int res;
    union smbus_data data;

    data.word = value;
    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_WordData, &data);
    up(&smbus_semaphore);

    return res;
}

extern int smbus_read_block_data(u16 addr, u8 command, u8 *length, u8 *values)
{
    int res;
    union smbus_data data;
    int loop;

    if ((values == NULL) || (length == NULL))
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, command, SMBUS_HST_CNT_SMB_CMD_Block, &data);
    up(&smbus_semaphore);

    for (loop = 0; loop <= data.block[0]; loop++)
        values[loop] = (u8) data.block[loop];

    if (res >= 0)
        *length = data.block[0];
    else
        *length = 0;

    return res;
}

extern int smbus_write_block_data(u16 addr, u8 command, u8 length, u8 *values)
{
    int res;
    union smbus_data data;
    int loop;

    if (length > SMBUS_BLOCK_MAX)
        length = SMBUS_BLOCK_MAX;
    for (loop = 1; loop <= length; loop++)
        data.block[loop] = (u8) values[loop - 1];

    data.block[0] = length;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_SMB_CMD_Block, &data);
    up(&smbus_semaphore);

    return res;
}

extern int i2c_read_block_data(u16 addr, u8 command, u8 *length, u8 *values)
{
    int res;
    union smbus_data data;
    int loop;

    if ((values == NULL) || (length == NULL))
        return -1;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_READ, command, SMBUS_HST_CNT_I2C_CMD_Block, &data);
    up(&smbus_semaphore);

    for (loop = 0; loop <= data.block[0]; loop++)
        values[loop] = (u8) data.block[loop];

    if (res >= 0)
        *length = data.block[0];
    else
        *length = 0;

    return res;
}

extern int i2c_write_block_data(u16 addr, u8 command, u8 length, u8 *values)
{
    int res;
    union smbus_data data;
    int loop;

    if (length > SMBUS_BLOCK_MAX)
        length = SMBUS_BLOCK_MAX;
    for (loop = 1; loop <= length; loop++)
        data.block[loop] = (u8) values[loop - 1];

    data.block[0] = length;

    down(&smbus_semaphore);
    res = smbus_access(addr, SMBUS_XMIT_SLVA_WRITE, command, SMBUS_HST_CNT_I2C_CMD_Block, &data);
    up(&smbus_semaphore);

    return res;
}

static int smbus_transaction(unsigned int xact)
{

    int tmp, result = 0, timeout = 0;

    /* Make sure the SMBus host is ready to start transmitting */
    /* 0x1f = Failed, Bus_Err, Dev_Err, Intr, Host_Busy */
    smbus_wait();

    if ((tmp = ((SMBUS_HST_STS_FAILED | SMBUS_HST_STS_BUS_ERR | SMBUS_HST_STS_DEV_ERR | SMBUS_HST_STS_INTR | SMBUS_HST_STS_HOST_BUSY) & inb_p(smbus_base + SMBUS_HST_STS))) != 0x00)
    {
        WIXVPRINT("SMBus busy (%02X). Resetting... \n", tmp);
        outb_p(tmp, (smbus_base + SMBUS_HST_STS));

        if ((tmp = ((SMBUS_HST_STS_FAILED | SMBUS_HST_STS_BUS_ERR | SMBUS_HST_STS_DEV_ERR | SMBUS_HST_STS_INTR | SMBUS_HST_STS_HOST_BUSY) & inb_p(smbus_base + SMBUS_HST_STS))) != 0x00)
        {
            WIXVPRINT("SMBus Failed! (%02x)\n", tmp);
            return -1;
        } else
        {
            WIXPRINT("Successfull!\n");
        }
    }

    outb_p((xact | SMBUS_HST_CNT_START), (smbus_base + SMBUS_HST_CNT));

    /* We will always wait for a fraction of a second! */
    timeout = 0;
    do {
        smbus_delay(1);
        tmp = inb_p(smbus_base + SMBUS_HST_STS);
    } while ((tmp & SMBUS_HST_STS_HOST_BUSY) && (timeout++ < MAX_TIMEOUT));

    /* If the SMBus is still busy, we give up */
    if (timeout >= MAX_TIMEOUT)
    {
        WIXVPRINT("SMBus Timeout!\n");
        outb_p(inb_p(smbus_base + SMBUS_HST_CNT) | SMBUS_HST_CNT_KILL, (smbus_base + SMBUS_HST_CNT));
        smbus_delay(1);
        outb_p(inb_p(smbus_base + SMBUS_HST_CNT) & (~SMBUS_HST_CNT_KILL), (smbus_base + SMBUS_HST_CNT));
                result = -1;
    }

    if ((tmp & (SMBUS_HST_STS_FAILED | SMBUS_HST_STS_BUS_ERR | SMBUS_HST_STS_DEV_ERR)) != 0x00)
    {
        if (tmp & SMBUS_HST_STS_FAILED)
            WIXVPRINT("Error: [Addr 0x%X] Failed bus transaction\n", inb_p((smbus_base + SMBUS_XMIT_SLVA)));
        if (tmp & SMBUS_HST_STS_BUS_ERR)
            WIXVPRINT("Bus collision! SMBus may be locked until next hard reset. (sorry!)\n");
        if (tmp & SMBUS_HST_STS_DEV_ERR)
            WIXVPRINT("Error: no response [0x%X][Addr 0x%X]!\n", xact, inb_p((smbus_base + SMBUS_XMIT_SLVA)));

        result = -1;
    }

    if ((inb_p(smbus_base + SMBUS_HST_STS) & 0x1f) != 0x00)
        outb_p(inb(smbus_base + SMBUS_HST_STS), (smbus_base + SMBUS_HST_STS));

    if ((tmp = (0x1f & inb_p(smbus_base + SMBUS_HST_STS))) != 0x00)
        WIXVPRINT("Failed reset at end of transaction (%02x)\n", tmp);

    if ((inb_p(smbus_base + SMBUS_HST_STS) & (SMBUS_HST_STS_FAILED | SMBUS_HST_STS_BUS_ERR | SMBUS_HST_STS_DEV_ERR | SMBUS_HST_STS_INTR | SMBUS_HST_STS_HOST_BUSY)) != 0x00)
    {
        smbus_wait();
    }

    return result;
}

static int smbus_block_transaction(u8 read_write, u8 type, union smbus_data *value)
{
    int result = 0;
    int len, loop;
    unsigned char hostc;

    if ((type == SMBUS_HST_CNT_I2C_CMD_Block) && (read_write == SMBUS_XMIT_SLVA_WRITE))
    {
        pci_read_config_byte(smbdev, SMBUS_PCI_HSTCFG, &hostc);
        pci_write_config_byte(smbdev, SMBUS_PCI_HSTCFG, hostc | SMBUS_PCI_HSTCFG_I2C_EN);
    }

    if (read_write == SMBUS_XMIT_SLVA_WRITE)
    {
        if (value->block[0] < 1)
            value->block[0] = 1;
        else if (value->block[0] > SMBUS_BLOCK_MAX)
            value->block[0] = SMBUS_BLOCK_MAX;
    }
    else
    {
        value->block[0] = 32;
    }

    if ((type == SMBUS_HST_CNT_I2C_CMD_Block) && (read_write == SMBUS_XMIT_SLVA_READ))
    {
        /* No supported */
    }
    else
    {
        outb_p(inb_p(smbus_base + SMBUS_AUX_CTL) | SMBUS_AUX_CTL_E32B, (smbus_base + SMBUS_AUX_CTL));
        inb_p(smbus_base + SMBUS_HST_CNT);

        /* Use 32-byte buffer to process this transaction */
        if (read_write == SMBUS_XMIT_SLVA_WRITE)
        {
            len = value->block[0];
            outb_p(len, (smbus_base + SMBUS_HST_D0));
            for (loop = 1; loop <= len; loop++)
            {
                outb_p(value->block[loop], (smbus_base + SMBUS_HOST_BLOCK_DB));
            }
        }

        if ((result = smbus_transaction(SMBUS_HST_CNT_SMB_CMD_Block)) == 0)
        {
            if (read_write == SMBUS_XMIT_SLVA_READ)
            {
                len = inb_p(smbus_base + SMBUS_HST_D0);
                if ((len >= 1) && (len <= SMBUS_BLOCK_MAX))
                {
                    value->block[0] = len;
                    for (loop = 1; loop <= len; loop++)
                        value->block[loop] = inb_p(smbus_base + SMBUS_HOST_BLOCK_DB);
                }
                else
                    result = -1;
            }
        }
    }

    if ((type == SMBUS_HST_CNT_I2C_CMD_Block) && (read_write == SMBUS_XMIT_SLVA_WRITE))
    {
        /* restore saved configuration register value */
        pci_write_config_byte(smbdev, SMBUS_PCI_HSTCFG, hostc);
    }

    return result;
}

static int smbus_access(u16 addr, u8 read_write, u8 command, u8 type, union smbus_data *value)
{
    int block = 0, ret;

    switch (type)
    {
        case SMBUS_HST_CNT_SMB_CMD_Quick:
            outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), (smbus_base + SMBUS_XMIT_SLVA));
            break;
        case SMBUS_HST_CNT_SMB_CMD_Byte:
        case SMBUS_HST_CNT_SMB_CMD_PEC_Byte:
            outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), (smbus_base + SMBUS_XMIT_SLVA));
            if (read_write == SMBUS_XMIT_SLVA_WRITE)
                outb_p(command, (smbus_base + SMBUS_HST_CMD));
            break;
        case SMBUS_HST_CNT_SMB_CMD_ByteData:
        case SMBUS_HST_CNT_SMB_CMD_PEC_ByteData:
            outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), (smbus_base + SMBUS_XMIT_SLVA));
            outb_p(command, (smbus_base + SMBUS_HST_CMD));
            if (read_write == SMBUS_XMIT_SLVA_WRITE)
                outb_p(value->byte, (smbus_base + SMBUS_HST_D0));
            break;
        case SMBUS_HST_CNT_SMB_CMD_WordData:
        case SMBUS_HST_CNT_SMB_CMD_PEC_WordData:
            outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), (smbus_base + SMBUS_XMIT_SLVA));
            outb_p(command, (smbus_base + SMBUS_HST_CMD));
            if (read_write == SMBUS_XMIT_SLVA_WRITE)
            {
                outb_p((value->word & 0xFF), (smbus_base + SMBUS_HST_D0));
                outb_p(((value->word & 0xFF00) >> 8), (smbus_base + SMBUS_HST_D1));
            }
            break;
        case SMBUS_HST_CNT_SMB_CMD_Block:
            outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), (smbus_base + SMBUS_XMIT_SLVA));
            outb_p(command, (smbus_base + SMBUS_HST_CMD));
            block = 1;
            break;
        case SMBUS_HST_CNT_I2C_CMD_Block:
            outb_p(((addr & 0x7f) << 1) | (read_write & 0x01), (smbus_base + SMBUS_XMIT_SLVA));
            if (read_write == SMBUS_XMIT_SLVA_READ)
                outb_p(command, (smbus_base + SMBUS_HST_D1));
            else
                outb_p(command, (smbus_base + SMBUS_HST_CMD));
            block = 1;
            break;
        case SMBUS_HST_CNT_SMB_CMD_BlockProcess:
        case SMBUS_HST_CNT_SMB_CMD_ProcessCall:
        case SMBUS_HST_CNT_SMB_CMD_I2CRead:
        default:
            WIXVPRINT("Unsupported transaction %X\n", type);
            return -1;
    }

    if(block)
    {
        ret = smbus_block_transaction(read_write, type, value);
    } else
    {
        switch (type)
        {
            case SMBUS_HST_CNT_SMB_CMD_PEC_Byte:
            case SMBUS_HST_CNT_SMB_CMD_PEC_ByteData:
            case SMBUS_HST_CNT_SMB_CMD_PEC_WordData:
                {
                    if (read_write == SMBUS_XMIT_SLVA_WRITE)
                        outb_p((value->block[SMBUS_DATA_PEC] & 0xFF), (smbus_base + SMBUS_PEC));
                    outb_p(type & ~(SMBUS_HST_CNT_INTERN), (smbus_base + SMBUS_HST_CNT));
                    ret = smbus_transaction(type & ~(SMBUS_HST_CNT_INTERN));
                }
                break;
            default:
                {
                    outb_p(type & ~(SMBUS_HST_CNT_PEC_EN | SMBUS_HST_CNT_INTERN), (smbus_base + SMBUS_HST_CNT));
                    ret = smbus_transaction(type & ~(SMBUS_HST_CNT_PEC_EN | SMBUS_HST_CNT_INTERN));
                }
                break;
        }
    }

    if(block)
        return ret;

    if(ret)
        return -1;

    if ((read_write == SMBUS_XMIT_SLVA_WRITE) || (type == SMBUS_HST_CNT_SMB_CMD_Quick))
        return 0;

    switch (type & 0xFF)
    {
        case SMBUS_HST_CNT_SMB_CMD_Byte: /* Result put in SMBHSTDAT0 */
        case SMBUS_HST_CNT_SMB_CMD_ByteData:
            value->byte = inb_p((smbus_base + SMBUS_HST_D0));
            break;
        case SMBUS_HST_CNT_SMB_CMD_WordData:
            value->word = inb_p((smbus_base + SMBUS_HST_D0)) + (inb_p((smbus_base + SMBUS_HST_D1)) << 8);
            break;
        case SMBUS_HST_CNT_SMB_CMD_PEC_Byte:
        case SMBUS_HST_CNT_SMB_CMD_PEC_ByteData:
            value->byte = inb_p(smbus_base + SMBUS_HST_D0);
            value->block[SMBUS_DATA_PEC] = (u8) inb_p(smbus_base + SMBUS_PEC);
            break;
        case SMBUS_HST_CNT_SMB_CMD_PEC_WordData:
            value->word = inb_p(smbus_base + SMBUS_HST_D0) + (inb_p((smbus_base + SMBUS_HST_D1)) << 8);
            value->block[SMBUS_DATA_PEC] = (u8) inb_p(smbus_base + SMBUS_PEC);
            break;
        }
        return 0;
}

int smbus_init(void) {
    u8 val;

    #if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
    sema_init(&smb_lock, 0);
    sema_init(&smbus_semaphore, 0);
    #else
    init_MUTEX_LOCKED(&smb_lock);
    init_MUTEX_LOCKED(&smbus_semaphore);
    #endif

    up(&smb_lock);
    up(&smbus_semaphore);

    smbdev = pci_get_device(SMBUS_VENDOR, SMBUS_DEVICE, NULL);
    if (smbdev == NULL)
    {
        WIXVPRINT("Can't find SMBus device.\n");
        return -1;
    }
    if (pci_read_config_dword(smbdev, SMBUS_PCI_BASEADDR, (u32 *) &smbus_base) < 0)
    {
        WIXVPRINT("pci_read_config_dword\n");
        return -1;
    }
    if ((smbus_base & 0x00000001) == 0x00)
    {
        WIXVPRINT("Can't support I/O space.\n");
        return -1;
    }

    smbus_base &= 0x0000FFE0;
    WIXPRINT("SMBus Base Address - %X\n", smbus_base);

    val = SMBUS_PCI_HSTCFG_HST_EN;
    val &= ~(SMBUS_PCI_HSTCFG_SMB_SMI_EN | SMBUS_PCI_HSTCFG_I2C_EN);
    pci_write_config_byte(smbdev, SMBUS_PCI_HSTCFG, val);

    #ifdef  WIXDEBUG
    pci_read_config_byte(smbdev, SMBUS_PCI_HSTCFG, (u8 *) &val);
    WIXVPRINT("SMBus Host Configuration - 0x%02X\n", val);
    #endif

    return 0;
}

void smbus_exit(void) {
    return;
}

/*
 * Get rid of taint message by declaring code as GPL.
 */
MODULE_LICENSE("GPL");
