/*
 * Data for SMBus Messages
 */
#ifndef SMBUS_H
#define SMBUS_H

#define SMBUS_BLOCK_MAX     32      /* As specified in SMBus standard */
#define SMBUS_DATA_PEC      (SMBUS_BLOCK_MAX + 1)

union smbus_data {
    u8 byte;
    u16 word;
    u8 block[SMBUS_BLOCK_MAX + 3];  /* block[0] is used for length */
};

int smbus_init(void);
void smbus_exit(void);
int smbus_quick (u16 addr);
int smbus_read_byte (u16 addr, u8 *value);
int smbus_write_byte (u16 addr, u8 command);
int smbus_read_byte_data (u16 addr, u8 command, u8 *value);
int smbus_write_byte_data (u16 addr, u8 command, u8 value);
int smbus_read_word_data (u16 addr, u8 command, u16 *value);
int smbus_write_word_data (u16 addr, u8 command, u16 value);
int smbus_read_byte_pec (u16 addr, u8 *value, u8 *pec);
int smbus_write_byte_pec (u16 addr, u8 command, u8 pec);
int smbus_read_byte_data_pec (u16 addr, u8 command, u8 *value, u8 *pec);
int smbus_write_byte_data_pec (u16 addr, u8 command, u8 value, u8 pec);
int smbus_read_word_data_pec (u16 addr, u8 command, u16 *value, u8 *pec);
int smbus_write_word_data_pec (u16 addr, u8 command, u16 value, u8 pec);
int smbus_read_block_data(u16 addr, u8 command, u8 *length, u8 *values);
int smbus_write_block_data(u16 addr, u8 command, u8 length, u8 *values);
int i2c_read_block_data(u16 addr, u8 command, u8 *length, u8 *values);
int i2c_write_block_data(u16 addr, u8 command, u8 length, u8 *values);
void smbus_lock(void);
void smbus_unlock(void);

#endif
