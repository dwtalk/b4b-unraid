#ifndef GPIOSIO_H
#define GPIOSIO_H

#define BANK_SEL        0x004E

#define AUXFANOUT0_RD    0x0011
#define AUXFANOUT1_RD    0x0013
#define AUXFANOUT2_RD    0x0015
#define AUXFANOUT0_WR    0x0309
#define AUXFANOUT1_WR    0x0809
#define AUXFANOUT2_WR    0x0909


int  siogpio_init(void);
void siogpio_exit(void);
struct gpio_device *siogphost_search(int index);

#endif
