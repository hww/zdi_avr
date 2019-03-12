#ifndef _LPC17XX_H_
#define	_LPC17XX_H_

#define	LPC17xx_FLASH_START	0x00000000
#define	LPC17xx_FLASH_END	0x0FFFFFFF

#define	LPC17xx_RAM_START	0x10000000
#define	LPC17xx_RAM_END		0x1FFEFFFF

#define	LPC17xx_ROM_START	0x1FFF0000
#define	LPC17xx_ROM_END		0x1FFFFFFF

#define	LPC17xx_PSRAM_START	0x20000000
#define	LPC17xx_PSRAM_END	0x2008FFFF	// region ends @ 0x3FFFFFFF but 0x20090000 is GPIO area

#define	ARGS_LPC17XX_PARAMS

extern int LPC17xx_load (COM_HANDLE tty, const file_data_type *data);

#endif

