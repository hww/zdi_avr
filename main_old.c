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
x nefunguje asi prekladac...
  CMD_RST
  promenna b se prepise a puvodni hodnota se ztrati!!!
  -> nekolikrat prepsany kod pro reset, takze uz to neni potreba (snad) resit
o nenastavi se spravne banka pro zapis do pameti
:40000000F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C020070
:020000040006F4
:08000000C303FA0000C306EC83
:00000001FF
  -> tohle prepise RAM na adrese 000000

- stale nefunguje spravne zapis do RAM
  defaultne je povolena Flash!!!
  tj. pro pokusy musim zakazat iFlash a povolit eRAM
stop
rst
out f8 0
out aa 0
out ab 0
out ac 7
out ad 28
:40000000101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f00
:40000000505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f00
  po dokonceni jednoho radku je hodnota PC divne mala, pokazde ale jina... a v RAM se zmeni jen par bytu

  prikaz memw funguje dobre - tam se zapisuje z bufferu, tj. neceka se na zadna data...
  mozna tam je nejaky maximalni cas mezi daty, pri prekroceni se "kanal uzavre"... ??
  tj. bud muzu zpracovavat take cely radek najednou
  nebo muzu vyuzit buffer pro operace flash a data pak zapsat najednou...
:40000000F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C0200F3ED7E5BC38C020070
:050066005BC322020053
:400100006001640168016C017001740178017C018001840188018C019001940198019C01A001A401A801AC01B001B401B801BC01C001C401C801CC01D001D401D801DC01DF
:40014000E001E401E801EC01F001F401F801FC010002040208020C021002140218021C02C3ECE0B7C3F0E0B7C3F4E0B7C3F8E0B7C3FCE0B7C300E1B7C304E1B7C308E1B7E4

x v rezimu Intel-HEX se vypisuje info o PC, instrukce apod.
  to je uplne spatne!!!
  -> i kdyz: mozna za to muze CR-LF na konci radku...?

ToDo:
=====
- na osciloskopu si zmerit, jak dlouho trva zapis jednoho byte po ZDI
  kvuli casovani zapisu do RAM a hlavne Flash
  zdi_write_val (...) zapisuje byty po 8us
  win: 6.8us ZCL/byte; 8.32us do druheho bytu na ZCL (komunikace po prikazu RST)
  2 byty se prenaseji 347us

x mozna lepsi podpora pro mapovani CSn / pameti
  tj. jediny prikaz CS0..CS3, ale pak i pro interni RAM a Flash...?
  -> IMHO staci soucasna podpora pro zapis do I/O registru...
- zacit pracovat na podpore programovani do MCUload
  tj. nejak definovat nastaveni mapovani pameti
  pak odeslani samotneho HEXu
  jeste nevim, zda se bude zapis do RAM/Flash nejak lisit ...? asi ano...
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
  - erase: asi jen bit PG_ERASE / MASS_ERASE
    cislo page: FLASH_PAGE registr
  programming:
  - single byte: 66..85us(byte)/10.8ms(row)
    tj. 13 taktu Flash-clock
    zapis FLASH_PAGE, FLASH_ROW, FLASH_COL
    zapis FLASH_DATA
  - multibyte: 41..52us(byte)/6.7ms(row)
    tj. 8 taktu Flash-clock
    kontrola FLASH_IRQ
    zapis FLASH_PAGE, FLASH_ROW, FLASH_COL
    nastavit bit FLASH_PGCTL.ROW_PGM
    zapisovat data FLASH_DATA (adresa je automaticky inkrementovana)
    high-voltage je trvale aktivni, dokud se nezapise cely radek...
    musi se kontrolovat row-timeout
      2432 flash controller clocks (12.4-15.8)ms
        frekvence pro Flash: (154..196)kHz
	pro vypocet se pouziva hodnota tak, aby byla co nejbliz k 196kHz

      tj. zapis by musel byt provadeny z dat ulozenych v bufferu
        ale asi ani to by nestacilo:
	nastaveni dat do registru
	vykonani instrukce pro zapis na port
	behem zapisu je nastaveny Wait signal, ktery blokuje procesor pri pristupu ke flash
      ATmega8 komunikuje na 19200Bd, tedy znak ~ 500us --> to je prilis pomale!!!
      tedy buffer by musel byt v RAM procesoru a pouzit instrukci OTIR
  - single-byte memory write: 66..85us, data se prebiraji ze zapisoveho cyklu sbernice
    -> tento rezim by se dal pouzit a byl by kompatibilni se zapisem do RAM; ale pouziva zrejme pomalejsi "single-byte" zapis...
    jen se mi nechce verit, ze zapis se opravdu spousti jen zapisem na danou adresu... to by pak jakykoli zapis znicil obsah flash...?
  Flash aktivuje signal WAIT, dokud probiha samotny zapis
  -> pro zacatek budu muset zkusit signle byte, resp. memory write
  FLASH_KEY {0xB6, 0x49} odmyka pristup k FLASH_FDIV, FLASH_PROT...
  
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
//  dputs (PSTR ("ZDI driver v"));
  dputs (PSTR ("ZDI v"));
  dputs (build);
  dputs (PSTR ("\nbuilt "));
  dputs (build_date);
  dputc (',');
  dputs (build_time);
  dputc ('\n');
}

// ====================================
// buffer
// ====================================
static unsigned char buffer[256];
static unsigned char wr = 0, rd = 0;

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
  "PC\0";
// ------------------------------------
#define	CMD_INDEX_FLAG	0x01
// ------------------------------------
static unsigned char cmd_get (const char *tab, unsigned char idx) {
unsigned char b, d, i, j;
  while (buffer[idx] <= ' ') {
    if (idx == rd)
      return CMD_COUNT;
    idx++;
  }
  i = 0;
  for (i = 0; i < CMD_COUNT; i++) {
    j = idx;
    do {
      b = buffer[j];
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
static data_type regs_old[12], regs_new[12];
// ------------------------------------
static void regs_init (void) {
#warning "taky si schovat ADL, MADL, IFF..."
  zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL /*| SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC*/, regs_old);
  zdi_exx ();
  //zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL, &regs_old[8]);
  zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL | SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC, &regs_old[4]);
  zdi_exx ();
}

static void reg_print (unsigned char idx) {
  dputs (reg_tab + (((idx < 4) ? idx : (idx - 4)) * 3));
  if (idx >= 4 && idx < 8)
    dputc ('\'');
  else
    dputc (' ');
  dprint (PSTR ("=%06X"), regs_old[idx].val);
}

static void regs_changed (void) {
unsigned char idx, cnt = 0;
  zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL /*| SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC*/, regs_new);
  zdi_exx ();
  //zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL, &regs_new[8]);
  zdi_reg (SAVE_AF | SAVE_BC | SAVE_DE | SAVE_HL | SAVE_IX | SAVE_IY | SAVE_SP | SAVE_PC, &regs_new[4]);
  // PC shouldn't be printed...
  regs_old[11].val = regs_new[11].val;
  // print all changed registers
  for (idx = 0; idx < 12; idx++) {
    if (regs_new[idx].val == regs_old[idx].val)
      continue;
    regs_old[idx].val = regs_new[idx].val;
    if (cnt >= 4) {
      cnt = 1;
      dputc ('\n');
    } else
    if (cnt++) {
      dputc ('\t');
    }
    reg_print (idx);
  }
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
  dprint (PSTR("%06X:\t"), regs_new[11].val);
  // disassemble
  z80_disasm (instr, regs_new[11].val, zdi_read_byte (ZDI_STAT) & ZDI_ADL);
  dputc ('\n');
}

// ====================================
// opcode execution
// ====================================
static void exec (unsigned char *op, unsigned char len) {
  // execute instruction
  zdi_addr (ZDI_IS0 + 2 - (len << 1));
  op += len;
  while (len--) {
    zdi_write (*--op);
  }
  // dummy NOP
  zdi_write_byte (ZDI_IS0, 0x00);
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
      cmd = rd = wr;
    }
    if (TIFR & (1 << TOV0)) {
      static unsigned short cnt = 0;
      TIFR = (1 << TOV0);
      if (!cnt) {
	cnt = 100000 / 256;
	b = zdi_read_byte (ZDI_STAT);
	if ((b & ZDI_ACTIVE)) {
	  if (!(CPU_state & ZDI_ACTIVE) && cmd == rd) {
	    CPU_state |= ZDI_ACTIVE;
	    //dputc ('\r');
	    //regs_changed ();
	    //dprint (PSTR ("break at %06X\n"), regs_new[11].val);
	    dputs (PSTR ("\rBreak!\n"));
	    show_step ();
	  }
	} else {
	  CPU_state &= ~ZDI_ACTIVE;
	}
      } else {
	cnt--;
      }
    }
    // read data to buffer
    if (dkbhit ())
      buffer[wr++] = dgetc ();
    // check new data
    if (rd == wr)
      continue;
    // store new data
    b = buffer[rd++];
    // command mode
    if (b != '\r' && b != '\n') {
      // process Intel-HEX byte-by-byte
      if (buffer[cmd] == ':') {
	// process every two-character byte
	static unsigned char type, len, check;
	static unsigned short page, offs;
	//static unsigned long start = 0xFFFFFFFF;
	static data_type addr = {.val = 0xFFFFFFFF};
	unsigned char tmp;
	static unsigned char flash_buf[128], flash_ptr = 0;
	b = rd - cmd - 1;
	if (b & 1)
	  continue;
	if (!b) {
	  check = 0;
	  continue;
	}
	tmp = hex_val (cmd + b - 1);
	check += tmp;
	if (b < 10) {
	  if (b == 2)
	    len = tmp;
	  else
	  if (b == 4)
	    offs = tmp << 8;
	  else
	  if (b == 6)
	    offs |= tmp;
	  else
	  if (b == 8)
	    type = tmp;
	  continue;
	}
	// process length & checksum
	if (!len) {
	  // checksum
	  if (check) {
	    dputc ('C');
	    continue;
	  }
	  dputc ('>');
	  if (type == 0x00) {
// prakticky budu potrebovat bud dvojity buffer nebo offline zpracovani bufferu...
	    // write flash buffer



	  } else
	  if (type == 0x01) {
	    // end-of-file record
#if 0
	    // run application
	    addr.val = start;
	    zdi_write_val (addr);
	    zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC | ZDI_REG_WR);
	    zdi_write_byte (ZDI_BRK_CTRL, 0);
#else
	    // set PC to 0
	    addr.val = 0;
	    zdi_write_val (addr);
	    zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC | ZDI_REG_WR);
#endif
	    addr.val = 0xFFFFFFFF;
	    //start = 0xFFFFFFFF;
	    page = 0;
	  }
	  continue;
	}
	len--;
	// process record bytes
	if (type == 0x00) {
	  // data record
#if 0
	  if (b == 10) {
	    // check used address
	    if (offs != addr.offs || page != addr.page) {
	      // set new address
	      addr.offs = offs;
	      addr.page = page;
//dprint (PSTR ("PC:%06X\n"), addr.val);
dputc ('a');
	      zdi_write_val (addr);
	      zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC | ZDI_REG_WR);
	      // start data write
	      zdi_addr (ZDI_WR_MEM);
//	      if (addr.val < start)
//		start = addr.val;
	    }
	  }
dputc ('#');
	  zdi_write (tmp);
	  addr.val++;
#else
	  if (b == 10)
	    flash_ptr = 0;
	  flash_buf[flash_ptr++] = tmp;
	  if (!len) {
	    // check used address
	    if (offs != addr.offs || page != addr.page) {
	      // set new address
	      addr.offs = offs;
	      addr.page = page;
	      zdi_write_val (addr);
	      zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC | ZDI_REG_WR);
	    }
	    // start data write
	    zdi_addr (ZDI_WR_MEM);
	    for (b = 0; b < flash_ptr; b++) {
	      zdi_write (flash_buf[b]);
	      addr.val++;
	    }
	  }
#endif
	} else
	if (type == 0x04) {
//#error "tohle nefunguje spravne... viz poznamka v 'bugs'..."
	  // extended linear address
	  //:020000040006F4
	  page <<= 8;
	  page |= tmp;
//if (!len) dprint (PSTR ("page:%04X\n"), page);
/**
	} else
	if (type == 0x05) {
	  // start/run linear address
**/
	}
      } else
      // echo
      if (b >= ' ')
        dputc (b);
      continue;
    }
    // CR or LF is last received byte
    if (buffer[cmd] == ':') {
      // Intel-HEX record processed already
      // just release buffer
      cmd = rd;
      //dputc ('>');
      continue;
    }
    // look for command
    b = buffer[cmd];
    if (b == '\r' || b == '\n') {
      // repeat last command?
      // or simple: just force step command
      b = CMD_STEP;
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
	regs_init ();
	if (cmd != rd || !(zdi_read_byte (ZDI_STAT) & ZDI_ACTIVE)) {
	  b = cmd_val (cmd);
	  // single reset pulse
	  reset_cpu (b);
	  dputc ('\n');
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
#if 0
// test se musi provadet ve stavu STOP, jinak se tento kod nikdy nevykona...
dprint (PSTR ("%d:"), cmd);
dprint (PSTR (" %06X"), val.val);
dprint (PSTR ("-%02X\n"), zdi_read_byte (ZDI_STAT));
if (++cmd > 0x60) break;
continue;
#endif
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
	  if (cmd != rd) {
	    val.val = cmd_val (cmd);
	    zdi_pc (val);
	    cmd = next_par (cmd);
	  } else {
	    zdi_write_byte (ZDI_RW_CTL, ZDI_REG_PC);
	    val = zdi_read_val ();
	  }
	  if (cmd != rd) {
	    cnt = cmd_val (cmd);
	  } else {
	    cnt = 0x20;
	  }
	  cmd = 0;
	  zdi_addr (ZDI_RD_MEM);
	  while (cnt) {
	    if (!cmd) {
	      cmd = (cnt > 0x20) ? 0x20 : (unsigned char)cnt;
	      dprint (PSTR (":%02X"), cmd);
	      dprint (PSTR ("%04X00"), val.val);
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
	  // set A register
	  if (cmd != rd) {
	    val.val = cmd_val (cmd);
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
	regs_init ();
	exec (instr, b);
	regs_changed ();
	zdi_close ();
	break;
      case CMD_EXX:
        zdi_exx ();
	regs_changed ();
	break;
      case CMD_ADL:
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
	  instr[0] = 0xED;
	  instr[1] = (cmd_val (cmd)) ? 0x7D : 0x7E;
	  exec (instr, 2);
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
      // register access
      // ------------------------------
      case CMD_REG:
        // read/write CPU registers
        if (cmd == rd) {
	  zdi_open (0);
          // read all registers
	  regs_init ();
	  for (cmd = REG_RD_AF; cmd <= REG_RD_HL; cmd++) {
	    reg_print (cmd);
	    if ((cmd & 3) == 3)
	      dputc ('\n');
	    else
	      dputc ('\t');
          }
          for (cmd = REG_RD_AF; cmd <= REG_RD_PC; cmd++) {
	    reg_print (4 + cmd);
	    if ((cmd & 3) == 3)
	      dputc ('\n');
	    else
	      dputc ('\t');
          }
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
	    regs_init ();
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



    // prepare for new prompt
    cmd = rd;
    dputc ('>');
  }
  return 0;
}
