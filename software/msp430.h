#ifndef _MSP430_H_
#define	_MSP430_H_

#define	MSP430_RAM_START	0x0200
#define	MSP430_RAM_END		0x09FF

#define	MSP430_ROM_START	0x0C00
#define	MSP430_ROM_END		0x0FFF

#define	MSP430_FLASH_START	0x1000
#define	MSP430_FLASH_END	0xFFFF

extern char *BLname;
extern int BSL_TEST;
extern int fast;
extern char *pwd_name;

#define	ARGS_MSP430_PARAMS \
  {ARGS_STRING, "--bsl", "<BSL filename>", &BLname, 0, "set BSL upgrade filename; default %s"},\
  {ARGS_OPTINTEGER, "--slow", NULL, &fast, 0, NULL},\
  {ARGS_OPTINTEGER, "--TEST", NULL, &BSL_TEST, 1, NULL},\
  {ARGS_STRING, "--pwd", "<HEX filename>", &pwd_name, 0, "set MCU password filename"},

//  {ARGS_CHOOSE, "--verify", "off/on", &verify, 2, "enable/disable verify; default %s"},

extern int msp430_load (COM_HANDLE hCom, file_data_type *data, const char *arg0);

#endif

