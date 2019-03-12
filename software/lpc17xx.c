#include <stdio.h>
#include <string.h>

#include "hardware.h"
#include "os.h"
#include "file.h"
#include "load.h"
#include "lpc17xx.h"

/**

ToDo:
====
- programovani Flash
  viz zdrojaky od ZIT
- vyresit nacitani HEXu spolecne pro vsechny MCU
  jeste nez se zacne programovat, tak je potreba znat vsechny parametry souboru
    IMHO to smeruje k vytvoreni celeho obrazu v RAM jako vazaneho seznamu bloku
    nakonec stejne bude potreba to znovu projit a bloky pripadne pospojovat...
  z tohoto seznamu pak jednoduse pujde zjistit jakakoliv data a jakekoliv adresy...
  - pri zpracovani HEXu by se take mely aktualizovat automaticky promenne file_offs jako nejnizsi adresa a file_run
    mozna by to spis melo byt tak, ze file_run se z HEX nastavi, ale file_offs se pouzije jen pri nacteni souboru .bin
    jeste by se melo vzit v uvahu, ze file_run zadane z prikazove radky ma vyssi prioritu pred udajem z HEX
    tj. pokud neni file_run nulove, tak se neprepisuje z HEXu
  zaroven musi existovat funkce pro uklizeni file_free (ptr);

- asi pridat do load.c parametr -R nebo --reset
  ale je otazka, zda to neni zbytecne...
  pokud se programuje do RAM, tak je reset nepouzitelny
  pokud se programuje do Flash, tak je zas prikaz Go dost obtizne proveditelny (musi se precist adresa z vektoru; ale da se zjistit z HEXu)
- pouzit parametry file_offs, file_run (pokud se nacita .bin)

**/

// ====================================
// global variables
// ====================================

// ====================================
// MCU parameters
// ====================================
enum {
  LPC21xx_128KB, LPC21xx_256KB,
  LPC213x_32KB, LPC213x_64KB, LPC213x_128KB, LPC213x_256KB, LPC213x_504KB, //LPC213x_512KB,
  LPC17xx_32KB, LPC17xx_64KB, LPC17xx_128KB, LPC17xx_256KB, LPC17xx_512KB,
  LPC_TYPE_CNT
};
#define	LPC_PAGE_CNT	3
static const struct {
  unsigned RAM_addr;
  unsigned short flash_block;
  struct {
    unsigned addr;
    unsigned short size;
    unsigned short count;
  } Flash[LPC_PAGE_CNT];
} LPC_memory[LPC_TYPE_CNT] = {
// LPC21xx
  {0x40000000, 0x2000, {{0x00000000, 8, 16}}}, // 128KB
  {0x40000000, 0x2000, {{0x00000000, 8, 8}, {0x00010000, 64, 2}, {0x00020000, 8, 8}}}, // 256KB
// LPC213x
  {0x40000000, 0x1000, {{0x00000000, 4, 8}}}, // 32KB
  {0x40000000, 0x1000, {{0x00000000, 4, 8}, {0x00008000, 32, 1}}}, // 64KB
  {0x40000000, 0x1000, {{0x00000000, 4, 8}, {0x00008000, 32, 3}}}, // 128KB
  {0x40000000, 0x1000, {{0x00000000, 4, 8}, {0x00008000, 32, 7}}}, // 256KB
  {0x40000000, 0x1000, {{0x00000000, 4, 8}, {0x00008000, 32, 14}, {0x00078000, 4, 5}}}, // 504KB
//  {0x40000000, {{0x00000000, 4, 8}, {0x00008000, 32, 14}, {0x0x00078000, 4, 8}}}, // 512KB: boot flash cannot be overwritten!
// LPC17xx
  {0x10000000, 0x1000, {{0x00000000, 4, 8}}}, // 32KB
  {0x10000000, 0x1000, {{0x00000000, 4, 16}}}, // 64KB
  {0x10000000, 0x1000, {{0x00000000, 4, 16}, {0x00010000, 32, 2}}}, // 128KB
  {0x10000000, 0x1000, {{0x00000000, 4, 16}, {0x00010000, 32, 6}}}, // 256KB
  {0x10000000, 0x1000, {{0x00000000, 4, 16}, {0x00010000, 32, 14}}} // 512KB
};

#define	SEND_CR		0x0001
#define	CORTEX_M	0x0002

static const struct {
  unsigned ID;
  char name[12];
  unsigned flags;
  unsigned mem_type;
} LPC_type[] = {
  {0x0101FF12, "LPC2114", 0,       LPC21xx_128KB},
  {0x0101FF13, "LPC2124", SEND_CR, LPC21xx_256KB},
  {0x0201FF12, "LPC2119", SEND_CR, LPC21xx_128KB},
  {0x0201FF13, "LPC2129", SEND_CR, LPC21xx_256KB},

  {0x0301FF13, "LPC2194", SEND_CR, LPC21xx_256KB},
  {0x0401FF12, "LPC2212", SEND_CR, LPC21xx_128KB},
  {0x0401FF13, "LPC2292", SEND_CR, LPC21xx_256KB},
  {0x0601FF13, "LPC2214", SEND_CR, LPC21xx_256KB},
  {0x0501FF13, "LPC2294", SEND_CR, LPC21xx_256KB},

  {0x0002FF01, "LPC2131", SEND_CR, LPC213x_32KB},
  {0x0002FF11, "LPC2132", SEND_CR, LPC213x_64KB},
  {0x0002FF12, "LPC2134", SEND_CR, LPC213x_128KB},
  {0x0002FF23, "LPC2136", SEND_CR, LPC213x_256KB},
  {0x0002FF25, "LPC2138", SEND_CR, LPC213x_504KB},

  {0x0402FF01, "LPC2141", SEND_CR, LPC213x_32KB},
  {0x0402FF11, "LPC2142", SEND_CR, LPC213x_64KB},
  {0x0402FF12, "LPC2144", SEND_CR, LPC213x_128KB},
  {0x0402FF23, "LPC2146", SEND_CR, LPC213x_256KB},
  {0x0402FF25, "LPC2148", SEND_CR, LPC213x_504KB},

  {0x0603FB02, "LPC2364",   0,     LPC213x_128KB},
  {0x1600F923, "LPC2366/B", 0,     LPC213x_256KB},
  {0x0603FB25, "LPC2368",   0,     LPC213x_504KB},
  {0x1600F925, "LPC2368/B", 0,     LPC213x_504KB},
  {0x1700FD25, "LPC2378/B", 0,     LPC213x_504KB},
  {0x0703FF25, "LPC2378",   0,     LPC213x_504KB},

  {0x26011922, "LPC1764", CORTEX_M, LPC17xx_128KB},
  {0x26013733, "LPC1765", CORTEX_M, LPC17xx_256KB},
  {0x26013F33, "LPC1766", CORTEX_M, LPC17xx_256KB},
  {0x26013F37, "LPC1768", CORTEX_M, LPC17xx_512KB},

  {0x25001110, "LPC1751", CORTEX_M, LPC17xx_32KB},
  {0x25001121, "LPC1752", CORTEX_M, LPC17xx_64KB},
//  {0x25013F37, "LPC1754", CORTEX_M, LPC17xx_128KB},
  {0x25011722, "LPC1754", CORTEX_M, LPC17xx_128KB},
  {0x25011723, "LPC1756", CORTEX_M, LPC17xx_256KB},
//  {0x26013F34, "LPC1758", CORTEX_M, LPC17xx_512KB},
  {0x25013F37, "LPC1758", CORTEX_M, LPC17xx_512KB},
  {0}
};

enum {
  CMD_SUCCESS,
  INVALID_COMMAND,
  SRC_ADDR_ERROR,
  DST_ADDR_ERROR,
  SRC_ADDR_NOT_MAPPED,
  DST_ADDR_NOT_MAPPED,
  COUNT_ERROR,
  INVALID_SECTOR,
  SECTOR_NOT_BLANK,
  SECTOR_NOT_PREPARED_FOR_WRITE_OPERATION,
  COMPARE_ERROR,
  BUSY,
  PARAM_ERROR,
  ADDR_ERROR,
  ADDR_NOT_MAPPED,
  CMD_LOCKED,
  INVALID_CODE,
  INVALID_BAUD_RATE,
  INVALID_STOP_BIT,
  CODE_READ_PROTECTION_ENABLED
};

// ====================================
// LPC17xx bootloader protocol
// ====================================
// buffer debug print
// ------------------------------------

static void dbg_buf (const char *name, const char *buf, unsigned len) {
unsigned pos;
  printf ("%s[%u]=\"", name, len);
  for (pos = 0; pos < len; pos++) {
    if (buf[pos] >= ' ' && buf[pos] < 0x7F)
      putchar (buf[pos]);
    else
      printf ("<%02X>", buf[pos]);
  }
  printf ("\"\n");
}

// ====================================
// bootloader command
// ------------------------------------

static int send_cmd (COM_HANDLE tty, const char *cmd, const char *reply) {
char rbuf[256];
int i;
  com_flush (tty);
  com_send_buf (tty, (unsigned char*)cmd, strlen (cmd));
#warning "add LF for LPC2xxx..."
  if (verbose >= VERB_COMM)
    dbg_buf ("CMD", cmd, strlen (cmd));
  if (*reply != '!') {
    // cekam na echo prikazu
    i = com_rec_buf (tty, (unsigned char*)rbuf, strlen (cmd), 1000);
    if (i <= 0) {
      printf ("Echo timeout!\n");
      return -101;
    }
    rbuf[i] = 0;
    if (i != strlen (cmd) || strcmp (rbuf, cmd)) {
      if (verbose >= VERB_COMM)
        dbg_buf ("echo", rbuf, i);
      return -102;
    }
  } else {
    reply ++;
  }
  // cekam na odpoved
  i = 0;
  if (*reply) {
    i = com_rec_buf (tty, (unsigned char*)rbuf, strlen (reply), 1000);
    if (i < 0) {
      printf ("Reply timeout!\n");
      return -111;
    }
    rbuf[i] = 0;
    if (verbose >= VERB_COMM)
      dbg_buf ("reply", rbuf, i);
    if (i != strlen (reply) || strcmp (rbuf, reply)) {
/*
      if (verbose >= VERB_COMM) {
        dbg_buf ("req", reply, strlen (reply));
        dbg_buf ("tst", rbuf, strlen (rbuf));
      }
*/
      // convert number from response
      if (sscanf (rbuf, "%d", &i) == 1)
	return -i;
      return -112;
    }
  }
  return i;
}

// ====================================
// send block of data (to RAM)
// ------------------------------------
// 61 chars per line (i.e. 45 bytes of UU-encoded data + '\r'['\n'])
//#define	LINE_LEN	((45*4)/3+3)
// max 20 lines before checksum is sent
//#define	BLOCK_LEN	(45*20)

static int send_block (COM_HANDLE tty, unsigned addr, const unsigned char *data, unsigned len, unsigned waddr) {
char buf[64];
unsigned lnum, pos, uudata, sum;
  if (verbose >= VERB_DEBUG)
    printf ("Block: %08X, %u bytes\n", waddr, len);
  else
  if (verbose >= VERB_INFO) {
    printf ("\r>%X\r", waddr);
    fflush (stdout);
  }


  sprintf (buf, "W %u %u\r", addr, len);
  if (send_cmd (tty, buf, "0\r\n") < 0)
    return -1;
  lnum = pos = uudata = sum = 0;
  while (len) {
    // collect data for uu-encode
    uudata <<= 8;
    uudata |= *data;
    sum += *data++;
    if (!pos) {
      // prepare line header
      buf[0] = (len > 45) ? ' ' + 45 : ' ' + len;
    }
    pos++;
    len--;
    if (!len && (pos & 3) != 3) {
      // align last data
      uudata <<= ((3 - (pos & 3)) * 8);
      pos |= 3;
    }
    if ((pos & 3) == 3) {
      // add UU-encoded bytes
      buf[pos-2] = ' ' + ((uudata >> 18) & 0x3F);
      buf[pos-1] = ' ' + ((uudata >> 12) & 0x3F);
      buf[pos] = ' ' + ((uudata >> 6) & 0x3F);
      buf[pos+1]   = ' ' + ((uudata) & 0x3F);
      pos++;
    }
    if (!len || pos >= 60) {
      buf[61] = '\r';
      buf[62] = 0;
      for (pos = 0; buf[pos]; pos++) {
	if (buf[pos] == 0x20)
	  buf[pos] = 0x60;
      }
      send_cmd (tty, buf, "");
      pos = 0;
      lnum++;
      if (lnum >= 20 || !len) {
	sprintf (buf, "%u\r", sum);
	if (send_cmd (tty, buf, "OK\r\n") < 0)
	  return -2;
	lnum = sum = 0;
	if (verbose >= VERB_INFO) {
	  waddr += 20*45;
	  printf ("\r>%X\r", waddr);
	  fflush (stdout);
	}
      }
    }
  }
  return 0;
}

static int rec_reply (COM_HANDLE tty, unsigned char *buf, unsigned len, unsigned timeout) {
unsigned start = os_tick ();
unsigned idx = 0;
  do {
    int i;
    i = com_rec_byte (tty, 0);
    if (i >= 0) {
      if (i == '\n') {
        if (idx) {
          buf[idx] = 0;
if (verbose >= VERB_COMM) printf ("{time=%lums}", os_tick () - start);
          return idx;
        }
      } else
      if (i != '\r') {
        buf[idx++] = i;
      }
    }
  } while (os_tick () - start < timeout && idx < len);
if (verbose >= VERB_COMM) printf ("{time=%lums}", os_tick () - start);
  if (idx) {
    if (idx == len) idx--;
    buf[idx] = 0;
  }
  return idx;
}

// ====================================
// programming LPC17xx/LPC21xx/LPC213x
// ====================================

int LPC17xx_load (COM_HANDLE tty, const file_data_type *data) {
unsigned char err = 0;
unsigned char cpu_idx = (sizeof (LPC_type) / sizeof (LPC_type[0])) - 1;
unsigned char mem_idx = 0;

  if (!data || !data->data) {
    printf ("Data memory error!\n");
    return -1;
  }

  // frequency check
  if (!cpu_freq)
    cpu_freq = 12000;
  
  // MCU mode selection
  com_set_signal (tty, RST_LOW, 0);
  if (1) {
    if (verbose >= VERB_DEBUG)
      printf ("Connecting to LPC bootloader...\n");
    com_set_signal (tty, BOOT_LOW, 50);
  } else {
    com_set_signal (tty, BOOT_HIGH, 50);
  }
  // MCU reset
  com_set_signal (tty, RST_HIGH, 50);

  do {
    // bootloader initialization
    if (send_cmd (tty, "?", "!Synchronized\r") < 0) {
      err = 101;
      break;
    }
    // LPC needs some time here...
    os_sleep (100);
    // without this delay it often ends with error 102...
    // in verbose mode, printf delay is sufficient
    if (send_cmd (tty, "Synchronized\r", "OK\r\n") < 0) {
      err = 102;
      break;
    }
//    if (send_cmd (tty, "12000\r", "OK\r\n") < 0)
    {
      char freq[32];
      sprintf (freq, "%u\r", cpu_freq);
      if (send_cmd (tty, freq, "OK\r\n") < 0) {
        err = 103;
        break;
      }
    }
    // bootcode version
    if (send_cmd (tty, "K\r", "0\r\n") < 0) {
      err = 161;
      break;
    }
    {
      unsigned char buf[32];
      int i;
      i = rec_reply (tty, buf, 32, 1000);
      if (i < 0) {
        err = 123;
        break;
      }
      buf[i++] = '.';
      i = rec_reply (tty, buf + i, 32, 1000);
      // LPC1768: "1\r\n4\r\n"
      if (i < 0) {
        err = 123;
        break;
      }
      if (verbose >= VERB_INFO)
        dbg_buf ("boot", (char*)buf, strlen ((char*)buf));
    }
    // MCU ID
    if (send_cmd (tty, "J\r", "0\r\n") < 0) {
      err = 161;
      break;
    }
    {
      unsigned char buf[32];
      int i;
      unsigned id;
      i = rec_reply (tty, buf, 32, 3000);
      // LPC1768: "637615927\r\n" ~ dec number
      if (i < 0) {
        err = 123;
        break;
      }
      if (verbose >= VERB_DEBUG)
        dbg_buf ("ID", (char*)buf, i);
      // look for ID in database
      sscanf ((char*)buf, "%u", &id);
      for (i = 0; LPC_type[i].ID; i++)
	if (LPC_type[i].ID == id)
	  break;
      if (!LPC_type[i].ID) {
	printf ("Unknown MCU ID! (%08X)\n", id);
	err = 124;
	break;
      }
      if (verbose >= VERB_INFO)
	printf ("Device: %s\n", LPC_type[i].name);
      cpu_idx = i;
      mem_idx = LPC_type[i].mem_type;
    }

    // unlock bootloader (for Flash write, Erase, Go commands)
    if (send_cmd (tty, "U 23130\r", "0\r\n") < 0) {
      err = 161;
      break;
    }

    // Flash erasing
    if (data->addr >= LPC17xx_FLASH_START && data->addr <= LPC17xx_FLASH_END) {
      unsigned start, end, addr;
      const file_data_type *ptr = data;
      char buf[32];
      int i;
      if (ptr->next != NULL)
        ptr = ptr->next;
      // prepare sector(s)
      // look for starting sector
      start = 0;
      for (i = 0; i < LPC_PAGE_CNT && ptr->addr > LPC_memory[mem_idx].Flash[i].addr + LPC_memory[mem_idx].Flash[i].size * 1024 * LPC_memory[mem_idx].Flash[i].count; i++) {
	start += LPC_memory[mem_idx].Flash[i].count;
      }
      if (i >= LPC_PAGE_CNT) {
	printf ("Flash too small!\n");
	err = 140;
	break;
      }
      end = start; // sector before start is changed
      start += (ptr->addr - LPC_memory[mem_idx].Flash[i].addr) / (LPC_memory[mem_idx].Flash[i].size * 1024);
printf ("Sectors: %08X:%u..", ptr->addr, start);
      // go through data and look for last address in flash
      addr = LPC17xx_FLASH_START; // not needed - just for compiler to prevent warning
      while (ptr && ptr->addr >= LPC17xx_FLASH_START && ptr->addr <= LPC17xx_FLASH_END) {
	addr = ptr->addr + ptr->len - 1; // get last address in block
	ptr = ptr->next;
      }
      // align address to flash region
      if (addr > LPC17xx_FLASH_END)
	addr = LPC17xx_FLASH_END;
      // look for last sector
      while (i < LPC_PAGE_CNT && addr > LPC_memory[mem_idx].Flash[i].addr + LPC_memory[mem_idx].Flash[i].size * 1024 * LPC_memory[mem_idx].Flash[i].count) {
	end += LPC_memory[mem_idx].Flash[i].count;
	i++;
      }
      if (i >= LPC_PAGE_CNT) {
	printf ("Flash too small!\n");
	err = 140;
	break;
      }
      end += (addr - LPC_memory[mem_idx].Flash[i].addr) / (LPC_memory[mem_idx].Flash[i].size * 1024);
printf ("%08X:%u\n", addr, end);
      if (verbose >= VERB_INFO)
	printf ("Erasing flash...\n");
      sprintf (buf, "P %u %u\r", start, end);
      if (send_cmd (tty, buf, "0\r\n") < 0) {
	err = 142;
	break;
      }
      sprintf (buf, "E %u %u\r", start, end);
      if ((i = send_cmd (tty, buf, "0\r\n")) < 0) {
        if (i == -CODE_READ_PROTECTION_ENABLED) {
	  if (verbose >= VERB_INFO)
	    printf ("Flash protected! Full flash erase sent.\n");
	  sprintf (buf, "P 0 %u\r", LPC_memory[mem_idx].Flash[0].count + LPC_memory[mem_idx].Flash[1].count + LPC_memory[mem_idx].Flash[2].count - 1);
	  if (send_cmd (tty, buf, "0\r\n") < 0) {
	    err = 142;
	    break;
	  }
	  sprintf (buf, "E 0 %u\r", LPC_memory[mem_idx].Flash[0].count + LPC_memory[mem_idx].Flash[1].count + LPC_memory[mem_idx].Flash[2].count - 1);
	  if (send_cmd (tty, buf, "0\r\n") < 0) {
	    err = 143;
	    break;
	  }
	}
      }
    }

    // check vector table checksum
    if ((data->next && data->next->len >= 8*4) || (!data->next && data->len > 8*4)) {
      const file_data_type *ptr = data;
      unsigned sum = 0, i;
      if (ptr->next != NULL)
        ptr = ptr->next;
      // this must be in first block
      for (i = 0; i < 8; i++) {
	if ((!(LPC_type[cpu_idx].flags & CORTEX_M) && i == 5) || ((LPC_type[cpu_idx].flags & CORTEX_M) && i == 7))
	  continue;
	sum -= ptr->data[i*4] | (ptr->data[i*4+1] << 8) | (ptr->data[i*4+2] << 16) | (ptr->data[i*4+3] << 24);
      }
      if (!(LPC_type[cpu_idx].flags & CORTEX_M))
	i = 5*4;
      else
	i = 7*4;
      if (sum != (ptr->data[i] | (ptr->data[i+1] << 8) | (ptr->data[i+2] << 16) | (ptr->data[i+3] << 24))) {
	printf ("Vector table checksum incorrect!\nChanging vector table checksum at %08X to value %08X\n", ptr->addr + i, sum);
	ptr->data[i] = sum;
	ptr->data[i+1] = sum >> 8;
	ptr->data[i+2] = sum >> 16;
	ptr->data[i+3] = sum >> 24;
      }
    }

    // MCU programming
    if (verbose >= VERB_INFO)
      printf ("Downloading data...\n");
    {
      const file_data_type *ptr = data;
      if (ptr->next != NULL)
        ptr = ptr->next;
      while (ptr != NULL && !err) {
	if (ptr->addr >= LPC17xx_FLASH_START && ptr->addr <= LPC17xx_FLASH_END) {
	  unsigned pos, sector, sec_idx, sec_addr;
	  // look for first sector
	  sector = sec_idx = 0;
	  for (sec_idx = 0; ptr->addr > LPC_memory[mem_idx].Flash[sec_idx].addr + LPC_memory[mem_idx].Flash[sec_idx].size * 1024 * LPC_memory[mem_idx].Flash[sec_idx].count; sec_idx++) {
	    sector += LPC_memory[mem_idx].Flash[sec_idx].count;
	  }
	  pos = (ptr->addr - LPC_memory[mem_idx].Flash[sec_idx].addr) / (LPC_memory[mem_idx].Flash[sec_idx].size * 1024);
	  sec_addr = LPC_memory[mem_idx].Flash[sec_idx].addr + pos * LPC_memory[mem_idx].Flash[sec_idx].size * 1024;
	  sector += pos;
	  // write data to RAM buffer and copy to Flash
	  for (pos = 0; pos < ptr->len; ) {
	    char buf[32];
	    unsigned len = ptr->len - pos;
	    // limit maximal block length
	    if (len > LPC_memory[mem_idx].flash_block)
	      len = LPC_memory[mem_idx].flash_block;
	    // align address to flash block
//printf ("%08X[%u+%u]\n", ptr->addr, pos, len);
	    if ((ptr->addr + pos + len) > (((ptr->addr + pos) & ~(LPC_memory[mem_idx].Flash[sec_idx].size * 1024 - 1)) + LPC_memory[mem_idx].flash_block)) {
	      len = ((ptr->addr & ~(LPC_memory[mem_idx].Flash[sec_idx].size * 1024 - 1)) + LPC_memory[mem_idx].flash_block) - ptr->addr - pos;
	      if (verbose >= VERB_DEBUG)
		printf ("Aligning flash block size @ %08X to %u bytes...\n", ptr->addr + pos, len);
	    }
	    if (send_block (tty, LPC_memory[mem_idx].RAM_addr + 0x200, ptr->data + pos, len, ptr->addr + pos)) {
	      err = 201;
	      break;
	    }
#warning "last block can contain some data from previous block!"
// consider filling RAM with 0xFF
	    // prepare/unlock Flash for Write
	    sprintf (buf, "P %u %u\r", sector, sector);
	    if (send_cmd (tty, buf, "0\r\n") < 0) {
	      err = 202;
	      break;
	    }
	    // adjust minimal block length
	    if (len <= 512)
	      len = 512;
	    else
	    if (len <= 1024)
	      len = 1024;
	    else
	    if (len <= 4096)
	      len = 4096;
	    else
	      len = 8192;
	    // copy RAM to Flash
	    if (verbose >= VERB_DEBUG)
	      printf ("Move block to flash @ %08X\n", ptr->addr + pos);
	    sprintf (buf, "C %u %u %u\r", ptr->addr + pos, LPC_memory[mem_idx].RAM_addr + 0x200, len);
	    if (send_cmd (tty, buf, "0\r\n") < 0) {
	      err = 203;
	      break;
	    }
	    pos += len;
	    // align sector number
	    if (ptr->addr + pos >= sec_addr + LPC_memory[mem_idx].Flash[sec_idx].size * 1024) {
	      sector++;
	      sec_addr += LPC_memory[mem_idx].Flash[sec_idx].size;
	      if (ptr->addr+ pos >= LPC_memory[mem_idx].Flash[sec_idx].addr + LPC_memory[mem_idx].Flash[sec_idx].size * 1024 * LPC_memory[mem_idx].Flash[sec_idx].count) {
		sec_idx++;
		// not needed - and - can be out of bounds!
		//sec_addr = LPC_memory[mem_idx].Flash[sec_idx].addr;
	      }
	    }
	  }
	} else {
	  // write data directly to RAM
	  if (send_block (tty, ptr->addr, ptr->data, ptr->len, ptr->addr)) {
	    err = 211;
	    break;
	  }
	}
	ptr = ptr->next;
      }
    }
    if (err)
      break;

    // run code
    if (data->addr > LPC17xx_FLASH_END) {
      // code downloaded in RAM
      char buf[16];
      int i;
      sprintf (buf, "G %u T\r", file_run & 0xFFFFFFFE);
      if ((i = send_cmd (tty, buf, "0\r\n")) < 0) {
	if (i == -CODE_READ_PROTECTION_ENABLED && verbose >= VERB_INFO)
	  printf ("Flash protected. Code can't be executed!\n");
        err = 162;
        break;
      }
    } else {
      // code in Flash
      com_set_signal (tty, RST_LOW | BOOT_HIGH, 50);
      com_set_signal (tty, RST_HIGH, 0);
    }
  } while (0);

  // error message
  if (err) {
    printf ("Error %d!\n", err);
    return err;
  }
  if (verbose >= VERB_INFO)
    printf ("Done.     \n");
  return 0;
}

