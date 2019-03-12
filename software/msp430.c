// ====================================
// MSP430Fxxx BSL programming utility
// (C) DEC 2011 Hynek Sladky
// ====================================

#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "os.h"
#include "file.h"
#include "load.h"
#include "msp430.h"


/**
Bugs:
====
- linux neumi prepinat baudrate bez nulovani DTR/RTS
  -> zatim vyreseno prepnutim pinu RST na NMI (viz RST_HACK)
  ale chtelo by to systemove reseni...!
  - zkusit tcsetattr:
    - zda je DTR ovlivneno jen prostym volanim teto funkce nebo pouze pri zmene nekterych parametru?
      tj. zkusit menit ruzne parametry (CS8-CS6 apod.)
    - slo by mit prepnutou rychlost B38400 a 9600 posilat pri CS6 jako 4+4 bity (3+3 bity datove)
      ale pokud zmena CS take ovlivnuje DTR, nema cenu o tomto uvazovat
    - invertory (XOR s DIP) pro BOOT a RST signaly
      pak by klidova uroven byla nulova a pres invertor bude spravne jednicka i po nastaveni baudrate
    - zkusit dale hledat reseni; treba se to tyka jen CDC ?? nebo to neni v Mintu opravene ??
      zkusit forum pro Mint ??

ToDo:
====
- podpora pro odemceni procesoru heslem z jineho HEXu
- podpora pro ruzne procesory
- podpora kodu v RAM + spusteni
  mspgcc uklada do HEXu adresu spusteni...

**/
// ------------------------------------
// compile options
// ------------------------------------
#ifdef COMPILE_FOR_LINUX
#define	RST_HACK
#endif

// ------------------------------------
// shared variables
// ------------------------------------

// MSP430 setup
char *BLname = "bl_150s_14x.hex";
int BSL_TEST = 0;
int fast = 1; //0; //1;
char *pwd_name = 0;

// ------------------------------------
// program locals
// ------------------------------------
static unsigned char ack_data[256];
// ------------------------------------
// MSP430F flash password - erased state
// ------------------------------------
static const unsigned short def_pwd[16]={0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};

#define	ERASE_MASS	"\x80\x18\x04\x04\xE0\xFF\x06\xA5", 8
#define	ERASE_MAIN	"\x80\x16\x04\x04\xE0\xFF\x04\xA5", 8
#define	ERASE_SEG(a)	"\x80\x16\x04\x04" a "\x02\xA5", 8
#define	LOAD_PC(a)	"\x80\x1A\x04\x04" a "\x12\x34", 8
#define	PASSWD_FF	"\x80\x10\x24\x24\x00\x00\x00\x00"\
			"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"\
			"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"\
			"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"\
			"\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF", 40
#define	READ_MEM(a)	"\x80\x14\x04\x04" a, 8

#define	ACK		0x90
#define	NAK		0xA0
#define	ACK_DATA	0x80


// ====================================
// MSP430Fxxx bootloader functions
// ====================================
// BSL command header
// ------------------------------------
static struct {
  unsigned char SYNC;
  unsigned char CMD;
  unsigned char len1, len2;
  unsigned short addr;
  unsigned short len;
} __attribute__ ((packed)) header;
// ------------------------------------
// send BSL command
// ------------------------------------
static int sendcmd (COM_HANDLE hCom, unsigned char cmd, unsigned char hdrlen, unsigned short addr, unsigned short len, const void *data) {
int i, cnt;
unsigned char ack;
unsigned short *ptr, crc;
  if (verbose >= VERB_COMM) {
    printf ("\r>%02X %02X %04X %04X", cmd, hdrlen, addr, len);
    if (data)
      printf (" +data");
    putchar ('\n');
  }
  // vymena SYNC-ACK
  for (cnt = 3; cnt; cnt--) {
    com_send_buf (hCom, (unsigned char*)"\x80", 1);
    ack = com_rec_byte (hCom, 100);
    if (ack == 0x90) break;
    printf ("Unknown ACK! (%02X)\n", ack);
  }
  if (ack != 0x90) 
    return 1;
  os_sleep (10);
  // poslani hlavicky
  header.SYNC = 0x80;
  header.CMD = cmd;
  header.len1 = header.len2 = hdrlen;
  header.len = len;
  header.addr = addr;
  com_send_buf (hCom, (unsigned char*)&header, sizeof (header));
  crc = 0xFFFF;
  ptr = (unsigned short*)&header;
  for (i = sizeof(header)/2; i; i--, ptr++)
    crc ^= *ptr;
  // poslani dat
  if (data != NULL) {
    com_send_buf (hCom, data, len);
    ptr = (unsigned short*)data;
    for (i = len/2; i; i--, ptr++)
      crc ^= *ptr;
  }
  com_send_buf (hCom, (unsigned char*)&crc, sizeof (crc));
  // prijem ACK
  ack = com_rec_byte (hCom, 1000);
  if (ack == 0x90)
    return 0;
  if (ack != 0x80) {
    printf ("Unknown ACK! (%02X).\n", ack);
    return 2;
  }
  // tady mi chce poslat nejaka data, tedy 4B + data + CRC
#warning "Tady IMHO chybi kontrola prijimanych dat!!!"
  header.CMD = com_rec_byte (hCom, 100);
  header.len1 = com_rec_byte (hCom, 100);
  header.len2 = com_rec_byte (hCom, 100);
  if (header.len1 != header.len2)
    return 3;
  if (verbose >= VERB_COMM)
    printf ("<(%02X) %02X %02X %02X", ack, header.CMD, header.len1, header.len2);
  for (i = 0; i < header.len1+2; i++) {
    ack_data[i] = com_rec_byte (hCom, 100);
  }
  if (verbose >= VERB_COMM)
    printf (" +data\n");
  return 0;
}
// ------------------------------------
// run MSP bootloader
// ------------------------------------
void RunBSL (COM_HANDLE hCom) {
  // nastaveni serioveho portu
  com_speed (hCom, 9600, COM_PAR_EVEN);
  os_sleep (100);
  // reset procesoru
  com_set_signal (hCom, (BSL_TEST) ? BOOT_LOW | RST_HIGH : BOOT_HIGH | RST_HIGH, 100);
  com_set_signal (hCom, RST_LOW, 100);
  // dummy write (CDC @ win needs something to be written before data read even works!)
  com_send_buf (hCom, (unsigned char*)"\x00", 1);
  // spusteni BSL (TCK)
  com_set_signal (hCom, (BSL_TEST) ? BOOT_HIGH : BOOT_LOW, 0);
  com_set_signal (hCom, (BSL_TEST) ? BOOT_LOW : BOOT_HIGH, 0);
  com_set_signal (hCom, (BSL_TEST) ? BOOT_HIGH : BOOT_LOW, 0);
  com_set_signal (hCom, RST_HIGH, 0);
  com_set_signal (hCom, (BSL_TEST) ? BOOT_LOW : BOOT_HIGH, 250);
  com_flush (hCom);
}
// ------------------------------------
// send IntelHEX to MSP BSL
// ------------------------------------
#define	BlockMaxLen	0xF0
enum {HEX_std, HEX_run};
// ------------------------------------
static int SendHex (COM_HANDLE hCom, const file_data_type *data, int mode) {
//int i, len, adr, maxadr;
//unsigned char dbuf[BlockMaxLen];
const file_data_type *ptr;
unsigned addr_max;

  if (!data || !data->data)
    return -1;
  if (verbose >= VERB_COMM)
    printf ("Sending data...\n");
  // prepare block pointer
  ptr = data;
  if (ptr->next)
    ptr = ptr->next;
  // prepare address limit
  addr_max = (mode == HEX_run) ? 0xFFE0 : 0x10000;
  while (ptr) {
    unsigned pos = 0;
    do {
      unsigned char dbuf[BlockMaxLen];
      unsigned len = 0, addr;
      int i;
      // check if data is aligned to even eddresses
      addr = ptr->addr + pos;
      if (addr & 1) {
        dbuf[len++] = 0xFF;
        addr--;
      }
      // fill send buffer
      while (len < BlockMaxLen && pos < ptr->len && (addr + len) < addr_max)
        dbuf[len++] = ptr->data[pos++];
      // check if buffer has even length
      if (len & 1)
        dbuf[len++] = 0xFF;
      // send data
      if (verbose >= VERB_INFO) {
        printf (">%04X\r", addr);
        fflush (stdout);
      }
      i = sendcmd (hCom, 0x12, 0x04 + len, addr, len, dbuf);
      if (i) {
        printf ("Error (%i) sending data @ %04X!\n", i, addr);
        return 2;
      }
      if ((addr + len) >= addr_max)
        break;
    } while (pos < ptr->len);
    // go to next block
    ptr = ptr->next;
  }
  // verification
  if (verify) {
    if (verbose >= VERB_COMM)
      printf ("Verifying memory...\n");
    // prepare block pointer
    ptr = data;
    if (ptr->next)
      ptr = ptr->next;
    while (ptr) {
      unsigned pos = 0;
      do {
        unsigned char dbuf[BlockMaxLen];
        unsigned len = 0, addr;
        int i;
        // check if data is aligned to even eddresses
        addr = ptr->addr + pos;
        if (addr & 1) {
          dbuf[len++] = 0xFF;
          addr--;
        }
        // fill send buffer
        while (len < BlockMaxLen && pos < ptr->len && (addr + len) < addr_max)
          dbuf[len++] = ptr->data[pos++];
        // check if buffer has even length
        if (len & 1)
          dbuf[len++] = 0xFF;
        // request data
        if (verbose >= VERB_INFO) {
	  printf (">%04X\r", addr);
	  fflush (stdout);
        }
        i = sendcmd (hCom, 0x14, 0x04 + len, addr, len, NULL);
        if (i) {
	  printf ("Error (%i) retrieving data @ %04X!\n", i, addr);
	  return 3;
        }
        // verification
        for (i = 0; i < len; i++) {
	  if (ack_data[i] != dbuf[i]) {
	    printf ("Error verifying data @ %04X!\n", addr + i);
#if 0
	    printf ("%04X: ", addr);
	    for (i = 0; i < len; i++)
	      printf ("%02X-%02X ", dbuf[i], ack_data[i]);
	    putchar ('\n');
#endif
	    return 4;
	  }
	  ack_data[i] = 0;
        }
        if ((addr + len) >= addr_max)
          break;
      } while (pos < ptr->len);
      // go to next block
      ptr = ptr->next;
    }
  }
  // code execution required
  if (mode == HEX_run) {
    unsigned addr = 0;
    int i;
    if (verbose >= VERB_COMM)
      printf ("Running MCU...\n");
    // prepare block pointer
    ptr = data;
    if (ptr->next)
      ptr = ptr->next;
    // look for reset vector
    while (ptr) {
      if (ptr->addr <= 0xFFFE && (ptr->addr + ptr->len) >= 0xFFFF) {
        // reset vector is here
        addr = ptr->data[0xFFFE - ptr->addr] | (ptr->data[0xFFFF - ptr->addr] << 8);
        break;
      }
      ptr = ptr->next;
    }
    if (!ptr) {
      // look for RAM based bootloader
      // note: patch.hex run address is 0x0220 directly!
      // prepare block pointer
      ptr = data;
      if (ptr->next)
        ptr = ptr->next;
      // look for RAM bootloader address
      while (ptr) {
        if (ptr->addr <= 0x0220 && (ptr->addr + ptr->len) >= 0x0221) {
          // reset vector is here
          addr = ptr->data[0x0220 - ptr->addr] | (ptr->data[0x0221 - ptr->addr] << 8);
          break;
        }
        ptr = ptr->next;
      }
    }
    if (verbose >= VERB_DEBUG)
      printf ("Executing code at address %04X...\n", addr);
    i = sendcmd (hCom, 0x1A, 0x04, addr, 0, NULL);
    if (i) {
      printf ("Error (%i) executing loaded SW!\n", i);
      return 5;
    }
  }
  return 0;
}

// ====================================
// MSP430 programming
// ====================================
int msp430_load (COM_HANDLE hCom, file_data_type *data, const char *arg0) {
int i, j;
unsigned short BSLver, MSPtype;
char path[1024];

  // check filename
  if (!data || !data->data)
    return -1;

#warning "Kontrola BSL key!!!"

  // inicializace komunikacnich dratu
  com_set_signal (hCom, RST_LOW | BOOT_LOW, 50);
  //if (!EscapeCommFunction (hCom, SETBREAK)) // TxD off
  //  printf ("Can't reset TxD signal!\n");

  if (verbose >= VERB_INFO)
    printf ("Initializing communication...\n");
  RunBSL (hCom);

  if (!pwd_name) {
    // nejprve musim provest 20x Erase
    if (verbose >= VERB_DEBUG)
      printf ("Erasing all flash...\n");
    j = sendcmd (hCom, 0x18, 0x04, 0xFFE0, 0xA506, NULL);
    if (j)
      return j;
  }
  // ted zapisu prazdny PassWord
  j = sendcmd (hCom, 0x10, 0x24, 0xFFE0, 0x0020, def_pwd);
  if (j) {
    file_data_type *pwd, *ptr;
    unsigned char usr_pwd[32];
    if (!pwd_name)
      return 3+j;
    // load password
    pwd = file_load (pwd_name);
    if (pwd == NULL) {
      printf ("Can't load password file %s!\n", pwd_name);
      return -1;
    }
    // look for password area
    memset (usr_pwd, 0xFF, sizeof (usr_pwd));
    ptr = pwd;
    while (ptr != NULL) {
      if ((ptr->addr >= 0xFFE0 && ptr->addr < 0x10000) || (ptr->addr + ptr->len > 0xFFFE && ptr->addr + ptr->len <= 0x10000)) {
        // copy password data
        unsigned pos = 0, idx = 0;
        if (ptr->addr < 0xFFE0)
          pos = 0xFFE0 - ptr->addr;
        if (ptr->addr + pos > 0xFFE0)
          idx = ptr->addr + pos - 0xFFE0;
        while (ptr->addr + pos < 0x10000 && pos < ptr->len)
          usr_pwd[idx++] = ptr->data[pos++];
      }
      ptr = ptr->next;
    }
    // use loaded password
    if (verbose >= VERB_DEBUG) {
      printf ("User password:");
      for (i = 0; i < sizeof (usr_pwd); i++)
        printf (" %02X", usr_pwd[i]);
      printf ("\n");
    }
    j = sendcmd (hCom, 0x10, 0x24, 0xFFE0, 0x0020, usr_pwd);
    if (j)
      return 3+j;
  }

  // dotaz na verzi BSL a chipu
  if ((j = sendcmd (hCom, 0x14, 0x04, 0x0FF0, 0x000C, NULL)))
    return 6+j;
  BSLver = (ack_data[10] << 8) | ack_data[11];
  MSPtype = (ack_data[0] << 8) | ack_data[1];
  if (verbose >= VERB_DEBUG)
    printf ("CHIP type: %04X\n#BSL ver: %02X.%02X\n", MSPtype, BSLver >> 8, BSLver & 0x00FF);
  if (BSLver == 0x0110) {
    if (verbose >= VERB_DEBUG)
      printf ("Additional erase loops...\n");
    for (i = 20; i; i--) {
      j = sendcmd (hCom, 0x18, 0x04, 0xFF00, 0xA506, NULL);
      if (j)
        break;
    }
    if (j)
      return j;
    // nastaveni SP...
    j = sendcmd (hCom, 0x1A, 0x04, 0x0C22, 0, NULL);
    if (j)
      return 9+j;
    // ted zapisu prazdny PassWord
    j = sendcmd (hCom, 0x10, 0x24, 0xFFE0, 0x0020, def_pwd);
    if (j)
      return 12+j;
    // naprogramovani nove verze BSL do RAM
    if (verbose >= VERB_DEBUG)
      printf ("Downloading RAM based bootloader...\n");
    {
      file_data_type *bl;
      bl = file_load (BLname);
      if (bl == NULL) {
#if 0
	// tato sekvence funguje zrejme jen ve Windows...
        strcpy (path, arg0);
        strcat (path, "/../");
        strcat (path, BLname);
#else
	unsigned idx;
        strcpy (path, arg0);
	for (idx = 0; path[idx]; idx++);
	while (idx && path[idx] != '/' && path[idx] != '\\' && path[idx] != ':')
	  idx--;
	if (path[idx] == '/' || path[idx] == '\\' || path[idx] == ':')
	  idx++;
        strcpy (path + idx, BLname);
#endif
        bl = file_load (path);
      }
      j = SendHex (hCom, bl, HEX_run);
    }
    if (j)
      return 34+j;
    BSLver = 0x0150;
  }
  if (fast) {
    // prepnuti na rychlejsi komunikacni rychlost
    if (verbose >= VERB_DEBUG)
      printf ("Fast speed...\n");
    if (BSL_TEST)
      j = sendcmd (hCom, 0x20, 0x04, 0x8C80, 0x0002, NULL); // MSP430F2132
    else
      j = sendcmd (hCom, 0x20, 0x04, 0x87E0, 0x0002, NULL); // MSP430F149
    if (j)
      return 24+j;
    // switch baudrate -> resets RTS/DTR on FT2232H
    // sometimes blocks COM7 (USB reconnection needed!)
#ifdef RST_HACK
#warning "DTR/RESET hack used!"
    {
      unsigned short wdtctl;
      if ((j = sendcmd (hCom, 0x14, 0x04, 0x0120, 0x0002, NULL)))
        return 80+j;
      wdtctl = ack_data[0] | (ack_data[1] << 8);
      if (verbose >= VERB_DEBUG)
	printf ("WDTCTL=%04X\n", wdtctl);
      if ((wdtctl & 0xFF00) == 0x6900) {
        wdtctl = (wdtctl & 0xFF) | 0x5A20;
        if ((j = sendcmd (hCom, 0x12, 0x06, 0x0120, 0x0002, &wdtctl)))
          return 83+j;
	if (verbose >= VERB_DEBUG)
	  printf ("RST switched to NMI.\n");
      } else {
        wdtctl = 0;
      }
#else
#ifdef COMPILE_FOR_LINUX
#warning "Don't know how to switch baudrate without affecting DTR/RTS!!"
#endif
#endif
    if (verbose >= VERB_DEBUG)
      printf ("Changing baudrate...\n");
    com_speed (hCom, 38400, COM_PAR_EVEN);
//    com_set_signal (hCom, (BSL_TEST) ? BOOT_LOW | RST_HIGH : BOOT_HIGH | RST_HIGH, 0);
//    com_set_signal (hCom, (BSL_TEST) ? BOOT_LOW | RST_HIGH : BOOT_HIGH | RST_HIGH, 0);
#ifdef RST_HACK
      if ((wdtctl & 0xFF00) == 0x5A00) {
        wdtctl &= ~0x0020;
        if ((j = sendcmd (hCom, 0x12, 0x06, 0x0120, 0x0002, &wdtctl)))
          return 83+j;
	if (verbose >= VERB_DEBUG)
	  printf ("RST switched back to RESET.\n");
      }
    }
#endif
  }
  if (pwd_name) {
    // erase all segments except InfoA
    // main flash
    if (verbose >= VERB_DEBUG)
      printf ("Erasing main flash...\n");
    for (i = 12; i; i--) {
      j = sendcmd (hCom, 0x16, 0x04, 0xFFE0, 0xA504, NULL);
      if (j)
        return j;
    }
    // Info D
    if (verbose >= VERB_DEBUG)
      printf ("Erasing Info D...\n");
    j = sendcmd (hCom, 0x16, 0x04, 0x1000, 0xA502, NULL);
    if (j)
      return j;
    // Info C
    if (verbose >= VERB_DEBUG)
      printf ("Erasing Info C...\n");
    j = sendcmd (hCom, 0x16, 0x04, 0x1040, 0xA502, NULL);
    if (j)
      return j;
    // Info B
    if (verbose >= VERB_DEBUG)
      printf ("Erasing Info B...\n");
    j = sendcmd (hCom, 0x16, 0x04, 0x1080, 0xA502, NULL);
    if (j)
      return j;
  }
  // naprogramovani vysledneho SW
  if (verbose >= VERB_INFO)
    printf ("Downloading memory...\n");
  j = SendHex (hCom, data, HEX_std);
  if (j)
    return 39+j;
  
  return 0;
}

