#include "global.h"
#include "zdi.h"

// ====================================
// ZDI
// ====================================
// macros
// ------------------------------------

// send bit (unrolled loop in assembly)
//   there is no delay between CLK and DTA lines!!
#define	SEND_BIT(var,bit) \
  "bst %[" #var "]," #bit "\n" \
  "bld %[REG_L],%[DTA_BIT]\n" \
  "out %[OUT],%[REG_L]\n" \
  "bld %[REG_H],%[DTA_BIT]\n" \
  "out %[OUT],%[REG_H]\n" \
  "out %[OUT],%[REG_L]\n"
// receive bit (unrolled loop in assembly)
#define	RECV_BIT(var,bit) \
  "out %[OUT],%[REG_H]\n" \
  "in __tmp_reg__,%[INP]\n" \
  "bst __tmp_reg__,%[DTA_BIT]\n" \
  "out %[OUT],%[REG_L]\n" \
  "bld %[" #var "]," #bit "\n"

// ====================================
// initialization
// ------------------------------------
void zdi_init (void) {
  BITSET (ZDA_O);
  BITSET (ZDA_D);
  BITSET (ZCL_O);
  BITSET (ZCL_D);
}

// ====================================
// byte output
// ------------------------------------
void __attribute__ ((noinline)) zdi_write (unsigned char b) {
unsigned char tmpH, tmpL;
  // prepare temp registers & ZCL signal
  tmpH = ZDI_O;
  BITRES (ZCL_O);
  tmpL = ZDI_O;
  // send data bits
  asm volatile (
      SEND_BIT (DATA, 7)
      SEND_BIT (DATA, 6)
      SEND_BIT (DATA, 5)
      SEND_BIT (DATA, 4)
      SEND_BIT (DATA, 3)
      SEND_BIT (DATA, 2)
      SEND_BIT (DATA, 1)
      SEND_BIT (DATA, 0)
   : : [OUT] "i" (_SFR_IO_ADDR (ZDI_O)), [DTA_BIT] "i" (ZDA_BIT), [DATA] "r" (b), [REG_L] "r" (tmpL), [REG_H] "r" (tmpH)
  );
  // bit separator
  BITSET (ZDA_O);
  BITSET (ZCL_O);
}

// ====================================
// start command
// ------------------------------------
#ifndef zdi_addr
void zdi_addr (unsigned char addr) {
  // START
  BITRES (ZDA_O);
  zdi_write (addr);
}
#endif
// ====================================
// byte input
// ------------------------------------
unsigned char __attribute__ ((noinline)) zdi_read (void) {
unsigned char b, tmpL, tmpH;
  // prepare temp registers & ZCL signal
  BITRES (ZDA_D);
  tmpH = ZDI_O;
  BITRES (ZCL_O);
  tmpL = ZDI_O;
  // receive data bits
  asm volatile (
      RECV_BIT (DATA, 7)
      RECV_BIT (DATA, 6)
      RECV_BIT (DATA, 5)
      RECV_BIT (DATA, 4)
      RECV_BIT (DATA, 3)
      RECV_BIT (DATA, 2)
      RECV_BIT (DATA, 1)
      RECV_BIT (DATA, 0)
   : [DATA] "+r" (b) : [OUT] "i" (_SFR_IO_ADDR (ZDI_O)), [INP] "i" (_SFR_IO_ADDR (ZDI_I)), [DTA_BIT] "i" (ZDA_BIT), [REG_L] "r" (tmpL), [REG_H] "r" (tmpH)
  );
  // bit separator
  BITSET (ZDA_O);
  BITSET (ZDA_D);
  BITSET (ZCL_O);
  return b;
}

// ====================================
// read 3-byte data register
// ------------------------------------
data_type zdi_read_val (void) {
data_type val;
  zdi_addr (ZDI_RD_DATA_L);
  val.L = zdi_read ();
  val.H = zdi_read ();
  val.U = zdi_read ();
  val.UU = 0;
  return val;
}

// ====================================
// read 1-byte register
// ------------------------------------
unsigned char zdi_read_byte (unsigned char addr) {
unsigned char val;
  zdi_addr (addr);
  val = zdi_read ();
  return val;
}

// ====================================
// write 3-byte data register
// ------------------------------------
void zdi_write_val (data_type val) {
  zdi_addr (ZDI_WR_DATA_L);
  zdi_write (val.L);
  zdi_write (val.H);
  zdi_write (val.U);
}

// ====================================
// write 1-byte register
// ------------------------------------
#if 1
void zdi_write_byte (unsigned char addr, unsigned char val) {
  zdi_addr (addr);
  zdi_write (val);
}
#else
#warning "rychlejsi zapis do regitru - neprinasi nic noveho/lepsiho..."
void __attribute__ ((noinline)) zdi_write_byte (unsigned char addr, unsigned char data) {
unsigned char tmpH, tmpL;
  BITRES (ZDA_O);
  // prepare temp registers & ZCL signal
  tmpH = ZDI_O;
  BITRES (ZCL_O);
  tmpL = ZDI_O;
  // send data bits
  asm volatile (
      SEND_BIT (DATA, 7)
      SEND_BIT (DATA, 6)
      SEND_BIT (DATA, 5)
      SEND_BIT (DATA, 4)
      SEND_BIT (DATA, 3)
      SEND_BIT (DATA, 2)
      SEND_BIT (DATA, 1)
      SEND_BIT (DATA, 0)
   : : [OUT] "i" (_SFR_IO_ADDR (ZDI_O)), [DTA_BIT] "i" (ZDA_BIT), [DATA] "r" (addr), [REG_L] "r" (tmpL), [REG_H] "r" (tmpH)
  );
  // bit separator
  BITSET (ZDA_O);
  BITSET (ZCL_O);
  BITRES (ZCL_O);
  // send data bits
  asm volatile (
      SEND_BIT (DATA, 7)
      SEND_BIT (DATA, 6)
      SEND_BIT (DATA, 5)
      SEND_BIT (DATA, 4)
      SEND_BIT (DATA, 3)
      SEND_BIT (DATA, 2)
      SEND_BIT (DATA, 1)
      SEND_BIT (DATA, 0)
   : : [OUT] "i" (_SFR_IO_ADDR (ZDI_O)), [DTA_BIT] "i" (ZDA_BIT), [DATA] "r" (data), [REG_L] "r" (tmpL), [REG_H] "r" (tmpH)
  );
  // bit separator
  BITSET (ZDA_O);
  BITSET (ZCL_O);
}
#endif

/**
// ====================================
// read CPU register
// ------------------------------------
data_type zdi_read_reg (unsigned char reg) {
  zdi_write_byte (ZDI_RW_CTL, reg);
  return zdi_read_val ();
}

// ====================================
// write CPU register
// ------------------------------------
void zdi_write_reg (unsigned char reg, data_type val) {
  zdi_write_val (val);
  zdi_write_byte (ZDI_RW_CTL, reg);
}
**/

// ====================================
// ZDI communication wrapper
// ====================================
static unsigned char SAVE_mask = 0;
static data_type SAVE_reg[8]; // 13 with second register set and second SP*...
static unsigned char ZDI_mask = 0;
#define ZDI_STOP        0x01
#define ZDI_EXX         0x02
// ====================================
// open ZDI communication
// - stop CPU
// - save requested registers
// ------------------------------------
void zdi_open (unsigned char mask) {
unsigned char tmp;
  ZDI_mask = 0;
  // get CPU status
  tmp = zdi_read_byte (ZDI_STAT);
  if (!(tmp & ZDI_ACTIVE)) {
    zdi_write_byte (ZDI_BRK_CTRL, ZDI_BRK_NEXT);
    ZDI_mask |= ZDI_STOP;
  }
  // save registers
  mask &= SAVE_MASK;
  SAVE_mask = mask;
  zdi_reg (mask, SAVE_reg);
}

// ====================================
// exchange register set
// ------------------------------------
void zdi_exx (void) {
  zdi_write_byte (ZDI_RW_CTL, REG_EX);
  ZDI_mask ^= ZDI_EXX;
}

// ====================================
// set PC register
// ------------------------------------
void zdi_pc (data_type val) {
  if (!(SAVE_mask & SAVE_PC)) {
    zdi_write_byte (ZDI_RW_CTL, REG_RD_PC);
    SAVE_reg[ZDI_REG_PC] = zdi_read_val ();
    SAVE_mask |= SAVE_PC;
  }
  zdi_write_val (val);
  zdi_write_byte (ZDI_RW_CTL, REG_WR_PC);
}

// ====================================
// save requested registers
// ------------------------------------
void zdi_reg (unsigned char mask, data_type *buf) {
unsigned char reg = 0;
  while (mask) {
    if ((mask & 1)) {
      // read one register
      zdi_write_byte (ZDI_RW_CTL, reg);
      buf[reg] = zdi_read_val ();
    }
    mask >>= 1;
    //buf++;
    reg++;
  }
}

// ====================================
// close ZDI communication
// - return register bank
// - restore selected registers
// - possibly run CPU again
// ------------------------------------
void zdi_close (void) {
  // restore CPU status
  if (ZDI_mask & ZDI_EXX)
    zdi_write_byte (ZDI_RW_CTL, REG_EX);
  // restore registers
  if (SAVE_mask & SAVE_MASK) {
    unsigned char mask = SAVE_mask & SAVE_MASK, reg = 0;
    while (mask) {
      if (mask & 1) {
	// write one register
	zdi_write_val (SAVE_reg[reg]);
	zdi_write_byte (ZDI_RW_CTL, reg | ZDI_REG_WR);
      }
      mask >>= 1;
      reg++;
    }
  }
  if (ZDI_mask & ZDI_STOP)
    zdi_write_byte (ZDI_BRK_CTRL, 0);
}
