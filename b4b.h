#ifdef  WIXDEBUG
#define WIXPRINT(fmt, args...) printk(KERN_ERR "(%s) : " fmt, __FUNCTION__, ## args)
#else
#define WIXPRINT(fmt, args...)
#endif
#define WIXVPRINT(fmt, args...) printk(KERN_ERR "(%s) " fmt, __FUNCTION__, ## args)

#define SIO_ADDR		0x4E

#define DESKTOP         0
#define RACKMOUNT       1
