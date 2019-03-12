// ============================================================================
// ZDI (ZiLOG Debug Interface) driver
// (C) 2013 Hynek Sladky
// ATmega8 @ intRC 8MHz
// ============================================================================

#include "global.h"
#include <avr/pgmspace.h>
//#include <avr/interrupt.h>
#include "zdi.h"
#include "disasm.h"
#include "eZ80.h"

// ====================================
// compile control
// ------------------------------------

// ------------------------------------

/**
test

Bugs:
=====
- proc se meni hodnota ZDI_RD_DATA_* ??
  zadne vysvetleni jsem nenasel...
  -> asi se tam ukladaji vysledky cteni/zapisu na sbernici...?
- ZDIR 20 vzdy provede dve cteni !!!
  -> asi to je vlastnost... dokoncene cteni spusti novy cteci cyklus... asi i proto jsou dalsi data ulozena v registrech ZDI_RD_L a ZDI_RD_H
  ani na osciloskopu nevidim nec, co bych mel zmenit
  mozna bych musel dlouho laborovat, nez bych na neco prisel...
- provadeni prikazu take funguje nejak divne
  >>INC B
  BC = 000000
  EXEC 04
  PC = 00008C, BC = 000000 --> inkrement se neprovedl; mozna se provedla jeste predchozi instrukce?
  EXEC 04
  PC = 00008E, BC = 000100 --> inkrement se provedl az pri druhem zadani instrukce
  EXEC 0C
  PC = 000090, BC = 000200 --> provedla se jeste jednou puvodni instrukce
  EXEC 0C
  PC = 000090, BC = 000201 --> teprve zde se vykonala nova instrukce...
  --> proc to ten procesor, resp. ZDI interface dela???
  original to posila jednou a ono to hned napoprve zafunguje:
    W16:08 = set ADL
    W16:07 = read PC
    R10:50 R11:05 R12:00
    W16:00 = read AF
    R10:00 R11:42 R12:00
    W24:00 W25:3E = LD A,0
    W23:A3 W24:39 W25:ED = OUT0 (A3),A
    W13:57 W14:05 W15:00
    W16:87 = write PC
    W13:00 W14:42 W15:00
    .. asi W16:80 = write AF
  -> zapisuje se po bytu, nezapisuje se blokove... treba by pomohlo aspon posledni byte zapsat samostatne... ??
  -> nebo za to muze moc dlouha doba bitoveho oddelovace a CLK=H ??
     tj. musela by se udelat funkce, ktera bude zapisovat data z bufferu a bude mit tento cas minimalizovany... ??
  kazdopadne nefunguje to ani na 1-bytove instrukce, takze chybu musim hledat v samotnem zapisu...
  tj. ani EXEC 3, ani zdiw 25 3 neudela poprve to, co cekam...
  -> tedy zkusit mit buffer (globalni) pro zapis/cteni
  zdi_write (len) zapise len bytu (nebo len+1 ??)
  zdi_read (len) zapise prvni byte a precte len bytu...
- ani zrychleni zapisu zdi_write_byte nepomohlo
  jedine reseni je v provedeni dummy instrukce NOP po kazde sekvenci instrukci...
  tj. pak uz opravdu bude potreba ulozit PC registr...

- pri pokusu o vycteni a kontrolu pameti to obcas vypise podivny radek - jako by tam chybel kus seriove komunikace...
  memr 0 ffff 40
  vypada to, jako by se buffer v CDC portu nestihal po USB posilat do PC...


ToDo:
=====
- na osciloskopu si zmerit, jak dlouho trva zapis jednoho byte po ZDI
  kvuli casovani zapisu do RAM a hlavne Flash
  zdi_write_val (...) zapisuje byty po 8us
  win: 6.8us ZCL/byte; 8.32us do druheho bytu na ZCL (komunikace po prikazu RST)
  2 byty @57600Bd se prenaseji 347us

- zapis do registru muze byt najednou: W13-14-15-16
- pro krokovani:
  nacteni vsech registru (vcetne statusu: ADL, IFF)
  zobrazeni jen toho, co se zmenilo...
  o tj. funkce pro nacteni jednoho/vice registru
    hodila by se maska, co se ma nacist/obnovit -> da se pak pouzit i v jinych funkcich
  i kdyz tady musi byt ulozene registry vsechny a kontrolovat pri dalsim cteni, co se zmenilo...
  dale zkusit "nacpat" disassembler, aby bylo videt, jaka instrukce se ma dale provadet
- asi by to samo melo zjistovat, zda se procesor zastavil
  pak by se melo vypsat "break at ..."
- tedy pak bude potreba take umet nastavit breakpoint


Note:
=====
  cteni fuse: -q
  zapis fuse: -f <value-16bit>
    funkce jsou aktivni pri naprogramovani bitu do L
    high: RSTDISBL | WDTON | SPIEN | CKOPT | EESAVE | BOOTSZ1 | BOOTSZ0 | BOOTRST
    low: BODLEVEL | BODEN | SUT1 | SUT0 | CKSEL3 | CKSEL2 | CKSEL1 | CKSEL0
  cteni prot: -y
    - | - | BLB12 | BLB11 | BLB02 | BLB01 | LB2 | LB1
  zapis ext.fuse: -E

p:>stk500.exe -ccom6 -datmega8 -s -q -t -J -y
STK500 command line programmer, v 2.2 Atmel Corp (C) 2004-2005.

Connected to STK500 V2 on port com6
Getting oscillator frequency: 3.686 MHz (P=0x01, N=0x00)
Getting ISP frequency: 230.4 kHz (0x01)
Device parameters loaded
Programming mode entered
Signature is 0x1E 0x93 0x07
Reading fuse bits...
Fuse byte 0 read (0xE1)
Fuse byte 1 read (0xD9)
Reading lock bits... Lock bits read (0xFF)
Programming mode left
Connection to STK500 V2 closed

p:>stk500.exe -ccom6 -datmega328p -s -q -t -J -y
STK500 command line programmer, v 2.2 Atmel Corp (C) 2004-2005.

Connected to STK500 V2 on port com6
Getting oscillator frequency: 3.686 MHz (P=0x01, N=0x00)
Getting ISP frequency: 921.6 kHz (0x00)
Device parameters loaded
Programming mode entered
Signature is 0x1E 0x95 0x14
Reading fuse bits...
Fuse byte 0 read (0xE2)
Fuse byte 1 read (0xD9)
Fuse byte 2 read (0xFF)
Reading lock bits... Lock bits read (0xFF)
Programming mode left
Connection to STK500 V2 closed

- mapovani pameti dle priority
		default-to	debug-to	release-to	notes
  128KB int ROM	000000	01FFFF			000000	01FFFF	(registr FLASH_ADDR_U @ 00F7)
    4KB int RAM	FFE000	FFFFFF	B7E000	B7FFFF	B7E000	B7FFFF	(registr RAM_ADDR_U @ 00B5)
  CS0		000000	FFFFFF
  128KB ext RAM 				060000	07FFFF	(CS1; ve skutecnosti je osazeno 512KB)
- nastaveni CSn
  CS0 = ROMCS - vede pouze na konektor sbernice
  CS1 = RAMCS - vede na konektor sbernice i na pamet na desce
  CS2 = CS2 - vede pouze na konektor sbernice
  CS3 = IOCS - vede na GAL dekoder I/O sbernice
  intIO = 0080-00FF
  extIO = 0000-FFFF

- vytazeni informaci z projektu *.ztgt
		RAM	Flash
  PC		0	0
  SPL		B80000	B80000
  SPS		FFFF	FFFF
  extFlash	false	false
  intFlash	false	true
  CS0.mode	1	1
  CS0.ctl	20	20
  CS0.lower	6	6
  CS0.upper	7	7
  CS1.mode	1	1
  CS1.ctl	28	28
  CS1.lower	0	6	<-- RAM je bud cela nebo jen 6-7
  CS1.upper	7	7
  CS2.mode	2	2
  CS2.ctl	0	0
  CS2.lower	ff	ff
  CS2.upper	ff	ff
  CS3.mode	42	42
  CS3.ctl	18	18
  CS3.lower	10	10
  CS3.upper	10	10
  flash.page	0	0
  ram.page	b7	b7
  flash_ram?	false	true
  ram		true	true
  wait		1	1

- poznamky k mapovani/WS apod...
  CSx_LBR,_UBR,_CTL (00:FF:E8 00:00:00 00:00:00 00:00:00)
  CSx_BMC (02 02 02 02)
  RAM_CTL,_ADDR_U (80:FF)
  FLASH_ADDR_U,_CTRL (00:88) -> disable: (00:80)
  4 WS; min 60ns; 18.432MHz ~ 55ns
  zapisovy cyklus ale trva jen 1/2 periody! dal by se to melo prodluzovat po celem taktu...?
  IMHO by to slo nastavit na 1WS ??
  nejvyssi prioritu ma interni RAM, pak interni Flash a pak teprve CS0, CS1, CS2 a posledni CS3
  CS0 je defaultne cela pamet
  pri behu je 1WS pro flash (FLASH_CTRL=0x28)

- casovani pameti
  cycle @ 18.432MHz = 54.25ns
  extRAM:
    pamet ma zrejme pristupovou dobu 55ns (Samsung K6X4008CIF-BF55)
    doba vybaveni na zmenu adresy Taa = 55ns; na signal /CS Tcs = 55ns, na signal /OE Toe = 25ns
    doba zapisoveho pulsu na /CS Tcw = 45ns; na signalu /WE Twp = 40ns
    tedy 1ws tam musi byt, protoze 0ws by potrebovala 54ns - (2..19)ns - 1ns = (34..51)ns
  intFlash:
    v DS je uvadena hodnota 60ns
    defaultni hodnota je 4ws
    tj. by pro flash take mel byt 1ws...
- casovani zapisu do Flash
  jeden preneseny byte = 2 znaky z terminalu = 350us (tj. 54..69 taktu flash)
  13 taktu flash ~ (66..85)us ==> pri maximalni frekvenci to je (13..16.66) taktu
    tj. pokud budu pocitat presne 17 taktu, tak by se mela vejit nejpomalejsi i nejrychlejsi frekvence...
  cas pro 128 bytu: 2432 flash controller clocks (12.4-15.8)ms, tj. 19 taktu/byte (97..123)us
  - deleni Flash
    128 bytes (columns) per row
    8 rows per page (1K)
    16 pages per block (16K)
    8/16 blocks per Flash (64K/128K)
  upozorneni pri programovani Flash pro zachovani integrity dat:
  - kumulativni programovaci cas od posledniho Erase nesmi prekrocit 16ms pro kazdy radek
  - byte ve Flash se nesmi programovat vic nez 2x od posledniho mazani
  mazani:
    mass erase: 200ms
    page erase: 10ms

**/

// ====================================
// microsecond function (max 255us)
// ------------------------------------
#if 0
static void delay_us (unsigned char time) {
unsigned char start;
  start = TCNT0;
  // bez pretypovani to pouzije int!!! -> pomale, neefektivni a nefunkcni...
  //while ((TCNT0 - start) < time);
  while ((unsigned char)(TCNT0 - start) < time);
}
#endif
// ====================================
// millisecond function (max 8191ms)
// ------------------------------------
static void delay_ms (unsigned short ms) {
unsigned char start;
  ms <<= 3;
  start = TCNT0;
  while (ms--) {
    while ((unsigned char)(TCNT0 - start) < 125);
    start += 125;
  }
}

// ====================================
// UART
// ====================================
// character output
// ------------------------------------
/*static*/ void dputc (char c) {
  while (!BIT (UCSRA, UDRE));
  UDR = c;
}

// ====================================
// string output
// ------------------------------------
/*static*/ void dputs (const char *str) {
char c;
  while ((c=pgm_read_byte (str++))) {
    dputc (c);
  }
}

// ====================================
// value output
// ------------------------------------
static const char hex[16] PROGMEM = "0123456789ABCDEF";
// ------------------------------------
/*static*/ void dprint (const char *str, unsigned long val) {
unsigned char c, fill = ' ', min = 0;
static char buf[10];
  while ((c = pgm_read_byte (str++))) {
    if (c == '%') {
      if (!(c = pgm_read_byte (str++)))
        break;
      if (c == '0') {
        fill = '0';
        if (!(c = pgm_read_byte (str++)))
          break;
      }
      if (c > '0' && c <= '9') {
        min = c - '0';
        if (!(c = pgm_read_byte (str++)))
          break;
      }
      if (c == 'd') {
        c = 0;
        do {
          buf[c++] = (val % 10) + '0';
          val /= 10;
        } while (val && c < 10);
        goto print_val;
      } else
      if (c == 'X') {
        c = 0;
        do {
          buf[c++]=pgm_read_byte (hex + (val & 0x0F));
          val >>= 4;
        } while (val && c < 8);
  print_val:
        while (c < min)
          buf[c++] = fill;
        do {
          dputc (buf[--c]);
        } while (c);
        continue;
      }
    }
    dputc (c);
  }
}
#if 0
static void show_val (const char *str, data_type val) {
  if (str) {
    dputs (str);
    dputc ('=');
  }
  dputc (pgm_read_byte (hex + ((val.U >> 4) & 0x0F)));
  dputc (pgm_read_byte (hex + (val.U & 0x0F)));
  dputc (pgm_read_byte (hex + ((val.H >> 4) & 0x0F)));
  dputc (pgm_read_byte (hex + (val.H & 0x0F)));
  dputc (pgm_read_byte (hex + ((val.L >> 4) & 0x0F)));
  dputc (pgm_read_byte (hex + (val.L & 0x0F)));
  dputc ('\n');
}
#endif
// ====================================
// character input
// ------------------------------------
static char dgetc (void) {
  while (!BIT (UCSRA, RXC));
  return UDR;
}

// ====================================
// input test
// ------------------------------------
static char dkbhit (void) {
  return BIT (UCSRA, RXC);
}

// ====================================
// HW initialization
// ====================================
static void HW_init (void) {
  // enable puulups
//  SFIOR = 0b00000100;
  // I/O setup
  PORTB = PB_OUT;
  DDRB  = PB_DIR;
  PORTC = PC_OUT;
  DDRC  = PC_DIR;
  PORTD = PD_OUT;
  DDRD  = PD_DIR;
  // UART (pro 8MHz by slo celkem presne 38400Bd; 57600Bd by asi fungoval s odrenyma usima...)
  //UBRRL = 52-1; // 8MHz/8/19200-1
  UBRRL = 17-1; // 8MHz/8/19200-1 = 17.36111
  UCSRA = 0b00000010; // div8
  UCSRB = 0b00011000; // RXenable, TX enable
  UCSRC = 0b10000110; // 8-bit
  // TIMER0
  TCCR0 = 0b00000010; // 8MHz/8
  // TIMER1 + capture
  TCCR1A = 0b00000000;
  TCCR1B = 0b10000010; // noise canceler, falling edge, 1MHz clock
/**
  // TIMER1 + PWM
  TCCR1A = 0b11110001; // PWM mode
  TCCR1B = 0b00001001; // CLK/2 -> 15.625kHz
  OCR1A = 0;
  OCR1B = 0xFF;
  // ADC
  ADMUX = 0b11000000; // Vref=int, 
  ADCSRA = 0b10000111; // CK/128 -> 8MHz/128 = 62.5kHz
  // interrupts
  TIMSK = 0b00000001;
  sei ();
**/
  delay_ms (500);
}

// ====================================
// version info
// ====================================
extern const char build[] PROGMEM;
extern const char build_date[] PROGMEM;
extern const char build_time[] PROGMEM;
// ------------------------------------
static void print_ver (void) {
data_type val;
//  dputs (PSTR ("ZDI driver v"));
  dputs (PSTR ("ZDI v"));
  dputs (build);
  dputs (PSTR ("\nbuilt "));
  dputs (build_date);
  dputc (',');
  dputs (build_time);
  dputc ('\n');
// 00:08 = eZ80F91
// 00:07 = eZ80F92/93
  val.L = zdi_read_byte (ZDI_ID_L);
  val.H = zdi_read ();
  val.U = zdi_read ();
  if (val.offs == 0xFFFF && val.U == 0xFF) {
    dputs (PSTR ("No CPU found!\n"));
  } else {
    dputs (PSTR ("Found: "));
    if (val.offs == 0x0005)
      dputs (PSTR ("eZ80190")); // see PS0066.pdf
    else
    if (val.offs == 0x0006)
      dputs (PSTR ("eZ80L92")); // see PS0130.pdf
    else
    if (val.offs == 0x0007)
      dputs (PSTR ("eZ80F92/93")); // see PS0153.pdf
    else
    if (val.offs == 0x0008)
      dputs (PSTR ("eZ80F91")); // see PS0191.pdf
    else
      dprint (PSTR ("unknown ID (%04X)"), val.offs);
    dprint (PSTR (" rev %02X\n"), val.U);
  }
}

// ====================================
// buffer
// ====================================
static unsigned char buffer[256];
static unsigned char /*wr = 0,*/ rd = 0;

// ====================================
// commands
// ====================================
enum {
  CMD_HELP, CMD_VER, CMD_TEST,
  CMD_RESET,
  CMD_ZDI_RD, CMD_ZDI_WR,
  CMD_REG,
  CMD_MEM_RD, CMD_MEM_WR,
  CMD_INPUT, CMD_OUTPUT,
  CMD_STOP, CMD_RUN, CMD_STEP,
  CMD_EXEC,
  CMD_MAP,
  CMD_ADL, CMD_MADL, CMD_EXX,
  CMD_BREAK,
  CMD_MODE,
  CMD_DIS,
  CMD_STACK, CMD_SPS, CMD_SPL,
  CMD_COUNT,
  REG_FIRST,
  // see ZDI_REG_AF .. ZDI_REG_PC
  REG_AF = REG_FIRST,
  REG_BC,
  REG_DE,
  REG_HL,
  REG_IX,
  REG_IY,
  REG_SP,
  REG_PC,
  REG_LAST = REG_PC
};
static const char cmd_tab[] PROGMEM = 
  "HELP\0"
  "VER\0"
  "TEST\0"
  "RST\0"
  // low-level ZDI register access
  "ZDIR\0"
  "ZDIW\0"
  // high-level eZ80 access
  "REG\0"
  "MEMR\0"
  "MEMW\0"
  "IN\0"
  "OUT\0"
  "STOP\0"
  "RUN\0"
  "STEP\0"
  "EXEC\0"
  "MAP\0"
  "ADL\0"
  "MADL\0"
  "EXX\0"
  "BRK\0"
  "MODE\0"
  "DIS\0"
  "STACK\0"
  "SPS\0"
  "SPL\0"
;
#define	REG_COUNT	(REG_LAST - CMD_COUNT)
static const char reg_tab[] PROGMEM = 
  "AF\0"
  "BC\0"
  "DE\0"
  "HL\0"
  "IX\0"
  "IY\0"
  "SP\0"
  "PC\0"
  "SP\0";
// ------------------------------------
#define	CMD_INDEX_FLAG	0x01
// ------------------------------------
static unsigned char cmd_get (const char *tab, unsigned char idx) {
unsigned char i;
  while (buffer[idx] <= ' ') {
    if (idx == rd)
      return CMD_COUNT;
    idx++;
  }
  i = 0;
  for (i = 0; i < CMD_COUNT; i++) {
    unsigned char j, b, d;
    j = idx;
    do {
      b = (j != rd) ? buffer[j] : 0;
      d = pgm_read_byte (tab);
      // end of command; check whitespaces
      if (!d) {
	if (b >= '0' && b <= '9') {
	  // insert space to allow number processing
	  buffer[--j] = CMD_INDEX_FLAG;
	  b = 0;
	}
        if (j == rd || b <= ' ')
          return i;
        break;
      }
      j++;
      tab++;
      if (b >= 'a' && b <= 'z')
        b &= ~0x20;
    } while (b == d);
    do {
      d = pgm_read_byte (tab++);
    } while (d);
  }
  return CMD_COUNT;
}
// ------------------------------------
static unsigned char next_par (unsigned char idx) {
  if (buffer[idx] == CMD_INDEX_FLAG && idx != rd)
    idx++;
  while (buffer[idx] > ' ') {
    if (idx == rd)
      return idx;
    idx++;
  }
  while (buffer[idx] <= ' ') {
    if (buffer[idx] == CMD_INDEX_FLAG || idx == rd)
      return idx;
    idx++;
  }
  return idx;
}
// ------------------------------------
static unsigned long cmd_val (unsigned char idx) {
unsigned long val = 0;
unsigned char c;
  while (idx != rd) {
    c = buffer[idx++];
    if (c == CMD_INDEX_FLAG)
      continue;
    if (c < '0')
      break;
    if (c >= 'a' && c <= 'z')
      c &= ~0x20;
    if (c > 'F')
      break;
    if (c > '9') { 
      if (c < 'A')
        break;
      c -= 'A' - 10;
    } else
      c -= '0';
    val <<= 4;
    val |= c;
  }
  return val;
}
// load 2-character hex value
static unsigned char hex_val (unsigned char idx) {
unsigned char val = 0;
  if (idx != rd) {
    unsigned char c;
    c = buffer[idx++];
    if (c >= '0' && (c <= '9' || c >= 'A') && (c & ~0x20) <= 'F') {
      //val <<= 4;
      c |= 0x20;
      c -= '0';
      if (c > 9)
	c -= 'a' - '0' - 10;
      val |= c;
    }
  }
  if (idx != rd) {
    unsigned char c;
    c = buffer[idx];
    if (c >= '0' && (c <= '9' || c >= 'A') && (c & ~0x20) <= 'F') {
      val <<= 4;
      c |= 0x20;
      c -= '0';
      if (c > 9)
	c -= 'a' - '0' - 10;
      val |= c;
    }
  }
  return val;
}

// ====================================
// register change detection
// ====================================
typedef struct {
  data_type regs[13];
  unsigned char state;
} CPU_type;
static CPU_type cpu_old, cpu_new;
// ------------------------------------
static void regs_init (CPU_type *ptr) {
unsigned char tmp;
  if (!ptr)
    ptr = &cpu_old;
  // save ADL, MADL, IFF
  ptr->state = tmp = zdi_read_byte (ZDI_STAT);
  zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL /*| SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC*/, ptr->regs);
  zdi_exx ();
  //zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL, &ptr->regs[8]);
  zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL | SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC, &ptr->regs[4]);
  zdi_exx ();
  // switch ADL mode
  //zdi_adl ((tmp ^ ZDI_ADL) & ZDI_ADL);
  zdi_write_byte (ZDI_RW_CTL, (tmp & ZDI_ADL) ? ADL_CLR : ADL_SET);
  // load other SP* register
  zdi_reg (SAVE_SP, &ptr->regs[6]);
  if (!(tmp & ZDI_ADL))
    // re-load AF register to get MBASE
    zdi_reg (SAVE_AF, ptr->regs);
  // restore ADL mode
  zdi_write_byte (ZDI_RW_CTL, (tmp & ZDI_ADL) ? ADL_SET : ADL_CLR);
  //zdi_adl (tmp & ZDI_ADL);
}

static void reg_print (unsigned char idx) {
  dputs (reg_tab + (((idx < 4) ? idx : (idx - 4)) * 3));
  if ((idx >= 4 && idx < 8) || idx > 11)
    dputc ('\'');
  else
    dputc (' ');
  dprint (PSTR ("=%06X"), cpu_old.regs[idx].val);
}

static void regs_changed (void) {
unsigned char idx, cnt = 0;
  regs_init (&cpu_new);
  // PC shouldn't be printed...
  cpu_old.regs[11].val = cpu_new.regs[11].val;
  // fill MBASE register to PC address
  if (!(cpu_new.state & ZDI_ADL))
    cpu_old.regs[11].U = cpu_new.regs[11].U = cpu_new.regs[0].U;
  // print all changed registers
  for (idx = 0; idx < 13; idx++) {
    if (cpu_new.regs[idx].val == cpu_old.regs[idx].val)
      continue;
    cpu_old.regs[idx].val = cpu_new.regs[idx].val;
    if (cnt >= 4) {
      cnt = 1;
      dputc ('\n');
    } else
    if (cnt++) {
      dputc ('\t');
    }
    reg_print (idx);
  }
  // compare other SP* ??
  // compare ADL, IFF bits
  //...
  if (cnt)
    dputc ('\n');
}

// ====================================
// print single step instruction
// ====================================
static unsigned char instr[6];
// ------------------------------------
static void show_step (void) {
  // read all registers
  // compare saved values
  // print only changed registers
  zdi_open (SAVE_PC);
  regs_changed ();
  // read next instruction
  instr[0] = zdi_read_byte (ZDI_RD_MEM);
  instr[1] = zdi_read ();
  instr[2] = zdi_read ();
  instr[3] = zdi_read ();
  instr[4] = zdi_read ();
  instr[5] = zdi_read ();
  zdi_close ();
  dprint (PSTR("%06X:\t"), cpu_new.regs[11].val);
  // disassemble
  z80_disasm (instr, cpu_new.regs[11].val, zdi_read_byte (ZDI_STAT) & ZDI_ADL);
  dputc ('\n');
}
// ------------------------------------
static unsigned char disasm (unsigned long addr, unsigned char flags) {
unsigned char len;
  zdi_open (SAVE_PC);
  zdi_pc ((data_type)addr);
  // read next instruction
  instr[0] = zdi_read_byte (ZDI_RD_MEM);
  instr[1] = zdi_read ();
  instr[2] = zdi_read ();
  instr[3] = zdi_read ();
  instr[4] = zdi_read ();
  instr[5] = zdi_read ();
  zdi_close ();
  dprint (PSTR("%06X:\t"), addr);
  // disassemble
  len = z80_disasm (instr, addr, flags);
  dputc ('\n');
  return len;
} 

// ====================================
// opcode execution
// ====================================
static void exec (unsigned char *op, unsigned char len) {
unsigned char tmp;
  // check if in STOP mode
  tmp = zdi_read_byte (ZDI_STAT);
  // execute instruction
  zdi_addr (ZDI_IS0 + 2 - (len << 1));
  op += len;
  while (len--) {
    zdi_write (*--op);
  }
  // dummy NOP
  zdi_write_byte (ZDI_IS0, 0x00);
  // e.g. erasing Flash (out FF 1) will hold CPU running (active WAIT signal) until erase finished
  // so wait for CPU being back in stop mode
  if (tmp & ZDI_ACTIVE) {
    while (!(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE));
  }
}

// ====================================
// peripheral register input
// ====================================
static unsigned char input (unsigned short port) {
  zdi_open (SAVE_PC | SAVE_BC);
  // set BC register
  zdi_write_val ((data_type)(((unsigned long)port)));
  zdi_write_byte (ZDI_RW_CTL, REG_WR_BC);
  // execute IN C,(C)
  instr[0] = 0xED;
  instr[1] = 0x48;
  exec (instr, 2);
  // read BC register
  zdi_write_byte (ZDI_RW_CTL, REG_RD_BC);
  port = zdi_read_val ().val;
  zdi_close ();
  return port;
}

// ====================================
// delay with 1 cycle resolution
// lowest possible value is 4 !!
// lower values lead to underflow like value 256
// ====================================
// calling conventions: https://gcc.gnu.org/wiki/avr-gcc
// __tmp_reg__(r0),T,r18-r27,r30-r31 are call clobbered
// r1 is always zero
// r2-r17 are call-saved
// parameters are in registers r25..r8, start always with even number
// ex: fnc (char a, char b) : r24 = a, r22 = b
// inline assembler: http://www.nongnu.org/avr-libc/user-manual/inline_asm.html
static void __attribute__ ((noinline)) delay_nop (unsigned char delay) {
  asm volatile (
      "mov r31,%[CNT]\n"
      "andi r31,3\n"
      "ldi r30,pm_lo8(2f)\n"
      "sub r30,r31\n"
      "ldi r31,pm_hi8(2f)\n"
      "sbc r31,__zero_reg__\n"
      "lsr %[CNT]\n"
      "lsr %[CNT]\n"
      "ijmp\n"
      "nop\n"
      "nop\n"
    "1:\n"
      "nop\n"
    "2:\n"
      "dec %[CNT]\n"
      "brne 1b\n"
      : : [CNT] "r" (delay) : "r30","r31");
}

// ====================================
// CPU reset
// ====================================
static void reset_cpu (unsigned char delay) {
  // activate reset signal
  BITRES (RST_O);
  BITSET (RST_D);
  // short wait loop
  delay_nop (255);
  BITSET (RST_O);
  if (delay) {
    // short adjustable wait loop
    delay_nop (delay);
    // stop CPU
    zdi_write_byte (ZDI_BRK_CTRL, ZDI_BRK_NEXT);
    delay_nop (255);
  }
  BITRES (RST_D);
}

// ====================================
// main
// ====================================
int main (void) {
unsigned char b, cmd, *ptr;
static unsigned char CPU_state = 0;
static data_type BRK_addr[4];
static unsigned char BRK_mask = 0;
static unsigned char RST_delay = 0;
static unsigned char HEX_mode = 0;

  // initialize hardware
  HW_init ();
#ifndef _AVR_IOM328P_H_
  //OSCCAL = 0xA5; // factory default for 8MHz @ 5V
  OSCCAL = 0xB3; // 8MHz @ 3.3V
#else
  if (OSCCAL == 0x99) {
  //OSCCAL = 0x99; // factory default; probably can work for 8MHz @ 5V
    OSCCAL = 0x91; // 8MHz @ 3.3V
  } else
  if (OSCCAL == 0xAE) {
  //OSCCAL = 0xAE; // factory default; probably can work for 8MHz @ 5V
    OSCCAL = 0xAA; // 8MHz @ 3.3V
  } else {
    // average adjust
    OSCCAL -= 7;
  }
#endif
  delay_ms (10);
  print_ver ();
  dprint (PSTR ("OSCCAL=%X\n"), OSCCAL);

  zdi_init ();

  // command interpreter
  cmd = 0;
  dputc ('>');
  while (1) {
    // BOOT pin check
    if (BIT (BOOTin)) {
      // disable ZDI UART
      UCSRB = 0b00000000;
      // application control/terminal/access
      do {
	// RST pin check
	if (!BIT (RSTin)) {
	  // activate reset
	  BITRES (RST_O);
	  BITSET (RST_D);
	  while (!BIT (RSTin) && BIT (BOOTin));
	  // deactivate reset
	  BITRES (RST_D);
	}
	// copy RX/TX signals
	asm volatile (
	  "1:\n"
	    "in __tmp_reg__,%[INP]\n"
	    "bst __tmp_reg__,%[RXAPP]\n"
	    "bld %[VAL],%[TXBIT]\n"
	    "bst __tmp_reg__,%[RXBIT]\n"
	    "bld %[VAL],%[TXAPP]\n"
	    "out %[OUT],%[VAL]\n"
	    "sbrs __tmp_reg__,%[RST]\n"
	    "RJMP 2f\n"
	    "sbrc __tmp_reg__,%[BOOT]\n"
	    "RJMP 1b\n"
	  "2:\n"
	  : : [VAL] "r" (UART_OUT), [INP] "i" (_SFR_IO_ADDR (UART_INP)), [OUT] "i" (_SFR_IO_ADDR (UART_OUT)), 
	      [RXAPP] "i" (APP_RXD), [TXAPP] "i" (APP_TXD), [RXBIT] "i" (UART_RXD), [TXBIT] "i" (UART_TXD), [BOOT] "i" (UART_BOOT), [RST] "i" (UART_RST)
	);
      } while (BIT (BOOTin));
      // restore signals before return to ZDI
      BITSET (TXD);
      BITSET (RXAout);
      // enable ZDI UART
      UCSRB = 0b00011000;
      // reset command buffer
      cmd = rd;
    }
    if (TIFR & (1 << TOV0)) {
      static unsigned short cnt = 0;
      TIFR = (1 << TOV0);
      if (!cnt) {
	cnt = 100000 / 256;
	if (cmd != rd && buffer[cmd] == ':')
	  continue;
	b = zdi_read_byte (ZDI_STAT);
	if ((b & ZDI_ACTIVE)) {
	  if (!(CPU_state & ZDI_ACTIVE) && cmd == rd) {
	    CPU_state |= ZDI_ACTIVE;
	    //dputc ('\r');
	    //regs_changed ();
	    //dprint (PSTR ("break at %06X\n"), regs_new[11].val);
	    dputs (PSTR ("\rBreak!\n"));
	    show_step ();
	    dputc ('>');
	  }
	} else {
	  CPU_state &= ~ZDI_ACTIVE;
	}
      } else {
	cnt--;
      }
    }
    // check if new data available
    if (!dkbhit ())
      continue;
    // read & process new data
    b = dgetc ();
    if (b == '\b') {
      // backspace
      if (rd == cmd)
	continue;
      rd--;
      dputc (b);
      dputc (' ');
      dputc (b);
      continue;
    }
    if (b != '\r' && b != '\n' && b != 0x03) {
      if (b < ' ')
	// unsupported control char
	continue;
      // store char to buffer
      buffer[rd++] = b;
      // echo
      if (buffer[cmd] != ':') {
        dputc (b);
      } else {
	// process Intel-HEX immediately
	static unsigned char chk, len, type, reply;
	static data_type addr = {.val = 0};
	unsigned char tmp;
	b = rd - cmd - 1;
	if (b & 1)
	  continue;
	if (!b) {
	  chk = 0;
	  reply = 0;
	  continue;
	}
	// load next byte value
	tmp = hex_val (cmd + b - 1);
	// update checksum
	chk += tmp;
	if (b < 10) {
	  // process record header
	  if (b == 2)
	    len = tmp;
	  else
	  if (b == 4)
	    addr.H = tmp;
	  else
	  if (b == 6)
	    addr.L = tmp;
	  else
	  if (b == 8)
	    type = tmp;
	  continue;
	}
	if (!len) {
	  // process checksum byte
	  if (!reply) {
	    if (chk)
	      dputc ('c');
	    else
	      dputc ('*');
	    reply = 1;
	  }
	  // check for format error
	  if (reply < 3) {
	    reply++;
	    if (reply == 3)
	      dputc ('f');
	  }
	  continue;
	}
	len--;
	// process data bytes
	if (type == 0x00) {
	  // data record
	  if (b == 10) {
	    // set PC address
	    zdi_write_val (addr);
	    zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC | ZDI_REG_WR);
	    if (HEX_mode) {
	      // start memory writing
	      zdi_addr (ZDI_WR_MEM);
	    } else {
	      zdi_addr (ZDI_RD_MEM);
	    }
	  }
	  if (HEX_mode) {
	    // write data
	    zdi_write (tmp);
	  } else {
	    // verify data
	    if (zdi_read () != tmp && !reply) {
	      dputc ('v');
	      reply = 1;
	    }
	  }
	  addr.val++;
	} else
	if (type == 0x01) {
	  // end-of-file record
	  addr.page = 0;
	} else
	if (type == 0x04) {
	  // extended linear address
	  addr.page <<= 8;
	  addr.U = tmp;
	}
      }
      continue;
    }

    // CR or LF is last received byte
    if (buffer[cmd] == ':') {
#if 0
      if (len) {
	dputc ('f');
	reply = 1;
      }
#endif
      // release buffer
      cmd = rd;
      continue;
    }

    // look for command
    if (cmd == rd) {
      // repeat last command?
      // or simple: just force step command
      if (b == 0x03)
	b = CMD_STOP;
      else {
	b = CMD_STEP;
      }
      dputc ('\r');
    } else {
      b = cmd_get (cmd_tab, cmd);
      if (b < CMD_COUNT)
	cmd = next_par (cmd);
      else
	b = REG_FIRST + cmd_get (reg_tab, cmd);
      dputc ('\n');
    }
    switch (b) {
      // ------------------------------
      // common interface commands
      // ------------------------------
      case CMD_HELP:
        // print all available commands
        ptr = (void*)cmd_tab;
        for (cmd = 0; cmd < CMD_COUNT; cmd++) {
          do {
            b = pgm_read_byte (ptr++);
            if (cmd != CMD_HELP) {
              if (b)
                dputc (b);
              else
                dputc ('\n');
            }
          } while (b);
        }
        break;
      case CMD_VER:
        print_ver ();
	break;
      // ------------------------------
      // HW reset
      // ------------------------------
      case CMD_RESET:
        // do CPU reset
	dputs (PSTR ("CPU reset"));
	regs_init (0);
	if (cmd != rd || !(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE)) {
	  b = cmd_val (cmd);
	  // single reset pulse
	  reset_cpu (b);
dprint (PSTR (" {%d}\n"), b);
//	  dputc ('\n');
	  if (zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE)
	    show_step ();
	  break;
	}
	// automatic reset pulse adjust
	cmd = RST_delay;
	if (!cmd)
	  cmd = 4;
	b = 0;
	do {
	  data_type val;
	  // reset pulse
	  reset_cpu (cmd);
	  // check if reset was successful
	  zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC);
	  val = zdi_read_val ();
	  if (!val.val) {
	    // reset successful
	    if (cmd == RST_delay)
	      break;
	    b++;
	  } else
	  if (val.val == 0xFFFFFF) {
	    // CPU "frozen"
	    b = 0;
	  } else {
	    // CPU running or stopped not on reset address
	    if (b) {
	      // working reset range found
	      cmd -= (b >> 1) + 1;
	      dputs (PSTR (" adjusted."));
	      RST_delay = cmd;
	      reset_cpu (cmd);
	      break;
	    }
	    if (RST_delay && cmd > 0x20)
	      //cmd--;
	      // restart adjust
	      cmd = 0x20;
	    b = 0;
	  }
	  cmd++;
	} while (1);
	dprint (PSTR (" (%d)\n"), cmd);
	show_step ();
	break;
      // ------------------------------
      // low-level ZDI access
      // ------------------------------
      case CMD_ZDI_RD:
        // read ZDI registr(s)
        b = cmd_val (cmd);
        cmd = next_par (cmd);
        cmd = cmd_val (cmd);
        if (!cmd)
          cmd = 1;
        dprint (PSTR ("ZDI[%X]="), b);
        zdi_addr (ZDI_RD (b));
        do {
          dprint (PSTR ("%X "), zdi_read ());
        } while (--cmd);
        dputc ('\n');
        break;
      case CMD_ZDI_WR:
        // write ZDI register(s)
	b = cmd_val (cmd);
        cmd = next_par (cmd);
        zdi_addr (ZDI_WR (b));
        while (cmd != rd) {
          zdi_write (cmd_val (cmd));
          cmd = next_par (cmd);
        }
        break;
      // ------------------------------
      // memory access
      // ------------------------------
      case CMD_MEM_RD: // MEMR addr [cnt [line]]
	//if (cmd == rd)
	//  break;
	zdi_open (SAVE_PC);
	{
	  data_type val;
	  unsigned cnt = 1;
	  // load address
	  if (cmd != rd) {
	    val.val = cmd_val (cmd);
	    zdi_pc (val);
	    cmd = next_par (cmd);
	  } else {
	    zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC);
	    val = zdi_read_val ();
	  }
	  if (zdi_read_byte (ZDI_STAT) & ZDI_ADL)
	    dprint (PSTR (":02000004%04X00\n"), val.page);
	  // load count
	  if (cmd != rd) {
	    cnt = cmd_val (cmd);
	    cmd = next_par (cmd);
	  } else {
	    cnt = 0x20;
	  }
	  // load line length
	  if (cmd != rd) {
	    b = cmd_val (cmd);
	    //cmd = next_par (cmd);
	  } else {
	    b = 0x20;
	  }
	  cmd = 0;
	  zdi_addr (ZDI_RD_MEM);
	  while (cnt) {
	    if (!cmd) {
	      cmd = (cnt > b) ? b : (unsigned char)cnt;
	      dprint (PSTR (":%02X"), cmd);
	      dprint (PSTR ("%04X00"), val.offs);
	    }
	    dprint (PSTR ("%02X"), zdi_read ());
	    val.val++;
	    if (!--cmd)
	      dputs (PSTR ("00\n"));
	    --cnt;
	  }
	  // PC is always higher by +1 - probably new read cycle was triggered already
	  // ... so possibly correct PC by -1
	}
	zdi_close ();
	break;
      case CMD_MEM_WR:
	if (cmd == rd)
	  break;
	zdi_open (0);
	{
	  data_type val;
	  val.val = cmd_val (cmd);
	  zdi_pc (val);
	  cmd = next_par (cmd);
	}
	zdi_addr (ZDI_WR_MEM);
	while (cmd != rd) {
	  zdi_write (cmd_val (cmd));
	  cmd = next_par (cmd);
	}
	zdi_close ();
	break;
      // ------------------------------
      // I/O port access
      // ------------------------------
      case CMD_INPUT:
	if (cmd == rd)
	  break;
	{
	  unsigned short port = cmd_val (cmd);
	  dprint (PSTR ("IN[%04X]="), port);
	  dprint (PSTR ("%02X\n"), input (port));
	}
	break;
      case CMD_OUTPUT:
	if (cmd == rd)
	  break;
	zdi_open (SAVE_PC | SAVE_BC | SAVE_AF);
	{
	  data_type val;
	  // set BC register
	  val.val = cmd_val (cmd);
	  zdi_write_val (val);
	  zdi_write_byte (ZDI_RW_CTL, REG_WR_BC);
	  //dprint (PSTR ("IN[%04X]="), val.val);
	  cmd = next_par (cmd);
	  do {
	    // set A register
	    if (cmd != rd) {
	      val.val = cmd_val (cmd);
	      cmd = next_par (cmd);
	    } else {
	      val.val = 0;
	    }
	    zdi_write_val (val);
	    zdi_write_byte (ZDI_RW_CTL, REG_WR_AF);
	    //dprint (PSTR ("IN[%04X]="), val.val);
	    // execute OUT (C),A
	    instr[0] = 0xED;
	    instr[1] = 0x79;
	    exec (instr, 2);
	  } while (cmd != rd);
	}
	zdi_close ();
	break;
      // ------------------------------
      // debugging
      // ------------------------------
      case CMD_STOP:
	// stop CPU
	zdi_write_byte (ZDI_BRK_CTRL, ZDI_BRK_NEXT);
	break;
      case CMD_BREAK:
	// manage breakpoints
	if (cmd == rd) {
	  // print all breakpoints
	  if (!(BRK_mask & (ZDI_BRK_ADDR0 | ZDI_BRK_ADDR1 | ZDI_BRK_ADDR2 | ZDI_BRK_ADDR3))) {
	    dputs (PSTR ("No breakpoints.\n"));
	    break;
	  }
	  cmd = BRK_mask;
	  for (b = 0; b < 4; b++) {
	    if ((cmd & ZDI_BRK_ADDR0)) {
	      dprint (PSTR ("BRK%d="), b);
	      dprint (PSTR ("%06X"), BRK_addr[b].val);
	      if (cmd & ZDI_IGN_LOW_0)
		dputs (PSTR (".page"));
	      dputc ('\n');
	      cmd &= ~ZDI_BRK_ADDR0;
	    }
	    cmd >>= 1;
	  }
	  break;
	}
	// breakpoint number can be part of BRK command
	if (buffer[cmd] == CMD_INDEX_FLAG) {
	  // load breakpoint number
	  b = cmd_val (++cmd);
	  if (b > 3)
	    break;
	  cmd = next_par (cmd);
	  if (cmd == rd) {
	    // print one breakpoint
	    if (!(BRK_mask & (ZDI_BRK_ADDR0 << b))) {
	      dprint (PSTR ("BRK%d unused.\n"), b);
	    } else {
	      dprint (PSTR ("BRK%d="), b);
	      dprint (PSTR ("%06X"), BRK_addr[b].val);
	      if (b < 2 && (BRK_mask & (ZDI_IGN_LOW_0 << b)))
		dputs (PSTR (".page"));
	      dputc ('\n');
	    }
	    break;
	  }
	  if (buffer[cmd] == '-') {
	    // delete/disable breakpoint
	    BRK_mask &= ~(ZDI_BRK_ADDR0 << b);
	    dprint (PSTR ("BRK%d removed.\n"), b);
	  }
	} else {
	  // look for empty breakpoint
	  unsigned char tmp = BRK_mask;
	  for (b = 0; b < 4; b++) {
	    if (!(tmp & ZDI_BRK_ADDR0))
	      break;
	    tmp >>= 1;
	  }
	  if (b >= 4) {
	    // no free breakpoint
	    dputs (PSTR ("No free breakpoint!\n"));
	    break;
	  }
	  // free breakpoint found
	}
	if (buffer[cmd] != '-') {
	  // add breakpoint
	  BRK_addr[b].val = cmd_val (cmd);
	  BRK_mask |= (ZDI_BRK_ADDR0 << b);
	  // write break address
	  zdi_write_byte (ZDI_ADDR0L + b*8, BRK_addr[b].L);
	  zdi_write (BRK_addr[b].H);
	  zdi_write (BRK_addr[b].U);
	  // print info
	  dprint (PSTR ("BRK%d="), b);
	  dprint (PSTR ("%06X"), BRK_addr[b].val);
	  if (b < 2) {
	    cmd = next_par (cmd);
	    if (cmd != rd) {
	      BRK_mask |= (ZDI_IGN_LOW_0 << b);
	      dputs (PSTR (".page"));
	    } else {
	      BRK_mask &= ~(ZDI_IGN_LOW_0 << b);
	    }
	  }
	  dputc ('\n');
	}
	// refresh breakpoint settings
	if ((zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE))
	  break;
      case CMD_RUN:
	// run CPU
	CPU_state &= ~ZDI_ACTIVE;
	zdi_write_byte (ZDI_BRK_CTRL, BRK_mask);
	break;
      case CMD_STEP:
	// check if STOP mode
	if (!(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE))
	  break;
	// make one step
	zdi_write_byte (ZDI_BRK_CTRL, ZDI_BRK_NEXT | ZDI_SINGLE_STEP);
	// print changed registers, next instruction etc.
	show_step ();
	break;
      case CMD_DIS:
	//if (!(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE))
	//  break;
	{
	  unsigned long addr, stop;
	  unsigned char flags;
	  // load address
	  if (cmd != rd) {
	    addr = cmd_val (cmd);
	    cmd = next_par (cmd);
	  } else {
	    addr = cpu_new.regs[11].val;
	  }
	  // load stop address
	  if (cmd != rd) {
	    stop = cmd_val (cmd);
	    cmd = next_par (cmd);
	  } else {
	    stop = addr;
	  }
	  // load flags: ADL is non-zero
	  if (cmd != rd) {
	    flags = cmd_val (cmd);
	    //cmd = next_par (cmd);
	  } else {
	    flags = zdi_read_byte (ZDI_STAT) & ZDI_ADL;
	  }
	  while (addr <= stop) {
	    unsigned char len;
	    len = disasm (addr, flags);
	    addr += len;
	  }
	}
	break;
      case CMD_EXEC: // execute entered opcode
	if (cmd == rd)
	  break;
	// check if STOP mode
	if (!(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE))
	  break;
	b = 0;
	{
	  unsigned char tmp = cmd;
	  while (tmp != rd && b < 5) {
	    instr[b++] = cmd_val (tmp);
	    tmp = next_par (tmp);
	  }
	  if (b > 5) {
	    //b = 5;
	    break;
	  }
	}
	zdi_open (SAVE_PC);
	regs_init (0);
	exec (instr, b);
	regs_changed ();
	zdi_close ();
	break;
      case CMD_EXX:
        zdi_exx ();
	regs_changed ();
	break;
      case CMD_ADL:
	if (!(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE))
	  break;
	if (cmd != rd) {
	  // set ADL
	  zdi_write_byte (ZDI_RW_CTL, cmd_val (cmd) ? ADL_SET : ADL_CLR);
	}
	// print ADL
	dprint (PSTR ("ADL=%d\n"), (zdi_read_byte (ZDI_STAT) & ZDI_ADL) != 0);
	break;
      case CMD_MADL:
	if (cmd != rd) {
	  // set MADL
	  zdi_open (SAVE_PC);
	  instr[0] = 0xED;
	  instr[1] = (cmd_val (cmd)) ? 0x7D : 0x7E;
	  exec (instr, 2);
	  zdi_close ();
	}
	// print MADL
	dprint (PSTR ("MADL=%d\n"), (zdi_read_byte (ZDI_STAT) & ZDI_MADL) != 0);
	break;
      case CMD_MAP:
        // internal Flash
	b = input (FLASH_CTRL);
	dputs (PSTR ("Flash: "));
	if (b & FLASH_EN) {
	  dprint (PSTR ("%02X"), input (FLASH_ADDR_U));
	  dprint (PSTR (" %dws\n"), (b & FLASH_WAIT_MASK) >> FLASH_WAIT_BIT);
	} else {
	  dputs (PSTR ("disabled.\n"));
	}
        // internal RAM
	b = input (RAM_CTL);
	dputs (PSTR ("RAM: "));
	if (b & RAM_EN) {
	  dprint (PSTR ("%02X\n"), input (RAM_ADDR_U));
	} else {
	  dputs (PSTR ("disabled.\n"));
	}
        // CSx
	for (cmd = 0; cmd < 4; cmd++) {
	  b = input (CS0_CTL + cmd*3);
	  dprint (PSTR ("CS%d: "), cmd);
	  if (b & CSx_EN) {
	    if (b & CSx_IO) {
	      dputs (PSTR ("IO "));
	    } else {
	      dputs (PSTR ("MEM "));
	    }
	    dprint (PSTR ("%02X"), input (CS0_LBR + cmd*3));
	    dprint (PSTR ("-%02X"), input (CS0_UBR + cmd*3));
	    dprint (PSTR (" %dws\n"), (b & CSx_WAIT_MASK) >> CSx_WAIT_BIT);
	  } else {
	    dputs (PSTR ("disabled.\n"));
	  }
	}
	break;
      // ------------------------------
      // Intel-HEX processing mode
      // ------------------------------
      case CMD_MODE:
	if (cmd != rd) {
	  b = cmd_val (cmd);
	  if (b > 2)
	    b = 2;
	  HEX_mode = b;
	}
	dprint (PSTR ("MODE=%d\n"), HEX_mode);
	break;
      // ------------------------------
      // command testing
      // ------------------------------
      case CMD_TEST:
#if 0
	// read ZDI registers ID_L:ID_H:ID_REV, STAT
        zdi_addr (ZDI_ID_L);
        dputs (PSTR ("ID="));
        for (b = 4; b; b--) {
          dprint (PSTR ("%X "), zdi_read ());
        }
        dputc ('\n');
	// read "read memory address" (14-bit value)
        zdi_addr (ZDI_RD_DATA_L);
        dputs (PSTR ("RD="));
        for (b = 3; b; b--) {
          dprint (PSTR ("%X "), zdi_read ());
        }
        dputc ('\n');
	// read BUS stat : data
        dprint (PSTR ("BUStat=%X\n"), zdi_read_byte (ZDI_BUS_STAT));
        //dprint (PSTR (" %X\n"), zdi_read_byte (ZDI_RD_MEM)); // triggers memory read & increments PC (twice! why?)
#elif 0
	// pokusy s EXEC
	if (cmd == rd)
	  break;
	b = cmd_val (cmd);
	zdi_write_byte (ZDI_IS0, b);
#elif 0
	// pokusy RESET
	if (cmd != rd)
	  b = cmd_val (cmd);
	else
	  b = 0;
	zdi_write_byte (ZDI_BRK_CTRL, ZDI_BRK_NEXT);
	zdi_write (ZDI_RESET);
	while (b--)
	  asm volatile ("nop");
	zdi_write_byte (ZDI_BRK_CTRL, ZDI_BRK_NEXT);
#endif
	break;
      // ------------------------------
      // view stack
      // ------------------------------
      case CMD_STACK:
      case CMD_SPS:
      case CMD_SPL:
	{
	  unsigned char cnt;
	  data_type val;
	  // get item count
	  if (cmd != rd) {
	    cnt = cmd_val (cmd);
	  } else {
	    cnt = 5;
	  }
	  // prepare ADL mode
	  zdi_open (0);
	  cmd = zdi_read_byte (ZDI_STAT);
	  if ((cmd & ZDI_ADL) && b == CMD_SPS) {
	    // reset ADL
	    zdi_write_byte (ZDI_RW_CTL, ADL_CLR);
	    cmd &= ~ZDI_ACTIVE;
	  } else
	  if (!(cmd & ZDI_ADL) && b == CMD_SPL) {
	    // set ADL
	    zdi_write_byte (ZDI_RW_CTL, ADL_CLR);
	    cmd &= ~ZDI_ACTIVE;
	  }
	  // prepare stack item length
	  if (((cmd & ZDI_ADL) && b != CMD_SPS) || b == CMD_SPL)
	    b = 3;
	  else
	    b = 2;
	  // read SP* and set PC
	  zdi_write_byte (ZDI_RW_CTL, ZDI_REG_SP);
	  val = zdi_read_val ();
	  zdi_pc (val);
	  // read & print memory
	  zdi_addr (ZDI_RD_MEM);
	  while (cnt--) {
	    instr[0] = zdi_read ();
	    instr[1] = zdi_read ();
	    if (b > 2) {
	      dprint (PSTR ("%06X:"), val.val);
	      val.val += b;
	      dprint (PSTR ("%02X"), zdi_read ());
	    } else {
	      dprint (PSTR ("%04X:"), val.offs);
	      val.offs += b;
	    }
	    dprint (PSTR ("%02X"), instr[1]);
	    dprint (PSTR ("%02X\n"), instr[0]);
	  }
	  // restore ADL
	  if (!(cmd & ZDI_ACTIVE))
	    zdi_write_byte (ZDI_RW_CTL, (cmd & ZDI_ADL) ? ADL_SET : ADL_CLR);
	  zdi_close ();
	}
	break;
      // ------------------------------
      // register access
      // ------------------------------
      case CMD_REG:
        // read/write CPU registers
        if (cmd == rd) {
	  zdi_open (0);
          // read all registers
	  regs_init (0);
	  for (cmd = REG_RD_AF; cmd <= REG_RD_HL; cmd++) {
	    reg_print (cmd);
	    if ((cmd & 3) == 3)
	      dputc ('\n');
	    else
	      dputc ('\t');
          }
          for (cmd = REG_RD_AF; cmd <= REG_RD_PC + 1; cmd++) {
	    reg_print (4 + cmd);
	    if ((cmd & 3) == 3)
	      dputc ('\n');
	    else
	      dputc ('\t');
          }
	  // other SP* register
	  //reg_print (4 + REG_RD_PC + 1);
	  // show CPU state
	  cmd = zdi_read_byte (ZDI_STAT);
	  dprint (PSTR ("ADL=%X\t"), (cmd & ZDI_ADL) != 0);
	  dprint (PSTR ("MADL=%X\t"), (cmd & ZDI_MADL) != 0);
	  dprint (PSTR ("IEF1=%X\t"), (cmd & ZDI_IEF1) != 0);
	  if (cmd & ZDI_HALT_SLP)
	    dputs (PSTR ("HALT\t"));
	  zdi_close ();
	  cmd = zdi_read_byte (ZDI_STAT);
	  if (cmd & ZDI_ACTIVE)
	    dputs (PSTR ("STOP"));
	  dputc ('\n');
	  break;
        }
	b = REG_FIRST + cmd_get (reg_tab, cmd);
      default:
	// look for register name
	if (b > CMD_COUNT && b <= REG_LAST) {
	  // valid register name
	  data_type val;
	  b -= REG_FIRST;
	  // reg_print is hard to use here...
	  dputs (reg_tab + (b * 3));
	  zdi_open (0);
	  if (buffer[cmd+2] == '\'' && b < 4) {
	    // alternate register
	    zdi_exx ();
	    dputc ('\'');
	  }
	  cmd = next_par (cmd);
	  if (cmd == rd) {
	    // read one register
	    zdi_write_byte (ZDI_RW_CTL, b);
	    val = zdi_read_val ();
	  } else {
	    // write one register
	    val.val = cmd_val (cmd);
	    zdi_write_val (val);
	    zdi_write_byte (ZDI_RW_CTL, b | ZDI_REG_WR);
	    // alternate register set' can confuse saved results!!
	    zdi_close ();
	    zdi_open (0);
	    // reload all registers
	    regs_init (0);
	  }
	  zdi_close ();
	  dprint (PSTR ("=%06X\n"), val.val);
	  break;
	}
	// invalid register/command name
	dputs (PSTR ("Unknown!\n"));
	break;
    }
    // check register change
    // ... already processed - see above
    // prepare for new prompt
    cmd = rd;
    dputc ('>');
  }
  return 0;
}
