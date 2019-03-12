#ifndef _TIVA_H_
#define	_TIVA_H_

#define	TIVA_FLASH_START	0x00000000
#define	TIVA_FLASH_END		0x00FFFFFF

#define	TIVA_ROM_START		0x01000000
#define	TIVA_ROM_END		0x01FFFFFF

#define	CC13xx_ROM_START	0x10000000
#define	CC13xx_ROM_END		0x1001CBFF

#define	TIVA_RAM_START		0x20000000
#define	TIVA_RAM_END		0x21FFFFFF	// 0x22000000 is bitband region

#define	TIVA_PACKET_LEN		76
// CC13xx: max packet length is 253 == maximum encodable packet length...

#define	ARGS_TIVA_PARAMS

extern unsigned tiva_flags;
#define	TIVA_ROM_BOOT		0x00000001
#define	TIVA_CC13xx		0x00000002

extern int tiva_load (COM_HANDLE hCom, const file_data_type *data);
//extern unsigned packet;
//#define	packet		PACKET_LEN
extern int tiva_read (COM_HANDLE tty, const char *filename);

#endif

