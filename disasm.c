// ============================================================================
// Z80 disassembler
// (C) 2014 Hynek Sladky
// ============================================================================

#include "global.h"
//#define Z80_INTERNALS
//#include "z80/z80.h"
#include <avr/pgmspace.h>
#include "main.h"

/**

ToDo:
====
o podpora pro cteni z bufferu instrukce
o podpora pro funkce tisku z main.c
o doplnit volani pgm_read_byte ...

o podpora pro flagy ADL, MADL
  tedy i pro adresy a data 24 bitu!!
o podpora prefixu SIS, SIL, LIS, LIL
o dale operace IXL, IXH, IN0, OUT0, INI2, INI2R, INIM, INIMR, INIRX, LD (HL),dreg, LD (IX+dis),dreg, LEA..., PEA..., RSMIX, STMIX
  operace IXL/IXH by mely byt uz davno podporovane (viz nedokumentovane instrukce Z80)

- instrukce DD/FD vyuzivaji pouze premapovani registru...
  jenze pro eZ80 to nestaci...
  {0x07, 0xCF, LD dd,(IX+d) -> ale dd je BC, DE, HL, IX/IY (misto SP)
  {0x0F, 0xCF, LD (IX+d),dd    --''--
  {0x31, 0xFF, LD IY,(IX+d) -> pro FD jsou IX a IY opacne...
  {0x3E, 0xFF, LD (IX+d),IY    --''--
- optimalizovat pole addr (jeden bit je ulozeny v samotnem bytu)
  type+len+size ma dohromady 16 bitu...
  dva bity addr by musely byt spojene treba s name_idx
    tedy jen pokud name_idx < 63

- rozdelit instrukce podle typu...
  zjednodusi to struktury...
- jeste pridat priznak, kde se da cekat modifikace DD/FD
- asi by se hodilo sjednotit parametry -> pouzit odkazy na tiskovy format -> tj. co se bude tisknout

- asi tam jeste chybi spousta instrukci...
  -> zkontrolovat vsechny instrukce dle tabulek!
- nazvy instrukci odkazovat podle indexu


**/

// ------------------------------------
// compile control
// ------------------------------------
#if 1
#define	Z180
#define	eZ80
// check
#ifdef Z180
#ifndef eZ80
#error "eZ80 defined without Z180!"
#endif
#endif
#endif

// ------------------------------------
// parameter type definitions
// ------------------------------------
enum {
  PAR_NONE,
// implicit registers
#define	REG_FIRST	REG_B
  REG_B, REG_C, REG_D, REG_E, REG_H, REG_L, REG_MEM, REG_A, //REG_F,
  REG_IXH, REG_IXL, REG_IYH, REG_IYL, REG_R, REG_I, REG_MB,
  DREG_BC, DREG_DE, DREG_HL, DREG_SP, DREG_AF,
  DREG_IX, DREG_IY, REG_IXdis, REG_IYdis,
#define	REG_LAST	REG_IYdis
#define	COND_FIRST	COND_NZ
  COND_NZ, COND_Z, COND_NC, COND_C, COND_PO, COND_PE, COND_P, COND_M,
#define	COND_LAST	COND_M
// parameters
#define	IMM_FIRST	IMM_UNSIGNED
  IMM_UNSIGNED,
//  IMM_SIGNED,
  IMM_INTMODE,
  IMM_RSTMODE,
  IMM_PCREL,
#define	IMM_LAST	IMM_PCREL
// tables
#define	TAB_FIRST	TAB_REG
  TAB_REG,
  TAB_DREG_SP,
  TAB_DREG_AF,
#ifdef eZ80
  TAB_DREG_IX,
  TAB_DREG_IY,
#endif
//  TAB_ALU,
  TAB_COND,
#define	TAB_LAST	TAB_COND
};

// ------------------------------------
// register name strings
// ------------------------------------
static const char reg_names[] PROGMEM =
  "??\0"
  "B\0""C\0""D\0""E\0""H\0""L\0""(HL)\0""A\0" "IXH\0""IXL\0" "IYH\0""IYL\0" "R\0""I\0" "MB\0"
  "BC\0""DE\0""HL\0""SP\0" "AF\0"
  "IX\0""IY\0" "IX+$%02X\0""IY+$%02X\0"
  "NZ\0""Z\0""NC\0""C\0""PO\0""PE\0""P\0""M\0";
// ------------------------------------
// field tables
// ------------------------------------
static const unsigned char cond[8] PROGMEM = {COND_NZ, COND_Z, COND_NC, COND_C, COND_PO, COND_PE, COND_P, COND_M};
static const unsigned char reg[8] PROGMEM = {REG_B, REG_C, REG_D, REG_E, REG_H, REG_L, REG_MEM, REG_A};
static const unsigned char dreg_sp[4] PROGMEM = {DREG_BC, DREG_DE, DREG_HL, DREG_SP};
static const unsigned char dreg_af[4] PROGMEM = {DREG_BC, DREG_DE, DREG_HL, DREG_AF};
#ifdef eZ80
static const unsigned char dreg_ix[4] PROGMEM = {DREG_BC, DREG_DE, DREG_HL, DREG_IX};
static const unsigned char dreg_iy[4] PROGMEM = {DREG_BC, DREG_DE, DREG_HL, DREG_IY};
static const unsigned char * const tabs[] PROGMEM = {reg, dreg_sp, dreg_af, dreg_ix, dreg_iy, cond};
#else
static const unsigned char * const tabs[] PROGMEM = {reg, dreg_sp, dreg_af, cond};
#endif
// ------------------------------------
// IX/IY replacement table
// ------------------------------------
static const unsigned char ixy_reg[][3] PROGMEM = {
  {REG_MEM, REG_IXdis, REG_IYdis},
  {DREG_HL, DREG_IX, DREG_IY},
  {REG_H, REG_IXH, REG_IYH},
  {REG_L, REG_IXL, REG_IYL},
  {0}
};

// ------------------------------------
// instruction name definitions
// ------------------------------------
enum {
  INST_NOP, INST_EX, INST_EXX, INST_RLCA, INST_RRCA, INST_RLA, INST_RRA, INST_DAA, INST_CPL, INST_SCF, INST_CCF,
  INST_HALT, INST_RET, INST_DI, INST_EI, INST_LD, INST_JP, INST_DJNZ, INST_JR, INST_OUT, INST_IN, INST_CALL,
  INST_ADD, INST_ADC, INST_SUB, INST_SBC, INST_AND, INST_XOR, INST_OR, INST_CP,
  INST_INC, INST_DEC, INST_POP, INST_PUSH, INST_RST,
  INST_RLC, INST_RRC, INST_RL, INST_RR, INST_SLA, INST_SRA, INST_SLL, INST_SRL, INST_BIT, INST_RES, INST_SET,
  INST_NEG, INST_RETN, INST_RETI, INST_IM, INST_RRD, INST_RLD,
  INST_LDI, INST_CPI, INST_INI, INST_OUTI, INST_LDIR, INST_CPIR, INST_INIR, INST_OTIR,
  INST_LDD, INST_CPD, INST_IND, INST_OUTD, INST_LDDR, INST_CPDR, INST_INDR, INST_OTDR,
#ifdef Z180
  // Z180 instructions
  INST_SLP, INST_MLT, INST_TST, INST_TSTIO,
  INST_IN0, INST_OUT0,
  INST_OTIM, INST_OTIMR, INST_OTDM, INST_OTDMR,
#ifdef eZ80
  // eZ80 instructions
  INST_LEA, INST_PEA,
  INST_RSMIX, INST_STMIX,
  INST_INIM, INST_INIMR, INST_INI2, INST_INI2R, INST_INDM, INST_INDMR, INST_IND2, INST_IND2R,
  INST_OUTI2, INST_OTI2R, INST_OUTD2, INST_OTD2R, INST_INIRX, INST_OTIRX, INST_INDRX, INST_OTDRX,
#endif
#endif
  INST_COUNT
};
// ------------------------------------
// instruction name strings
// ------------------------------------
static const char inst_names[] PROGMEM =
  "NOP\0" "EX\0" "EXX\0" "RLCA\0" "RRCA\0" "RLA\0" "RRA\0" "DAA\0" "CPL\0" "SCF\0" "CCF\0"
  "HALT\0" "RET\0" "DI\0" "EI\0" "LD\0" "JP\0" "DJNZ\0" "JR\0" "OUT\0" "IN\0" "CALL\0"
  "ADD\0" "ADC\0" "SUB\0" "SBC\0" "AND\0" "XOR\0" "OR\0" "CP\0"
  "INC\0" "DEC\0" "POP\0" "PUSH\0" "RST\0"
  "RLC\0" "RRC\0" "RL\0" "RR\0" "SLA\0" "SRA\0" "SLL\0" "SRL\0" "BIT\0" "RES\0" "SET\0"
  "NEG\0" "RETN\0" "RETI\0" "IM\0" "RRD\0" "RLD\0"
  "LDI\0" "CPI\0" "INI\0" "OUTI\0" "LDIR\0" "CPIR\0" "INIR\0" "OTIR\0"
  "LDD\0" "CPD\0" "IND\0" "OUTD\0" "LDDR\0" "CPDR\0" "INDR\0" "OTDR\0"
#ifdef Z180
  "SLP\0" "MLT\0" "TST\0" "TSTIO\0"
  "IN0\0" "OUT0\0"
  "OTIM\0" "OTIMR\0" "OTDM\0" "OTDMR\0"
#ifdef eZ80
  "LEA\0" "PEA\0"
  "RSMIX\0" "STMIX\0"
  "INIM\0" "INIMR\0" "INI2\0" "INI2R\0" "INDM\0" "INDMR\0" "IND2\0" "IND2R\0"
  "OUTI2\0" "OTI2R\0" "OUTD2\0" "OTD2R\0" "INIRX\0" "OTIRX\0" "INDRX\0" "OTDRX\0"
#endif
#endif
;

#ifdef eZ80
// ------------------------------------
// eZ80 explicit suffixes
// ------------------------------------
static const char sfx_name[] PROGMEM = 
  ".SIS\0"
  ".SIL\0"
  ".LIS\0"
  ".LIL\0"
;
#endif

// ------------------------------------
// instruction storage structure
// ------------------------------------
#if INST_COUNT > 10
#error "NEJDE!!"
#endif
typedef struct {
  unsigned char opcode, mask;
  unsigned char name_idx;
  struct {
#if 0
    unsigned type:6;
    unsigned addr:1;
    unsigned pos:5;
    unsigned len:5;
#else
    unsigned char type;
#warning "optimalizace obsazeni pameti!"
    unsigned char addr; // tohle je jen 1-bitova promenna!!! lze sloucit s type, pos, len...
    unsigned char pos;
    unsigned char len;
#endif
  } par[2];
} instr_type;
// ------------------------------------
// unprefixed
// ------------------------------------
static const instr_type op[] PROGMEM = {
// instructions with no parameter
//	11011101	<DD> prefix
//	11101101	<ED> prefix
//	11111101	<FD> prefix
#ifdef eZ80
// special eZ80 instructions prefixed with 0xDD/0xFD


#endif
//	00000000	NOP
  {0x00, 0xFF, INST_NOP, {{0}, {0}}},
//	00001000	EX AF,AF
  {0x08, 0xFF, INST_EX, {{DREG_AF}, {DREG_AF}}},
//	000..111	RLCA RLA RRCA RRA
  {0x07, 0xFF, INST_RLCA, {{0}, {0}}},
  {0x0F, 0xFF, INST_RRCA, {{0}, {0}}},
  {0x17, 0xFF, INST_RLA, {{0}, {0}}},
  {0x1F, 0xFF, INST_RRA, {{0}, {0}}},
//	00100111	DAA
  {0x27, 0xFF, INST_DAA, {{0}, {0}}},
//	00101111	CPL
  {0x2F, 0xFF, INST_CPL, {{0}, {0}}},
//	00110111	SCF
  {0x37, 0xFF, INST_SCF, {{0}, {0}}},
//	00111111	CCF
  {0x3F, 0xFF, INST_CCF, {{0}, {0}}},
//	01110110	HALT
  {0x76, 0xFF, INST_HALT, {{0}, {0}}},
//	11001001	RET
  {0xC9, 0xFF, INST_RET, {{0}, {0}}},
//	11001011	<CB> prefix
//	11011001	EXX
  {0xD9, 0xFF, INST_EXX, {{0}, {0}}},
//	11100011	EX (SP),HL
  {0xE3, 0xFF, INST_EX, {{DREG_SP, 1}, {DREG_HL}}},
//	11101011	JP (HL)
  {0xEB, 0xFF, INST_EX, {{DREG_DE}, {DREG_HL}}},
//	11110011	DI
  {0xF3, 0xFF, INST_DI, {{0}, {0}}},
//	11111011	EI
  {0xFB, 0xFF, INST_EI, {{0}, {0}}},
//	11101001	JP (HL)
  {0xE9, 0xFF, INST_JP, {{DREG_HL}, {0}}},
//	11111001	LD SP,HL
  {0xF9, 0xFF, INST_LD, {{DREG_SP}, {DREG_HL}}},

// instructions with one parameter

// parameter not in opcode

//	00010000 NN	DJNZ NN
  {0x10, 0xFF, INST_DJNZ, {{IMM_PCREL, 0, 8, 8}, {0}}},
//	00011000 NN	JR NN
  {0x18, 0xFF, INST_JR, {{IMM_PCREL, 0, 8, 8}, {0}}},
//	00100010 NNNN	LD (NNNN),HL
  {0x22, 0xFF, INST_LD, {{IMM_UNSIGNED, 1, 8, 16}, {DREG_HL}}},
//	00101010 NNNN	LD HL,(NNNN)
  {0x2A, 0xFF, INST_LD, {{DREG_HL}, {IMM_UNSIGNED, 1, 8, 16}}},
//	00100010 NNNN	LD (NNNN),A
  {0x32, 0xFF, INST_LD, {{IMM_UNSIGNED, 1, 8, 16}, {REG_A}}},
//	00101010 NNNN	LD A,(NNNN)
  {0x3A, 0xFF, INST_LD, {{REG_A}, {IMM_UNSIGNED, 1, 8, 16}}},
//	11000011 NNNN	JP NNNN
  {0xC3, 0xFF, INST_JP, {{IMM_UNSIGNED, 0, 8, 16}, {0}}},
//	11010011	OUT (NN),A
  {0xD3, 0xFF, INST_OUT, {{IMM_UNSIGNED, 1, 8, 8}, {REG_A}}},
//	11011011	IN A,(NN)
  {0xDB, 0xFF, INST_IN, {{REG_A}, {IMM_UNSIGNED, 1, 8, 8}}},
//	11001101 NNNN	CALL NNNN
  {0xCD, 0xFF, INST_CALL, {{IMM_UNSIGNED, 0, 8, 16}, {0}}},
//	11aaa110 NN	ALU A,NN
  {0xC6, 0xFF, INST_ADD, {{REG_A}, {IMM_UNSIGNED, 0, 8, 8}}},
  {0xCE, 0xFF, INST_ADC, {{REG_A}, {IMM_UNSIGNED, 0, 8, 8}}},
  {0xD6, 0xFF, INST_SUB, {{IMM_UNSIGNED, 0, 8, 8}, {0}}},
  {0xDE, 0xFF, INST_SBC, {{REG_A}, {IMM_UNSIGNED, 0, 8, 8}}},
  {0xE6, 0xFF, INST_AND, {{IMM_UNSIGNED, 0, 8, 8}, {0}}},
  {0xEE, 0xFF, INST_XOR, {{IMM_UNSIGNED, 0, 8, 8}, {0}}},
  {0xF6, 0xFF, INST_OR, {{IMM_UNSIGNED, 0, 8, 8}, {0}}},
  {0xFE, 0xFF, INST_CP, {{IMM_UNSIGNED, 0, 8, 8}, {0}}},

// parameter in opcode
//	00dd1001	ADD HL,%D
  {0x09, 0xCF, INST_ADD, {{DREG_HL}, {TAB_DREG_SP, 0, 4, 2}}},
//	00dd0010	LD (DD),A
  {0x02, 0xCF, INST_LD, {{TAB_DREG_SP, 1, 4, 2}, {REG_A}}},
//	00dd1010	LD A,(DD)
  {0x0A, 0xCF, INST_LD, {{REG_A}, {TAB_DREG_SP, 1, 4, 2}}},
//	00dd0011	INC DD
  {0x03, 0xCF, INST_INC, {{TAB_DREG_SP, 0, 4, 2}, {0}}},
//	00dd1011	DEC DD
  {0x0B, 0xCF, INST_DEC, {{TAB_DREG_SP, 0, 4, 2}, {0}}},
//	00rrr100	INC r
  {0x04, 0xC7, INST_INC, {{TAB_REG, 0, 3, 3}, {0}}},
//	00rrr101	DEC r
  {0x05, 0xC7, INST_DEC, {{TAB_REG, 0, 3, 3}, {0}}},
//	11ccc000	RET cc
  {0xC0, 0xC7, INST_RET, {{TAB_COND, 0, 3, 3}, {0}}},
//	11qq0001	POP DD
  {0xC1, 0xCF, INST_POP, {{TAB_DREG_AF, 0, 4, 2}, {0}}},
//	11qq0101	PUSH Q
  {0xC5, 0xCF, INST_PUSH, {{TAB_DREG_AF, 0, 4, 2}, {0}}},
//	11ttt111	RST tt
  {0xC7, 0xC7, INST_RST, {{IMM_RSTMODE, 0, 3, 3}, {0}}},
// two parameters, one in opcode
//	001cc000 NN	JR cc,
  {0x20, 0xE7, INST_JR, {{TAB_COND, 0, 3, 2}, {IMM_PCREL, 0, 8, 8}}},
//	00dd0001 NNNN	LD %D,NNNN
  {0x01, 0xCF, INST_LD, {{TAB_DREG_SP, 0, 4, 2}, {IMM_UNSIGNED, 0, 8, 16}}},
//	00rrr110 NN	LD r,NN
  {0x06, 0xC7, INST_LD, {{TAB_REG, 0, 3, 3}, {IMM_UNSIGNED, 0, 8, 8}}},
//	11ccc010 NNNN	JP cc,NNNN
  {0xC2, 0xC7, INST_JP, {{TAB_COND, 0, 3, 3}, {IMM_UNSIGNED, 0, 8, 16}}},
//	11ccc100 NNNN	CALL cc,NNNN
  {0xC4, 0xC7, INST_CALL, {{TAB_COND, 0, 3, 3}, {IMM_UNSIGNED, 0, 8, 16}}},

// both parameters in opcode
//	01rrrRRR	LD r,R
  {0x40, 0xC0, INST_LD, {{TAB_REG, 0, 3, 3}, {TAB_REG, 0, 0, 3}}},
//	10aaarrr	ALU A,r
  {0x80, 0xF8, INST_ADD, {{REG_A}, {TAB_REG, 0, 0, 3}}},
  {0x88, 0xF8, INST_ADC, {{REG_A}, {TAB_REG, 0, 0, 3}}},
  {0x90, 0xF8, INST_SUB, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x98, 0xF8, INST_SBC, {{REG_A}, {TAB_REG, 0, 0, 3}}},
  {0xA0, 0xF8, INST_AND, {{TAB_REG, 0, 0, 3}, {0}}},
  {0xA8, 0xF8, INST_XOR, {{TAB_REG, 0, 0, 3}, {0}}},
  {0xB0, 0xF8, INST_OR, {{TAB_REG, 0, 0, 3}, {0}}},
  {0xB8, 0xF8, INST_CP, {{TAB_REG, 0, 0, 3}, {0}}},
  {0}
};
  
// ------------------------------------
// CB prefix
// ------------------------------------
static const instr_type cbop[] PROGMEM = {  
//	00wwwrrr	RLC | RRC | RL | RR | SLA | SRA | ??? | SRL
  {0x00, 0xF8, INST_RLC, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x08, 0xF8, INST_RRC, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x10, 0xF8, INST_RL, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x18, 0xF8, INST_RR, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x20, 0xF8, INST_SLA, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x28, 0xF8, INST_SRA, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x30, 0xF8, INST_SLL, {{TAB_REG, 0, 0, 3}, {0}}},
  {0x38, 0xF8, INST_SRL, {{TAB_REG, 0, 0, 3}, {0}}},
//	01bbbrrr	BIT
  {0x40, 0xC0, INST_BIT, {{IMM_UNSIGNED, 0, 3, 3}, {TAB_REG, 0, 0, 3}}},
//	10bbbrrr	RES
  {0x80, 0xC0, INST_RES, {{IMM_UNSIGNED, 0, 3, 3}, {TAB_REG, 0, 0, 3}}},
//	11bbbrrr	SET
  {0xC0, 0xC0, INST_SET, {{IMM_UNSIGNED, 0, 3, 3}, {TAB_REG, 0, 0, 3}}},
  {0}
};

// ------------------------------------
// ED prefix
// ------------------------------------
static const instr_type edop[] PROGMEM = {
//	01rrr000	IN r,(C)
  {0x40, 0xC7, INST_IN, {{TAB_REG, 0, 3, 3}, {REG_C, 1}}},
//	01rrr001	OUT (C),r
  {0x41, 0xC7, INST_OUT, {{REG_C, 1}, {TAB_REG, 0, 3, 3}}},

//	01dd0010	SBC HL,DD
  {0x42, 0xCF, INST_SBC, {{DREG_HL}, {TAB_DREG_SP, 0, 4, 2}}},
//	01dd1010	ADC HL,DD
  {0x4A, 0xCF, INST_ADC, {{DREG_HL}, {TAB_DREG_SP, 0, 4, 2}}},
//	01dd0011 NNNN	LD (NNNN),D
  {0x43, 0xCF, INST_LD, {{IMM_UNSIGNED, 1, 8, 16}, {TAB_DREG_SP, 0, 4, 2}}},
//	01dd1011 NNNN	LD D,(NNNN)
  {0x4B, 0xCF, INST_LD, {{TAB_DREG_SP, 0, 4, 2}, {IMM_UNSIGNED, 1, 8, 16}}},
//	01000100	NEG
  {0x44, 0xFF, INST_NEG, {{0}, {0}}},
//	01000101	RETN
  {0x45, 0xFF, INST_RETN, {{0}, {0}}},
//	01001101	RETI
  {0x4D, 0xFF, INST_RETI, {{0}, {0}}},
//	010..110	IM ??
  {0x46, 0xE7, INST_IM, {{IMM_INTMODE, 0, 3, 2}, {0}}},
//	01000111	LD I,A
  {0x47, 0xFF, INST_LD, {{REG_I}, {REG_A}}},
//	01001111	LD R,A
  {0x4F, 0xFF, INST_LD, {{REG_R}, {REG_A}}},
//	01010111	LD A,I
  {0x57, 0xFF, INST_LD, {{REG_A}, {REG_I}}},
//	01011111	LD A,R
  {0x5F, 0xFF, INST_LD, {{REG_A}, {REG_R}}},
//	01100111	RRD
  {0x67, 0xFF, INST_RRD, {{0}, {0}}},
//	01101111	RLD
  {0x6F, 0xFF, INST_RLD, {{0}, {0}}},

  {0xA0, 0xFF, INST_LDI, {{0}, {0}}},
  {0xA1, 0xFF, INST_CPI, {{0}, {0}}},
  {0xA2, 0xFF, INST_INI, {{0}, {0}}},
  {0xA3, 0xFF, INST_OUTI, {{0}, {0}}},
  {0xB0, 0xFF, INST_LDIR, {{0}, {0}}},
  {0xB1, 0xFF, INST_CPIR, {{0}, {0}}},
  {0xB2, 0xFF, INST_INIR, {{0}, {0}}},
  {0xB3, 0xFF, INST_OTIR, {{0}, {0}}},
  {0xA8, 0xFF, INST_LDD, {{0}, {0}}},
  {0xA9, 0xFF, INST_CPD, {{0}, {0}}},
  {0xAA, 0xFF, INST_IND, {{0}, {0}}},
  {0xAB, 0xFF, INST_OUTD, {{0}, {0}}},
  {0xB8, 0xFF, INST_LDDR, {{0}, {0}}},
  {0xB9, 0xFF, INST_CPDR, {{0}, {0}}},
  {0xBA, 0xFF, INST_INDR, {{0}, {0}}},
  {0xBB, 0xFF, INST_OTDR, {{0}, {0}}},

#ifdef Z180
// Z180 opcodes
//	00rrr000	IN0 r,(NN)
  {0x00, 0xC7, INST_IN0, {{TAB_REG, 0, 3, 3}, {IMM_UNSIGNED, 1, 8, 8}}},
//	00rrr001	OUT0 (NN),r
  {0x01, 0xC7, INST_OUT0, {{IMM_UNSIGNED, 1, 8, 8}, {TAB_REG, 0, 3, 3}}},
//	00rrr100	TST A,r
  {0x04, 0xC7, INST_TST, {{REG_A}, {TAB_REG, 0, 3, 3}}},
//	01dd1100	MLT dd
  {0x4C, 0xCF, INST_MLT, {{TAB_DREG_SP, 0, 4, 2}, {0}}},
  {0x64, 0xFF, INST_TST, {{REG_A}, {IMM_UNSIGNED, 0, 8, 8}}},
  {0x74, 0xFF, INST_TSTIO, {{IMM_UNSIGNED, 0, 8, 8}, {0}}},
  {0x66, 0xFF, INST_SLP, {{0}, {0}}},
  {0x83, 0xFF, INST_OTIM, {{0}, {0}}},
  {0x93, 0xFF, INST_OTIMR, {{0}, {0}}},
  {0x8B, 0xFF, INST_OTDM, {{0}, {0}}},
  {0x9B, 0xFF, INST_OTDMR, {{0}, {0}}},
#ifdef eZ80
// eZ80 opcodes
//	00dd0010	LEA dd,(IX+d)
  {0x02, 0xCF, INST_LEA, {{TAB_DREG_IX, 0, 4, 2}, {REG_IXdis, 1, 8, 8}}},
//	00dd0011	LEA dd,(IY+d)
  {0x03, 0xCF, INST_LEA, {{TAB_DREG_IY, 0, 4, 2}, {REG_IYdis, 1, 8, 8}}},
//	00dd0111	LD dd,(HL)
  {0x07, 0xCF, INST_LD, {{TAB_DREG_IX, 0, 4, 2}, {REG_MEM, 1}}},
  {0x31, 0xFF, INST_LD, {{DREG_IY}, {REG_MEM, 1}}},
//	00dd1111	LD (HL),dd
  {0x0F, 0xCF, INST_LD, {{REG_MEM, 1}, {TAB_DREG_IX, 0, 4, 2}}},
  {0x3E, 0xFF, INST_LD, {{REG_MEM, 1}, {DREG_IY}}},
  {0x54, 0xFF, INST_LEA, {{DREG_IX}, {REG_IYdis, 1, 8, 8}}},
  {0x55, 0xFF, INST_LEA, {{DREG_IY}, {REG_IXdis, 1, 8, 8}}},
  {0x65, 0xFF, INST_PEA, {{REG_IXdis, 0, 8, 8}}},
  {0x66, 0xFF, INST_PEA, {{REG_IYdis, 0, 8, 8}}},
  {0x6D, 0xFF, INST_LD, {{REG_MB}, {REG_A}}},
  {0x6E, 0xFF, INST_LD, {{REG_A}, {REG_MB}}},
  {0x7D, 0xFF, INST_STMIX, {{0}, {0}}},
  {0x7E, 0xFF, INST_RSMIX, {{0}, {0}}},
  {0x82, 0xFF, INST_INIM, {{0}, {0}}},
  {0x92, 0xFF, INST_INIMR, {{0}, {0}}},
  {0x84, 0xFF, INST_INI2, {{0}, {0}}},
  {0x94, 0xFF, INST_INI2R, {{0}, {0}}},
  {0x8A, 0xFF, INST_INDM, {{0}, {0}}},
  {0x9A, 0xFF, INST_INDMR, {{0}, {0}}},
  {0x8C, 0xFF, INST_IND2, {{0}, {0}}},
  {0x9C, 0xFF, INST_IND2R, {{0}, {0}}},
  {0xA4, 0xFF, INST_OUTI2, {{0}, {0}}},
  {0xB4, 0xFF, INST_OTI2R, {{0}, {0}}},
  {0xAC, 0xFF, INST_OUTD2, {{0}, {0}}},
  {0xBC, 0xFF, INST_OTD2R, {{0}, {0}}},
  {0xC2, 0xFF, INST_INIRX, {{0}, {0}}},
  {0xC3, 0xFF, INST_OTIRX, {{0}, {0}}},
  {0xCA, 0xFF, INST_INDRX, {{0}, {0}}},
  {0xCB, 0xFF, INST_OTDRX, {{0}, {0}}},
  {0xC7, 0xFF, INST_LD, {{REG_I}, {DREG_HL}}},
  {0xD7, 0xFF, INST_LD, {{DREG_HL}, {REG_I}}},
#endif
#endif
  {0}
};

// ====================================
// get value from memory
// ------------------------------------
static unsigned long get_val (unsigned char *instr, unsigned char start, unsigned char len, unsigned char flags) {
union {
  unsigned char b[4];
  unsigned long l;
} val;
  while (start >= 8) {
    start -= 8;
    instr++;
  }
  val.l = *instr;
  if (len > 8) {
    val.b[1] = *++instr;
    if (flags & 0x01) {
      val.b[2] = *++instr;
      len += 8;
    }
  }
  if (start)
    val.l >>= start;
  val.l &= (1UL << len) - 1;
  return val.l;
}
// ====================================
// get packed string by index
// ------------------------------------
static const char *get_str (const char *buf, unsigned char type) {
const char *s = buf;
  while (pgm_read_byte (s)) {
    if (!type)
      return s;
    while (pgm_read_byte (s))
      s++;
    s++;
    type--;
  }
  return buf;
}
// ====================================
// print instruction
// ------------------------------------
#ifdef eZ80
#define	IMM_LONG	0x01
#define	REG_LONG	0x02
#define	FMT_LONG	(REG_LONG | IMM_LONG)
#define	FMT_MASK	(REG_LONG | IMM_LONG)
#define	FMT_SFX		0x04
#define	FMT_SIS		(FMT_SFX)
#define	FMT_SIL		(FMT_SFX | IMM_LONG)
#define	FMT_LIS		(FMT_SFX | REG_LONG)
#define	FMT_LIL		(FMT_SFX | REG_LONG | IMM_LONG)
#endif
// ------------------------------------
unsigned char z80_disasm (unsigned char *instr, unsigned long addr, unsigned char flags) {
unsigned char ixy = 0, len = 1;
const instr_type *tab = op;
unsigned char b, idx, tmp;
unsigned char type1, type2;
unsigned long val1 = 0, val2 = 0;

  b = *instr;
#ifdef eZ80
  if (flags)
    flags = FMT_LONG;
  // get 16/24 bit prefix
  while (b == 0x40 || b == 0x49 || b == 0x52 || b == 0x5B) {
    if (b == 0x40)
      flags = FMT_SIS;
    else
    if (b == 0x49)
      flags = FMT_LIS;
    else
    if (b == 0x52)
      flags = FMT_SIL;
    else
      flags = FMT_LIL;
    b = *++instr;
    //addr++;
    len++;
  }
#endif
  // get IX/IY prefix
  while (b == 0xDD || b == 0xFD) {
    if (b == 0xDD)
      ixy = 1;
    else
      ixy = 2;
    b = *++instr;
    //addr++;
    len++;
  }
  // check CB or ED prefix
  if (b == 0xCB) {
    tab = cbop;
    // process DD/FD+CB
    if (ixy) {
      val1 = val2 = /*(signed char)*/*++instr;
      //addr++;
      len++;
    }
    b = *++instr;
    //addr++;
    len++;
  } else
  if (b == 0xED) {
    tab = edop;
    b = *++instr;
    //addr++;
    len++;
  }
  // disassemble instruction
  idx = 0;
  tmp = pgm_read_byte (&tab[idx].mask);
  while (tmp && (b & tmp) != pgm_read_byte (&tab[idx].opcode)) {
    idx++;
    tmp = pgm_read_byte (&tab[idx].mask);
  }
  if (!tmp) {
    // instruction not found
    dputs (PSTR ("DB\t"));
    instr -= len;
    for (type1 = len; type1; type1--) {
      dprint (PSTR ("$%02X"), *instr++);
      if (type1 > 1)
	dputc (',');
    }
    dputs (PSTR (" ; UNKNOWN!"));
    return len;
  }
  // print instruction
  //dprintf ("\t%s", get_str (inst_names, tab[idx].name_idx));
  dputs (get_str (inst_names, pgm_read_byte (&tab[idx].name_idx)));
#ifdef eZ80
  if (flags & FMT_SFX)
    dputs (get_str (sfx_name, flags & FMT_MASK));
#endif
  // prepare parameters
  type1 = pgm_read_byte (&tab[idx].par[0].type);
  if (!type1)
    return len;
  if (type1 >= TAB_FIRST && type1 <= TAB_LAST)
    type1 = pgm_read_byte (&(((unsigned char*)pgm_read_word (&tabs[type1-TAB_FIRST]))[get_val (instr, pgm_read_byte (&tab[idx].par[0].pos), pgm_read_byte (&tab[idx].par[0].len), flags)]));
  type2 = pgm_read_byte (&tab[idx].par[1].type);
  if (type2 >= TAB_FIRST && type2 <= TAB_LAST)
    type2 = pgm_read_byte (&(((unsigned char*)pgm_read_word (&tabs[type2-TAB_FIRST]))[get_val (instr, pgm_read_byte (&tab[idx].par[1].pos), pgm_read_byte (&tab[idx].par[1].len), flags)]));
  // process IX/IY prefix
  if (ixy) {
    if (tab != cbop) {
      // process IXY relative adressing
      if (type1 == REG_MEM) {
	val1 = /*(signed char)*/*++instr;
        //addr++;
	len++;
      } else
      if (type2 == REG_MEM) {
	val2 = /*(signed char)*/*++instr;
        //addr++;
	len++;
      }
    }
    // IX/IY replacement
    if (*instr != 0xEB) {
      b = 0;
      tmp = pgm_read_byte (&ixy_reg[b][0]);
      while (tmp) {
	if (tmp == type1) {
	  type1 = pgm_read_byte (&ixy_reg[b][ixy]);
	  break;
	}
	if (tmp == type2) {
	  type2 = pgm_read_byte (&ixy_reg[b][ixy]);
	  break;
	}
	b++;
	tmp = pgm_read_byte (&ixy_reg[b][0]);
      }
    }
#warning "specialni formy DD/FD+CB!!"
/**
    if (tab == cbop) {
      if (!type2) {
	type2 = type1;
	type1 = (ixy == 1) ? REG_IXdis : REG_IYdis;
      } else {
	type2 = (ixy == 1) ? REG_IXdis : REG_IYdis;
      }
    }
**/
  }
  // get immediate values
  if (type1 >= IMM_FIRST && type1 <= IMM_LAST) {
    val1 = get_val (instr, pgm_read_byte (&tab[idx].par[0].pos), pgm_read_byte (&tab[idx].par[0].len), flags);
    if (type1 == IMM_INTMODE) {
      if (val1)
	val1--;
      type1 = IMM_UNSIGNED;
    }
  }
  if (type2 >= IMM_FIRST && type2 <= IMM_LAST) {
    val2 = get_val (instr, pgm_read_byte (&tab[idx].par[1].pos), pgm_read_byte (&tab[idx].par[1].len), flags);
/**
    if (type2 == IMM_INTMODE) {
      if (val2)
	val2--;
      type2 = IMM_UNSIGNED;
    }
**/
  }
  // update position/length
  tmp = pgm_read_byte (&tab[idx].par[0].len);
  // adjust for 24-bit immediate
  if (tmp >= 16 && (flags & IMM_LONG))
    tmp += 8;
  // process byte/word immediate
  while (tmp >= 8) {
    //addr++;
    len++;
    //instr++; // not used anymore
    tmp -= 8;
  }
  // update position/length
  tmp = pgm_read_byte (&tab[idx].par[1].len);
  // adjust for 24-bit immediate
  if (tmp >= 16 && (flags & IMM_LONG))
    tmp += 8;
  // process byte/word immediate
  while (tmp >= 8) {
    //addr++;
    len++;
    //instr++; // not used anymore
    tmp -= 8;
  }
  // print parameters
  dputc ('\t');
  if (pgm_read_byte (&tab[idx].par[0].addr) || (type1 >= REG_IXdis && type1 <= REG_IYdis)) 
    dputc ('(');
  if (type1 == IMM_UNSIGNED) {
    //dprintf ((tab[idx].par[0].len > 8) ? "$%04X" : "$%02X", val1);
    tmp = pgm_read_byte (&tab[idx].par[0].len);
    if (tmp > 8)
      dprint (PSTR ("$%04X"), val1);
    else
    if (tmp < 8)
      dprint (PSTR ("%d"), val1);
    else
      dprint (PSTR ("$%02X"), val1);
  } else
  if (type1 == IMM_PCREL)
    dprint (PSTR ("$%04X"), addr + len + (signed char)val1);
  else
  if (type1 == IMM_RSTMODE)
    dprint (PSTR ("$%02X"), val1 << 3);
  else
    dprint (get_str (reg_names, type1), val1);
  if (pgm_read_byte (&tab[idx].par[0].addr) || (type1 >= REG_IXdis && type1 <= REG_IYdis))
    dputc (')');
  if (!type2)
    return len;
  dputc (',');
  if (pgm_read_byte (&tab[idx].par[1].addr) || (type2 >= REG_IXdis && type2 <= REG_IYdis))
    dputc ('(');
  if (type2 == IMM_UNSIGNED) {
    //dprintf ((tab[idx].par[1].len > 8) ? "$%04X" : "$%02X", val2);
    tmp = pgm_read_byte (&tab[idx].par[1].len);
    if (tmp > 8)
      dprint (PSTR ("$%04X"), val2);
    else
    if (tmp < 8)
      dprint (PSTR ("%u"), val2);
    else
      dprint (PSTR ("$%02X"), val2);
  } else
  if (type2 == IMM_PCREL)
    dprint (PSTR ("$%04X"), addr + len + (signed char)val2);
  else
    dprint (get_str (reg_names, type2), val2);
  if (pgm_read_byte (&tab[idx].par[1].addr) || (type2 >= REG_IXdis && type2 <= REG_IYdis))
    dputc (')');
  return len;
}
