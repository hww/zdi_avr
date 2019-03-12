#ifndef _STM32_H_
#define _STM32_H_

#define	STM32_FLASH_START	0x08000000
#define	STM32_FLASH_END		0x0807FFFF

#define	STM32_EEPROM_START	0x08080000
#define	STM32_EEPROM_END	0x0808FFFF	// ???

#define	STM32_CCMRAM_START	0x10000000	// higher devices (STM32F4); memory for core & data only!
#define	STM32_CCMRAM_END	0x100FFFFF	// 64K @ STM43F4

#define	STM32_SYSTEM_START	0x1FF00000
//#define	STM32_ROM_START		0x1FF00000	// ROM on STM32L0
//#define	STM32_ROM_START		0x1FFF0000	// ROM on STM32F4
//#define	STM32_OPTION_START	0x1FF80000	// option @ STM32L0
//#define	STM32_OPTION_START	0x1FF0C000	// option @ STM32F4
#define	STM32_SYSTEM_END	0x1FFFFFFF

#define	STM32_SRAM_START	0x20000000
#define	STM32_SRAM_END		0x21FFFFFF	// bitband region follows

#define	STM32_ROM_START		0x1FFF0000	// address differs based on MCU type (offset 0xB000 or 0xF000)
#define	STM32_ROM_END		0x1FFFFFFF

#define	STM32_PACKET_LEN	0x80

extern unsigned stm32_protection;

#define	ARGS_STM32_PARAMS \
  {ARGS_OPTINTEGER, "-EO", 0, &erase, 2, ""}, \
  {ARGS_OPTINTEGER, "--erase-optimized", 0, &erase, 2, "optimized erase procedure (erased used sectors only)"}, \
  {ARGS_CHOOSE, "-PRT", "none/-r/+r/-r+r/-w/-r-w/+r-w/-r+r-w/+w/-r+w/+r+w/-r+r+w/-w+w/-r-w+w/+r-w+w/-r+r-w+w", &stm32_protection, 15, "select protection state; default %s"},

//  {ARGS_INTEGER, "-PRT", 0, &stm32_protection, 1, ""},
//  {ARGS_INTEGER, "--protection", 0, &stm32_protection, 1, "select protection state"},

#define	STM32_READ_UNPROTECT	0x0001
#define	STM32_READ_PROTECT	0x0002
#define	STM32_WRITE_UNPROTECT	0x0004
#define	STM32_WRITE_PROTECT	0x0008

#define	STM32_READ_PROTECTED	0x0100
  
/**
          "\t-i identify chip\n"
          "\t-e erase (optimized)\n"
          "\t-E erase entire FLASH\n"
          //"\t-b blank check\n"
          "\t-v verify after programming\n"
          //"\t-f compare to file\n"
          "\t-o output to file\n"
          "\t-x execute\n"
          "\t-t terminal\n"
          "\t-r reset\n"
**/

extern int stm32_load (COM_HANDLE tty, const file_data_type *data);
extern int stm32_read (COM_HANDLE tty, const char *filename);

#endif
