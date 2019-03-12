#ifndef _ZDI_H_
#define _ZDI_H_

// ====================================
// ZDI write-only registers
// ------------------------------------
#define	ZDI_WR(a)	((a << 1))
#define	ZDI_ADDR0L	ZDI_WR (0x00)
#define	ZDI_ADDR0H	ZDI_WR (0x01)
#define	ZDI_ADDR0U	ZDI_WR (0x02)
#define	ZDI_ADDR1L	ZDI_WR (0x04)
#define	ZDI_ADDR1H	ZDI_WR (0x05)
#define	ZDI_ADDR1U	ZDI_WR (0x06)
#define	ZDI_ADDR2L	ZDI_WR (0x08)
#define	ZDI_ADDR2H	ZDI_WR (0x09)
#define	ZDI_ADDR2U	ZDI_WR (0x0A)
#define	ZDI_ADDR3L	ZDI_WR (0x0C)
#define	ZDI_ADDR3H	ZDI_WR (0x0D)
#define	ZDI_ADDR3U	ZDI_WR (0x0E)
#define	ZDI_BRK_CTRL	ZDI_WR (0x10)
#define	ZDI_BRK_NEXT		0x80
#define	ZDI_BRK_ADDR3		0x40
#define	ZDI_BRK_ADDR2		0x20
#define	ZDI_BRK_ADDR1		0x10
#define	ZDI_BRK_ADDR0		0x08
#define	ZDI_IGN_LOW_1		0x04
#define	ZDI_IGN_LOW_0		0x02
#define	ZDI_SINGLE_STEP		0x01
#define	ZDI_MASTER_CTL	ZDI_WR (0x11)
#define	ZDI_RESET		0x80
#define	ZDI_WR_DATA_L	ZDI_WR (0x13)
#define	ZDI_WR_DATA_H	ZDI_WR (0x14)
#define	ZDI_WR_DATA_U	ZDI_WR (0x15)
#define	ZDI_RW_CTL	ZDI_WR (0x16)
#define	ZDI_REG_AF		0
#define	ZDI_REG_BC		1
#define	ZDI_REG_DE		2
#define	ZDI_REG_HL		3
#define	ZDI_REG_IX		4
#define	ZDI_REG_IY		5
#define	ZDI_REG_SP		6
#define	ZDI_REG_PC		7
//#define	ZDI_ADL_1		8
//#define	ZDI_ADL_0		9
//#define	ZDI_REG_EXX		10
#define ADL_SET                 0x08
#define ADL_CLR                 0x09
#define REG_EX                  0x0A
#define	ZDI_REG_MEM		0x0B
#define	ZDI_REG_WR		0x80
#define REG_RD_AF               ZDI_REG_AF
#define REG_RD_BC               ZDI_REG_BC
#define REG_RD_DE               ZDI_REG_DE
#define REG_RD_HL               ZDI_REG_HL
#define REG_RD_IX               ZDI_REG_IX
#define REG_RD_IY               ZDI_REG_IY
#define REG_RD_SP               ZDI_REG_SP
#define REG_RD_PC               ZDI_REG_PC
#define MEM_RD_PC               ZDI_REG_MEM
#define REG_WR_AF               (ZDI_REG_WR | ZDI_REG_AF)
#define REG_WR_BC               (ZDI_REG_WR | ZDI_REG_BC)
#define REG_WR_DE               (ZDI_REG_WR | ZDI_REG_DE)
#define REG_WR_HL               (ZDI_REG_WR | ZDI_REG_HL)
#define REG_WR_IX               (ZDI_REG_WR | ZDI_REG_IX)
#define REG_WR_IY               (ZDI_REG_WR | ZDI_REG_IY)
#define REG_WR_SP               (ZDI_REG_WR | ZDI_REG_SP)
#define REG_WR_PC               (ZDI_REG_WR | ZDI_REG_PC)
#define MEM_WR_PC               (ZDI_REG_WR | ZDI_REG_MEM)
#define	ZDI_BUS_CTL	ZDI_WR (0x17)
#define	ZDI_BUSAK_EN		0x80
#define	ZDI_BUSAK		0x40
#define	ZDI_IS4		ZDI_WR (0x21)
#define	ZDI_IS3		ZDI_WR (0x22)
#define	ZDI_IS2		ZDI_WR (0x23)
#define	ZDI_IS1		ZDI_WR (0x24)
#define	ZDI_IS0		ZDI_WR (0x25)
#define	ZDI_WR_MEM	ZDI_WR (0x30)
// ====================================
// ZDI read-only registers
// ------------------------------------
#define	ZDI_RD(a)	((a << 1) | 0x01)
#define	ZDI_ID_L	ZDI_RD (0x00)
#define	ZDI_ID_H	ZDI_RD (0x01)
#define	ZDI_ID_REV	ZDI_RD (0x02)
#define	ZDI_STAT	ZDI_RD (0x03)
#define	ZDI_ACTIVE		0x80
#define	ZDI_HALT_SLP		0x20
#define	ZDI_ADL			0x10
#define	ZDI_MADL		0x08
#define	ZDI_IEF1		0x04
#define	ZDI_RD_DATA_L	ZDI_RD (0x10)
#define	ZDI_RD_DATA_H	ZDI_RD (0x11)
#define	ZDI_RD_DATA_U	ZDI_RD (0x12)
#define	ZDI_BUS_STAT	ZDI_RD (0x17)
#define	ZDI_BUSACK_EN		0x80
#define	ZDI_BUSACK_STAT		0x40
#define	ZDI_RD_MEM	ZDI_RD (0x20)

// ====================================
// ZDI data type
// ------------------------------------
#if 1
typedef union {
  //unsigned char byte[4];
  struct {
    unsigned char L, H, U, UU;
  };
  struct {
    unsigned short offs;
    unsigned short page;
  };
  unsigned long val;
} data_type;
#else
typedef struct {
  unsigned char L, H, U;
} data_type;
#endif
// ====================================
// ZDI functions
// ------------------------------------
extern void zdi_init (void);

// physical layer functions
extern void zdi_write (unsigned char b);
#define	zdi_addr(addr) \
  BITRES (ZDA_O); \
  zdi_write (addr)
extern unsigned char zdi_read (void);

// byte register access functions
extern unsigned char zdi_read_byte (unsigned char addr);
extern void zdi_write_byte (unsigned char addr, unsigned char val);

// data register access functions
extern data_type zdi_read_val (void);
extern void zdi_write_val (data_type val);

// CPU register access functions
//extern data_type zdi_read_reg (unsigned char reg);
//extern void zdi_write_reg (unsigned char reg, data_type val);

// high-level functions
extern void zdi_open (unsigned char mask);
#define	SAVE_AF		(1 << ZDI_REG_AF)
#define	SAVE_BC		(1 << ZDI_REG_BC)
#define	SAVE_DE		(1 << ZDI_REG_DE)
#define	SAVE_HL		(1 << ZDI_REG_HL)
#define	SAVE_IX		(1 << ZDI_REG_IX)
#define	SAVE_IY		(1 << ZDI_REG_IY)
#define	SAVE_SP		(1 << ZDI_REG_SP)
#define	SAVE_PC		(1 << ZDI_REG_PC)
#if 0
#define	SAVE_AF_
#define	SAVE_BC_
#define	SAVE_DE_
#define	SAVE_HL_
#define	SAVE_SP_
#define	SAVE_STATE_	// IFF, ADL
#endif
#define	SAVE_MASK	(SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL | SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC)
extern void zdi_exx (void);
extern void zdi_pc (data_type val);
extern void zdi_reg (unsigned char mask, data_type *buf);
extern void zdi_close (void);

#endif
