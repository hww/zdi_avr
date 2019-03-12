#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "os.h"
#include "file.h"
#include "load.h"
#include "tiva.h"

// ====================================
// constants
// ====================================
#define	COMMAND_RET_SUCCESS	0x40
#define	COMMAND_RET_UNKNOWN_CMD	0x41
#define	COMMAND_RET_INVALID_CMD	0x42
#define	COMMAND_RET_INVALID_ADR	0x43
#define	COMMAND_RET_FLASH_FAIL	0x44

#define	COMMAND_ACK		0xCC
#define	COMMAND_NAK		0x33

#define	COMMAND_PING		0x20
#define	COMMAND_DOWNLOAD	0x21
#define	COMMAND_RUN		0x22 // N.A. for CC13xx
#define	COMMAND_GET_STATUS	0x23
#define	COMMAND_SEND_DATA	0x24
#define	COMMAND_RESET		0x25

// additional commands for CC13xx
#define	COMMAND_SECTOR_ERASE	0x26
#define	COMMAND_CRC32		0x27
#define	COMMAND_GET_CHIP_ID	0x28
#define	COMMAND_MEMORY_READ	0x2A
#define	COMMAND_MEMORY_WRITE	0x2B
#define	COMMAND_BANK_ERASE	0x2C
#define	COMMAND_SET_CCFG	0x2D

// ====================================
// shared variables
// ====================================
//unsigned packet = 76; // TIVA_PACKET_LEN;
unsigned tiva_flags = 0;

static struct {
  unsigned chip_id;
  unsigned flash_size;
  unsigned ram_size;
} tiva_info = {0};

// ====================================
// komunikace s Tiva
// ====================================

static int send_cmd (COM_HANDLE tty, const unsigned char *cmd, int len) {
unsigned size;
unsigned char chk = 0, nak = 0;
  if (len < 0) {len = -len; nak = 1;}
  // send size
  chk = len + 2;
  com_send_buf (tty, &chk, 1);
  if (verbose >= VERB_COMM) printf ("%02X", chk);
  // send checksum
  chk = 0;
  for (size = 0; size < len; size++)
    chk += cmd[size];
  com_send_buf (tty, &chk, 1);
  if (verbose >= VERB_COMM) printf (":%02X", chk);
  // send data
  com_send_buf (tty, cmd, len);
  if (verbose >= VERB_COMM)
    for (size = 0; size < len; size++)
      printf (" %02X", cmd[size]);
  if (!nak) {
    // receive ACK
    //len = recbuf (1000, &chk, 1);
    //if (verbose >= VERB_COMM) printf (">%d:%02X ", len, chk);
    //if (len != 1 || chk != COMMAND_ACK)
//if (cmd[0] == COMMAND_DOWNLOAD) printf ("erase time: %lu..", GetTickCount ());
    do {
      unsigned time = 500;
      if (cmd[0] == COMMAND_DOWNLOAD) {
	// add 64ms per sector(page)
	time += ((cmd[5] << 16) | (cmd[6] << 8) | cmd[7]);
	// time can be from 8ms - 15ms - 40ms - 75ms up to 500ms !!!
      } else
      if (cmd[0] == COMMAND_BANK_ERASE) {
	time += 1000;
      }
      chk = com_rec_byte (tty, time);
//if (cmd[0] == COMMAND_DOWNLOAD && chk == (unsigned char)COM_REC_TIMEOUT) chk = 0;
    } while (!chk);
//if (cmd[0] == COMMAND_DOWNLOAD) printf ("%lu\n", GetTickCount ());
    if (verbose >= VERB_COMM) printf (">%02X\n", chk);
    if (chk != COMMAND_ACK)
      return -2;
  }
  return 0;
}
static int read_cmd (COM_HANDLE tty, unsigned char *buf, int len) {
int i;
unsigned char chk;
static const unsigned char cmd_ack[1] = {COMMAND_ACK}, cmd_nak[1] = {COMMAND_NAK};
  // receive length
  do {
    i = com_rec_byte (tty, 100); // was 500, but it is really long!
  } while (!i);
  if (verbose >= VERB_COMM) printf ("%02X", i);
  // receive checksum
  chk = com_rec_byte (tty, 100);
  if (verbose >= VERB_COMM) printf (":%02X", chk);
  if (i > len+2 || i < 2) {
    com_send_buf (tty, cmd_nak, 1);
    return -i;
  }
  // receive data
  //i = recbuf (1000, buf, len);
  len = i - 2;
  for (i = 0; i < len; i++) {
    chk -= buf[i] = com_rec_byte (tty, 100);
    if (verbose >= VERB_COMM) printf (" %02X", buf[i]);
  }
  com_send_buf (tty, ((chk) ? cmd_nak : cmd_ack), 1);
  if (verbose >= VERB_COMM) printf ("<%02X\n", (chk) ? COMMAND_NAK : COMMAND_ACK);
  return (chk) ? -i : i;
}

// ====================================
// cteni pameti
// ====================================
static int mem_read (COM_HANDLE tty, unsigned addr, unsigned len, unsigned char *buf) {
unsigned char wbuf[16];
int i;
  // send the Read Memory command
  wbuf[0] = COMMAND_MEMORY_READ;
  wbuf[1] = addr >> 24;
  wbuf[2] = addr >> 16;
  wbuf[3] = addr >> 8;
  wbuf[4] = addr;
  wbuf[5] = 0;
  wbuf[6] = len;
  i = send_cmd (tty, wbuf, 7);
  if (i < 0)
    return i;
  i = read_cmd (tty, buf, len);
  return i;
}

// ====================================
// spusteni bootloaderu
// ====================================

static int tiva_reset (COM_HANDLE tty) {
static const unsigned char cmd_ping[1] = {COMMAND_PING};
  // MCU mode selection
  // BOOT = 0: run application from Flash / ROM bootloader
  // BOOT = 1: run Flash bootloader
  if (verbose >= VERB_DEBUG)
    printf ("Connecting to Tiva Flash bootloader...\n");
  com_set_signal (tty, RST_LOW | BOOT_HIGH, 50);
  // MCU reset
  com_set_signal (tty, RST_HIGH | BOOT_HIGH, 50);


  if (tiva_flags & TIVA_ROM_BOOT) {
    if (verbose >= VERB_DEBUG)
      printf ("Switching to Tiva ROM bootloader...\n");
    com_set_signal (tty, RST_HIGH | BOOT_LOW, 0);
    // ROM bootloader is launched after auto-bauding procedure
  }
//  os_sleep (50);
  com_flush (tty);
#if 1
  // auto-bauding
  {
    int i;
    com_send_buf (tty, (unsigned char*)"\x55\x55", 2);
    do {
      i = com_rec_byte (tty, 1000);
    } while (!i);
    if (i != COMMAND_ACK) {
      if (i == COM_REC_TIMEOUT)
        printf ("Error: Autobaud timeout!\n");
      else
        printf ("Error: Autobaud unsuccessful! (%02X)\n", i);
      return 1;
    }
  }
#endif
  // COMMAND_PING
  if (send_cmd (tty, cmd_ping, 1) < 0) {
    printf ("Error: Can't communicate with MCU!\n");
    return 2;
  }
  return 0;
}

static int tiva_boot (COM_HANDLE tty) {
static const unsigned char cmd_id[1] = {COMMAND_GET_CHIP_ID};
unsigned char buf[16];

  if (tiva_reset (tty))
    return 1;

  if (tiva_flags & TIVA_CC13xx) {
    unsigned size;
    int i;
    // COMMAND_GET_CHIP_ID
    if (send_cmd (tty, cmd_id, 1) < 0) {
      printf ("Error: Can't read chip ID!\n");
      return 3;
    }
    if (read_cmd (tty, buf, 4) < 1) {
      printf ("Error: MCU status error!\n");
      return 4;
    }
    tiva_info.chip_id = (buf[0] << 24) | (buf[1] << 18) | (buf[2] << 8) | buf[3];
    if (verbose >= VERB_INFO)
      printf ("Chip ID: %08X\n", tiva_info.chip_id);
    // try to get FLASH size
    size = 512;
    do {
      size <<= 1;
      i = mem_read (tty, TIVA_FLASH_START + size, 1, buf);
    } while (i > 0);
    tiva_info.flash_size = size;
    if (verbose >= VERB_INFO)
      printf ("Chip Flash size: %uKB\n", size >> 10);
    // reset MCU
    if (tiva_reset (tty))
      return 11;
    // try to get RAM size
    size = 0;
    do {
      size += 1024;
      i = mem_read (tty, TIVA_RAM_START + size, 1, buf);
    } while (i > 0);
    tiva_info.ram_size = size;
    if (verbose >= VERB_INFO)
      printf ("Chip RAM size: %uKB\n", size >> 10);
    // reset MCU
    if (tiva_reset (tty))
      return 11;
  } else {
    // TM4C sizes ???
    tiva_info.flash_size = TIVA_FLASH_END - TIVA_FLASH_START + 1;
    tiva_info.ram_size = TIVA_RAM_END - TIVA_RAM_START + 1;
  }
  return 0;
}

// ====================================
// programovani z BIN souboru
// ====================================

int tiva_load (COM_HANDLE tty, const file_data_type *data) {
unsigned long tick;
unsigned char wbuf[256];
const file_data_type *ptr;
static const unsigned char cmd_stat[1] = {COMMAND_GET_STATUS};

  // parameter check
  if (!data || !data->data) {
    printf ("Error: Wrong data!\n");
    return -1;
  }

  // boot parameters
  if (!(tiva_flags & TIVA_CC13xx) && ((data->next && !data->next->addr) || (!data->next && !data->addr)))
    tiva_flags |= TIVA_ROM_BOOT;
  else
    tiva_flags &= ~TIVA_ROM_BOOT;

  // enter bootloader
  if (tiva_boot (tty))
    return 99;

  // Erase flash
  if (erase && (tiva_flags & TIVA_CC13xx)) {
/**
    if (erase > 1) {
      // optimized 4KB sector erase
      if (verbose >= VERB_INFO)
	printf ("Erasing sectors... ");
    } else {
**/
    if (verbose >= VERB_INFO)
      printf ("Erasing device...\n");
    tick = os_tick ();
    wbuf[0] = COMMAND_BANK_ERASE;
    if (send_cmd (tty, wbuf, 1) < 0) {
      printf ("Can't erase flash!\n");
      return 101;
    }
    if (verbose >= VERB_INFO)
      printf ("Erased in %lums\n", os_tick () - tick);
  }
  
  if (prog_flash) {
    // MCU programming
    if (verbose >= VERB_INFO)
      printf ("Programming memory...\n");
    tick = os_tick ();
    ptr = data;
    if (ptr->next)
      ptr = ptr->next;
    while (ptr) {
      unsigned pos = 0;
      // COMMAND_DOWNLOAD
      wbuf[0] = COMMAND_DOWNLOAD;
      wbuf[1] = (unsigned char)(ptr->addr >> 24);
      wbuf[2] = (unsigned char)(ptr->addr >> 16);
      wbuf[3] = (unsigned char)(ptr->addr >> 8);
      wbuf[4] = (unsigned char)ptr->addr;
      wbuf[5] = (unsigned char)(ptr->len >> 24);
      wbuf[6] = (unsigned char)(ptr->len >> 16);
      wbuf[7] = (unsigned char)(ptr->len >> 8);
      wbuf[8] = (unsigned char)ptr->len;
      {
	unsigned long stamp = os_tick ();
	if (send_cmd (tty, wbuf, 9) < 0) {
	  printf ("Error: Can't start programming! (timeout %lums)\n", os_tick () - stamp);
	  return 103;
	}
	stamp = os_tick () - stamp;
	if (ptr->addr < 0x20000000 && verbose >= VERB_INFO && !(tiva_flags & TIVA_CC13xx))
	  printf ("Erase time: %lums\n", stamp);
      }
      // COMMAND_GET_STATUS
      if (send_cmd (tty, cmd_stat, 1) < 0) {
	printf ("Error: Can't get MCU status!\n");
	return 104;
      }
      if (read_cmd (tty, wbuf, 1) < 1) {
	printf ("Error: MCU status error!\n");
	return 105;
      }
      if (wbuf[0] != COMMAND_RET_SUCCESS) {
	printf ("Error: MCU fail! (%02X)\n", wbuf[0]);
	return 106;
      }
      while (pos < ptr->len) {
	unsigned long idx = 1;
	// COMMAND_SEND_DATA
	wbuf[0] = COMMAND_SEND_DATA;
	while (idx <= TIVA_PACKET_LEN && pos < ptr->len)
	  wbuf[idx++] = ptr->data[pos++];
	if (send_cmd (tty, wbuf, idx) < 0) {
	  printf ("Error writing data!\n");
	  return 110;
	}
/**
	  // COMMAND_GET_STATUS
	  wbuf[0] = COMMAND_GET_STATUS;
	  if (send_cmd (tty, wbuf, 1) < 0) {
	    printf ("Can't get MCU status!\n");
	    return 111;
	  }
**/
	if (verbose >= VERB_INFO) {
#warning "Print remaining part from one block only!"
	  printf ("\rRemaining: %u \b", ptr->len - pos);
	  fflush (stdout);
	}
      }
      ptr = ptr->next;
    }
    if (verbose >= VERB_DEBUG)
      printf ("\r\t\t\t\t\t\r");
    if (verbose >= VERB_COMM) putchar ('\n');
    if (verbose >= VERB_INFO)
      printf ("Downloaded in %lums\n", os_tick () - tick);
  }

  if (verify && (tiva_flags & TIVA_CC13xx)) {
    if (verify && verbose >= VERB_INFO)
      printf ("Verifying memory...\n");
    tick = os_tick ();

    ptr = data;
    if (ptr->next)
      ptr = ptr->next;
    while (ptr) {
      unsigned pos = 0;
      while (pos < ptr->len) {
	unsigned size;
	int i;
	// read memory
	size = (pos + TIVA_PACKET_LEN < ptr->len) ? TIVA_PACKET_LEN : ptr->len - pos;
	i = mem_read (tty, ptr->addr + pos, size, wbuf);
	if (i < 0 || i != size) {
	  printf ("Error reading memory!\n");
	  return 140;
	}
	// compare data
	i = 0;
	while (size--) {
	  if (wbuf[i++] != ptr->data[pos++]) {
	    printf ("Error verifying memory!\n");
	    return 141;
	  }
	}
	if (verbose >= VERB_INFO) {
#warning "Print remaining part from one block only!"
	  printf ("\rRemaining: %u \b", ptr->len - pos);
	  fflush (stdout);
	}
      }
      ptr = ptr->next;
    }
    if (verbose >= VERB_DEBUG)
      printf ("\r\t\t\t\t\t\r");
    if (verbose >= VERB_COMM) putchar ('\n');


    if (verbose >= VERB_INFO)
      printf ("Verified in %lums\n", os_tick () - tick);
  }

  // disable bootloader after possible reset
  com_set_signal (tty, BOOT_LOW | RST_HIGH, 50);
  if (execute) {
    if (!(tiva_flags & TIVA_CC13xx)) {
      // COMMAND_RUN -> get address automatically
      unsigned start = file_run;
      if (!start) {
/**
      // load start address from reset vector
  if (!start || start == addr) {
    fseek (f, 4, SEEK_SET);
    fread (&start, 4, 1, f);
  }
**/
	if (data->next)
	  start = data->next->addr;
	else
	  start = data->addr;
      }
      wbuf[0] = COMMAND_RUN;
      wbuf[1] = (unsigned char)(file_run >> 24);
      wbuf[2] = (unsigned char)(file_run >> 16);
      wbuf[3] = (unsigned char)(file_run >> 8);
      wbuf[4] = (unsigned char)file_run;
      if (send_cmd (tty, wbuf, -5) < 0) {
	printf ("Can't run application!\n");
	return 120;
      }
    } else {
      // reset MCU
      com_set_signal (tty, BOOT_LOW | RST_LOW, 50);
      com_set_signal (tty, BOOT_LOW | RST_HIGH, 50);
    }
  }
  return 0;
}

// ====================================
// vycteni procesoru
// ====================================

int tiva_read (COM_HANDLE tty, const char *filename) {
unsigned pos, max_size;
unsigned char wbuf[256];
FILE *f;
//#define	FLASH_SIZE	(128*1024UL)
//#define	ROM_SIZE	0x1CC00
  if (!(tiva_flags & TIVA_CC13xx))
    return 1;

  tiva_flags &= ~TIVA_ROM_BOOT;

  if (tiva_boot (tty))
    return 99;

  // open output file for writing
  if (verbose >= VERB_DEBUG)
    printf ("Creating output file '%s' ... ", filename);
  f = fopen (filename, "wb");
  if (f == NULL) {
    fprintf (stderr, "error: could not create output file\n");
    return 1;
  }
  if (verbose >= VERB_DEBUG)
    printf ("ok\n");

  if (verbose >= VERB_DEBUG)
    printf ("Reading chip @ %08X ", file_offs);

  // load region size limit
  max_size = 0x10000000;
  if (file_offs >= TIVA_FLASH_START && file_offs <= TIVA_FLASH_END)
    max_size = tiva_info.flash_size;
  else
  if (file_offs >= TIVA_RAM_START && file_offs <= TIVA_RAM_END)
    max_size = tiva_info.ram_size;

  // read all memory
  for (pos = 0; pos < max_size; pos += TIVA_PACKET_LEN) {
    int i;
    unsigned size;
    if (verbose >= VERB_INFO)
      if ((pos & 0x03FF) < TIVA_PACKET_LEN)
	printf (".");	// tick mark per 1K to show progress

    // send the Read Memory command
    size = (pos + TIVA_PACKET_LEN > max_size) ? max_size - pos : TIVA_PACKET_LEN;
    i = mem_read (tty, file_offs + pos, size, wbuf);
    if (i < 0) {
      printf ("Error: Can't read memory! (@%08X+%u)\n", file_offs + pos, size);
      return 103;
    }
    // write data to file
    if (!fwrite (wbuf, i, 1, f)) {
      fprintf (stderr, "error: could not write to output file\n");
      return 1;
    }
  }

  if (verbose >= VERB_DEBUG)
    printf (" done\n");
  fclose (f);

  return 0;
}
