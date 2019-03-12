// STM32 boot loader application
// Written by Dale Wheat - 28 January 2008
// rewritten by Hynek Sladky - 2013-2015

// notes:
	// control chip via serial handshaking lines:
	// DTR - reset (pin 4 on PC side, pin 6 on STM3210B-EVAL) - normally -12V (mark) but toggles ~8x during WinXP boot
	// RTS - boot mode
// FT2232H
// - time between command is about 20msec -> too much!!!

#include <stdio.h>
#include <string.h>
//#include <stdlib.h>
//#include <errno.h>
#include "hardware.h"
#include "os.h"
#include "file.h"
#include "load.h"
#include "stm32.h"

// codes used by the STM32 bootloader protocol

#define SYNC	0x7F
#define ACK	0x79
#define NACK	0x1F

// commands used by the STM32 bootloader protocol

//#define	CMD_GET		0x00
#define	CMD_ERASE	0x43
#define	CMD_EXT_ERASE	0x44

// command indexes
enum {
  CMD_IDX_GET,
  CMD_IDX_GET_VERSION_RPS,
  CMD_IDX_GET_ID,
  CMD_IDX_READ_MEMORY,
  CMD_IDX_GO,
  CMD_IDX_WRITE_MEMORY,
  CMD_IDX_ERASE,
  CMD_IDX_WRITE_PROTECT,
  CMD_IDX_WRITE_UNPROTECT,
  CMD_IDX_READOUT_PROTECT,
  CMD_IDX_READOUT_UNPROTECT,
  CMD_IDX_CNT
};

// default commands
static unsigned char CMDs[CMD_IDX_CNT] = {
  0x00,
  0x01,
  0x02,
  0x11,
  0x21,
  0x31,
  0x43,
  0x63,
  0x73,
  0x82,
  0x92
};

#define	STM32_PAGE_TYPE		3

#warning "pridat definici EEPROM oblasti, pripadne i user option..."
// viz utilitka map_scan a vysledky v souboru map_scan
// ale prijde mi, ze nejsou EEPROM a OPTION definovany uplne vsude...
//   tj. vypada to, jako ze jsou nektere MAP soubory nekompletni

// STM32 memory definitions
static const struct {
  unsigned short PID;
  unsigned short family;
  //unsigned char *BID; // bootloader ID
  unsigned int4 BID_addr; // bootloader ID address -> unsigned char
  //unsigned short *mem_size;
  unsigned int4 mem_size_addr; // -> unsigned short
  struct {
    unsigned size;
    unsigned cnt;
  } page[STM32_PAGE_TYPE];
  const char *name;
} MCU_info[] = {
  {0, 0, 0, 0, {{0x800, 0}}, "unknown"}, // default values
//  {0x0418, 1, 0, 0x1FFFF7E0, {{0x800, 32}}, "32F1 ConnLine"},
//  {0x0418, 1, 0, 0x1FFFF7E0, {{0x800, 64}}, "32F1 ConnLine"},
  {0x0418, 1, 0, 0x1FFFF7E0, {{0x800, 128}}, "32F1 ConnLine"},
//  {0x0414, 1, 0, 0x1FFFF7E0, {{0x800, 128}}, "32F1 HighDens"},
//  {0x0414, 1, 0, 0x1FFFF7E0, {{0x800, 192}}, "32F1 HighDens"},
  {0x0414, 1, 0, 0x1FFFF7E0, {{0x800, 256}}, "32F1 HighDens"},
//  {0x0428, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x800, 128}}, "32F1 HighValue"},
//  {0x0428, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x800, 192}}, "32F1 HighValue"},
  {0x0428, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x800, 256}}, "32F1 HighValue"},
//  {0x0412, 1, 0, 0x1FFFF7E0, {{0x400, 16}}, "32F1 LowDend"},
  {0x0412, 1, 0, 0x1FFFF7E0, {{0x400, 32}}, "32F1 LowDend"},
//  {0x0420, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x400, 16}}, "32F1 LowValue"},
//  {0x0420, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x400, 32}}, "32F1 LowValue"},
//  {0x0420, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x400, 64}}, "32F1 LowValue"},
  {0x0420, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x400, 128}}, "32F1 LowValue"},
//  {0x0410, 1, 0, 0x1FFFF7E0, {{0x400, 64}}, "32F1 MedDens"},
  {0x0410, 1, 0, 0x1FFFF7E0, {{0x400, 128}}, "32F1 MedDens"},
//  {0x0430, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x800, 512}}, "32F1 MedValue"},
  {0x0430, 1, 0x1FFFF7D6, 0x1FFFF7E0, {{0x800, 768}}, "32F1 MedValue"},
//[STM32F0]	
//ADDR_FLASH_SIZE=1FFFF7CC;
//  {0x0440, 7, 0x1FFFF7A6, 0, {{0x400, 16}}, "32F0"}, // F0
//  {0x0440, 7, 0x1FFFF7A6, 0, {{0x400, 32}}, "32F0"}, // F0
  {0x0440, 7, 0x1FFFF7A6, 0, {{0x400, 64}}, "32F0"}, // F0
//  {0x0444, 7, 0x1FFFF7A6, 0, {{0x400, 16}}, "32F0 Low"}, // F0
  {0x0444, 7, 0x1FFFF7A6, 0, {{0x400, 32}}, "32F0 Low"}, // F0
//[STM32F2]		
//ADDR_FLASH_SIZE=1FFF7A22;
//  {0x0411, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}}, "32F2"}, // F2
//  {0x0411, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}, {0x20000, 1}}, "32F2"}, // F2
  {0x0411, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}, {0x20000, 7}}, "32F2"}, // F2
//  {0x0422, 7, 0x1FFFF796, 0, {{0x800, 32}}, "32F303"}, // F303
//  {0x0422, 7, 0x1FFFF796, 0, {{0x800, 64}}, "32F303"}, // F303
  {0x0422, 7, 0x1FFFF796, 0, {{0x800, 128}}, "32F303"}, // F303
//  {0x0432, 7, 0x1FFFF7A6, 0, {{0x800, 32}}, "32F373"}, // F373
//  {0x0432, 7, 0x1FFFF7A6, 0, {{0x800, 64}}, "32F373"}, // F373
  {0x0432, 7, 0x1FFFF7A6, 0, {{0x800, 128}}, "32F373"}, // F373
//  {0x0413, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}}, "32F4"}, // F4 - 128KB
//  {0x0413, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}, {0x20000, 1}}, "32F4"}, // F4 - 256KB
//  {0x0413, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}, {0x20000, 3}}, "32F4"}, // F4 - 512KB
  {0x0413, 6, 0x1FFF77DE, 0, {{0x4000, 4}, {0x10000, 1}, {0x20000, 7}}, "32F4"}, // F4 - 1024KB
//  {0x0436, 5, 0x1FF00FFE, 0x1FF8004C, {{0x100, 1024}}, "32L High"},
  {0x0436, 5, 0x1FF00FFE, 0x1FF8004C, {{0x100, 1536}}, "32L High"},
//  {0x0416, 5, 0x1FF00FFE, 0x1FF8004C, {{0x100, 128}}, "32L Med"},
//  {0x0416, 5, 0x1FF00FFE, 0x1FF8004C, {{0x100, 256}}, "32L Med"},
  {0x0416, 5, 0x1FF00FFE, 0x1FF8004C, {{0x100, 512}}, "32L Med"},
  {0x0427, 5, 0x1FF01FFE, 0x1FFFF7E0, {{0x100, 1024}}, "32L Med+"},
// STM32F103 @ STM32F10X-128K-EVAL
//{0x0641-0x0041, 1, ...}
// STM32L051K6 - 64K
//  {0x0417, 5, (unsigned char*)0, (unsigned short*)0, {{0x80, 512}}, "32L0.."},
// STM32L0_x3_x2_x1_64K
  {0x0417, 5, 0x1FF00FFE, 0, {{0x80, 512}}, "32L0_x3_x2_x1"},

  {0}
};

// ====================================
// variables
// ------------------------------------
unsigned stm32_protection = 0;

// ====================================
// sync () - synchronize communication with STM32 bootloader
// ====================================

#define	SYNC_RESET	0
#define	SYNC_RECONNECT	1

static int sync (COM_HANDLE tty, int mode) {
unsigned char buffer[16];
int ack;

  if (mode == SYNC_RESET) {
    // set BOOT pin to BOOT mode
    com_set_signal (tty, BOOT_HIGH | RST_HIGH, 100);
    // reset the chip via the DTR handshaking line
    // note:  DTR clear=-12V, set=+12V
    com_set_signal (tty, RST_LOW, 100);
    // dummy write; MCU is held in reset
    com_send_buf (tty, (unsigned char*)"", 1);
    com_set_signal (tty, RST_HIGH, 100);
    // set BOOT pin for Flash reset
//    com_set_signal (tty, BOOT_LOW | RST_HIGH, 100);
    if (verbose >= VERB_DEBUG)
      printf ("Reset executed...\n");
  } else {
    os_sleep (100);
  }
  // there might be incoming garbage after the chip is first powered on, so clear out the incoming queue
  com_flush (tty);
  // dummy read that is intended to time out
  //com_rec_byte (tty, 0);

  buffer[0] = SYNC;		// SYNC byte
  if (verbose >= VERB_COMM)
    fprintf (stderr, ">%02X", buffer[0]);
  if (!com_send_buf (tty, buffer, 1)) {
    fprintf (stderr, "error: could not write sync byte");
    return (1);
  }
  // get ACK byte from chip... maybe
  ack = com_rec_byte (tty, 150);

#warning "na linuxu se tu musi zkouset jeste druhy pokus!"
// IMHO to je dusledek zapnuti UARTu na STM32 a chybneho vyhodnoceni "break" na lince...?
  if (ack >= 0 && ack != ACK && ack != NACK) {
    if (verbose >= VERB_COMM)
      fprintf (stderr, "<%02X", ack);
    else
    if (verbose >= VERB_DEBUG)
      fprintf (stderr, "warnning: unknown response (response = 0x%02x)\n", (unsigned char) ack);
    ack = com_rec_byte (tty, 100);
  }

  // check number of bytes actually read - from here we can deduce the state of the chip:
  //      if 1 byte received:
  //              if it is "ACK" (0x79) then the chip is sync'd and ready to go
  //              if it is 0x00 then it might be garbage received while chip was being powered up
  //              if it is anything else, then ???
  // if 0 bytes received:
  //              chip/board could be powered off
  //              serial cable could be disconnected
  //              chip is already sync'd...
  //              chip is not in boot loader mode - must have switches set and then chip reset

  if (ack >= 0) {
    if (verbose >= VERB_COMM)
      fprintf (stderr, "<%02X", ack);
    if (ack == ACK) {
      if (verbose >= VERB_DEBUG)
	printf ("Chip sync'd!\n");	// received ACK
    } else
    if (ack == NACK) {
      if (verbose >= VERB_DEBUG)
	printf ("Chip was already sync'd\n");	// received NACK
    } else {
      fprintf (stderr, "error: chip is in unknown state (response = 0x%02x)\n", (unsigned char) ack);
      return (1);
    }
  } else {
    buffer[0] = 0x00;		// dummy byte
    if (verbose >= VERB_COMM)
      fprintf (stderr, ">%02X", buffer[0]);
    if (!com_send_buf (tty, buffer, 1)) {
      fprintf (stderr, "error: could not send dummy byte to test chip synchronization\n");
      return (1);
    }
    ack = com_rec_byte (tty, 50);
    if (ack < 0) {
      fprintf (stderr, "error: could not read ACK byte from chip (2nd attempt)\n");
      return (1);
    }
    if (verbose >= VERB_COMM)
      fprintf (stderr, "<%02X", ack);
    if (ack == ACK) {
      if (verbose >= VERB_DEBUG)
	printf ("Chip sync'd!\n");
    } else
    if (ack == NACK) {
      if (verbose >= VERB_DEBUG)
	printf ("Chip was already sync'd\n");
    } else {
      fprintf (stderr, "error: chip is in unknown state (response = 0x%02x)\n", (unsigned char) ack);
      return (1);
    }
  }
  return 0;
}

// ====================================
// send block of data to STM32 bootloader
// ====================================
static int send_block (COM_HANDLE tty, const unsigned char *data, unsigned len, unsigned sleep, const char *type, const char *cmd) {
unsigned char buf[4];
unsigned tick;
int ack;
  if (data) {
    unsigned char checksum;
    unsigned cnt;
    // data block write
    if (!com_send_buf (tty, data, len)) {
      fprintf (stderr, "error: could not send %s block for %s!\n", type, cmd);
      return -1;
    }
    // checksum
    checksum = (len > 1) ? 0x00 : 0xFF;
    cnt = len;
    while (cnt--) {
      if (verbose >= VERB_COMM)
	fprintf (stderr, ">%02X", *data);
      checksum ^= *data++;
    }
    if (verbose >= VERB_COMM)
      fprintf (stderr, "=%02X", checksum);
    buf[0] = checksum;
    if (!com_send_buf (tty, buf, 1)) {
      fprintf (stderr, "error: could not send %s checksum for %s!\n", type, cmd);
      return -3;
    }
  }

  // check sending time (115200Bd: 25ms, 19200: 147ms)
#warning "tady by to chtelo zmenit..."
  sleep += ((len + 1) * 11000) / 115200; //baudrate;
//  if (sleep + timeouts.ReadTotalTimeoutMultiplier > timeouts.ReadTotalTimeoutConstant) {
  if (sleep + 5 > 50) {
    if (verbose >= VERB_COMM)
      fprintf (stderr, "{%ums}", sleep);
    //os_sleep (sleep);
  }
  // should receive ACK in response
  tick = os_tick ();
  do {
    ack = com_rec_byte (tty, 100); // 50); // 50ms is insufficient for EEPROM write
  } while (os_tick () - tick < sleep && ack < 0);
// --> az sem to je takove podivne...
  if (ack < 0) {
    fprintf (stderr, "error: could not receive %s ACK for %s (%ums)\n", type, cmd, os_tick () - tick);
    return -5;
  }
  if (verbose >= VERB_COMM) {
    fprintf (stderr, "<%02X", ack);
    if ((sleep = os_tick () - tick))
      fprintf (stderr, "{@%ums}", sleep);
  }
  if (ack != ACK) {
    fprintf (stderr, "error: did not receive %s ACK for %s (received 0x%02X)\n", type, cmd, ack);
    return -7;
  }
  return 0;
}  

// ====================================
// send command packet to STM32 bootloader
// ====================================
static int send_packet (COM_HANDLE tty, unsigned char cmd_idx, unsigned long addr, unsigned char *data, unsigned len, char *dbg) {
unsigned char buf[16];
int i, cnt;
  // command
  buf[0] = CMDs[cmd_idx];
  i = send_block (tty, buf, 1, 0, "command", dbg);
  if (i)
    return i;
  if (addr) {
    // address
    buf[0] = addr >> 24;
    buf[1] = addr >> 16;
    buf[2] = addr >> 8;
    buf[3] = addr;
    i = send_block (tty, buf, 4, 0, "address", dbg);
    if (i)
      return i;
  }
  switch (cmd_idx) {
    case CMD_IDX_GET:
    case CMD_IDX_GET_ID:
      // get bootloader version and list of supported commands
      // get device ID
      // read data length
      i = com_rec_byte (tty, 50);
      if (i < 0) {
	fprintf (stderr, "error: could not read byte count in response to %s!\n", dbg);
	return -101;
      }
      if (verbose >= VERB_COMM)
	fprintf (stderr, "<%02X", i);
      // data packet length
      i++;
      goto read_block;
    case CMD_IDX_READ_MEMORY:
      // read memory
      buf[0] = len - 1;
      // read length
      i = send_block (tty, buf, 1, 0, "read length", dbg);
      if (i)
	return i;
      i = len;
      goto read_block;
    case CMD_IDX_GET_VERSION_RPS:
      // get bootloader version and (fake in 2.0) read protection status
      i = 3;
    read_block:
      // read block
      if (i > len) {
	fprintf (stderr, "error: did not receive expected byte count in response to %s: expected %u, received %i\n", dbg, len, i);
	return -103;
      }
      // read remainder of response
      cnt = 0;
//      ack = com_rec_buf (tty, data, i, 50 + len * 5);
      while (i--) {
	int ack;
	ack = com_rec_byte (tty, 50);
	if (ack < 0) {
	  fprintf (stderr, "error: could not read data block for %s!\n", dbg);
	  return (1);
	}
	data[cnt++] = ack;
      }
      if (verbose >= VERB_COMM) {
	for (i = 0; i < cnt; i++)
	  fprintf (stderr, "<%02X", data[i]);
      }
      if (cmd_idx == CMD_IDX_READ_MEMORY) {
	if (verbose >= VERB_COMM)
	  fputc ('\n', stderr);
	return cnt;
      }
      // final ACK
      i = send_block (tty, NULL, 0, 0, "ACK", dbg);
      if (i)
	return i;
      if (verbose >= VERB_COMM)
	fputc ('\n', stderr);
      return cnt;
    case CMD_IDX_WRITE_MEMORY: {
      unsigned sleep = 0;
//      if (addr >= 0x08080000 && addr < 0x10000000)
//	sleep = 100; // timeout for EEPROM write
      i = send_block (tty, data, len, sleep, "data", dbg);
      if (verbose >= VERB_COMM)
	fputc ('\n', stderr);
      return i;
      }
    case CMD_IDX_ERASE: {
      unsigned sleep;
      // STM32F4: word=16..100us; 16K.erase=250..500ms; 128K.erase=1..2s; mass=8..16s
      if (len > 2) {
	if (CMDs[cmd_idx] == CMD_EXT_ERASE)
	  sleep = ((len / 2) - 1) * 2000;
	else
	  sleep = (len - 1) * 2000;
      } else
	sleep = 16000;
      i = send_block (tty, data, len, sleep, "data", dbg);
      if (verbose >= VERB_COMM)
	fputc ('\n', stderr);
      return i;
      }
    case CMD_IDX_GO:
      // go
      // final ACK
      if (verbose >= VERB_COMM)
	fputc ('\n', stderr);
      return 0;
#warning "GO nevrati final ACK..."
      i = send_block (tty, 0, 0, 0, "ACK", dbg);
      return i;
    case CMD_IDX_WRITE_PROTECT:
      // send sector map
      i = send_block (tty, data, len, 0, "data", dbg);
      if (i)
	return i;
#warning "po zpracovani seznamu sektoru se uz dalsi ACK neodesle!" // testovano na STM32L051
      return 0; // proto se dal nepokracuje na prijeti final ACK...
    case CMD_IDX_READOUT_PROTECT:
    case CMD_IDX_READOUT_UNPROTECT:
    case CMD_IDX_WRITE_UNPROTECT:
      // protect/unprotect
      // readout unprotect erases all flash - this can take lot of time!
      // --> see notes for erase command
      i = com_rec_byte (tty, 1500);
      if (i < 0) {
	fprintf (stderr, "error: could not receive second ACK for %s\n", dbg);
	return -5;
      }
      if (verbose >= VERB_COMM)
	fprintf (stderr, "<%02X", i);
      if (i != ACK) {
	fprintf (stderr, "error: did not receive second ACK for %s (received 0x%02X)\n", dbg, i);
	return -5;
      }
      return 0;
  }
  fprintf (stderr, "error: unsupported command index %u!\n", cmd_idx);
  return -109;
}

// ====================================
// initialize bootloader state
// - read bootloader version
// - read supported commands
// - read chip ID
// ====================================
// MCU parameter table index
static unsigned pid_idx = 0;
// communication buffer
static unsigned char buffer[STM32_PACKET_LEN + 32];

static int setup (COM_HANDLE tty) {
// bootloader version
unsigned char boot_ver = 0;
// temp variable
int i, j;
  // send "Get" command - should return bootloader version and supported command list
  i = send_packet (tty, CMD_IDX_GET, 0, buffer, 12, "Get command");
  if (i < 0)
    return 1;
  // interpret bootloader version number
  if (i > 0) {
    boot_ver = buffer[0];
    if (verbose >= VERB_DEBUG)
      printf ("Bootloader version %d.%d detected\n", buffer[0] >> 4, buffer[0] & 0x0F);
  }
  if (boot_ver != 0x20 && boot_ver != 0x30 && boot_ver != 0x31) {
    fprintf (stderr, "error: unsupported bootloader version!\n");
    return 1;
  }
  // supported command list
  for (j = 0; j < i-1 && j < CMD_IDX_CNT; j++)
    CMDs[j] = buffer[j+1];
  if (verbose >= VERB_DEBUG) {
    fprintf (stderr, "Supported commands:\n");
    for (j = 0; j < i-1; j++)
      fprintf (stderr, "idx[%d] %02X\n", j, CMDs[j]);
  }
/**
      case 0x00: printf("\t0x00 Get\n"); break;
      case 0x01: printf("\t0x01 Get Version and Read Protection Status\n"); break;
      case 0x02: printf("\t0x02 Get ID\n"); break;
      case 0x11: printf("\t0x11 Read Memory\n"); break;
      case 0x21: printf("\t0x21 Go\n"); break;
      case 0x31: printf("\t0x31 Write Memory\n"); break;
      case 0x43: printf("\t0x43 Erase\n"); break;
      case 0x63: printf("\t0x63 Write Protect\n"); break;
      case 0x73: printf("\t0x73 Write Unprotect\n"); break;

      case 0x82: printf("\t0x82 Readout Protect\n"); break;
      case 0x92: printf("\t0x92 Readout Unprotect\n"); break;
      default: printf("\t0x%02X Unknown command\n", (unsigned char) buffer[i]); break;
**/  

  // Get Version and 'Read Protection Status' command reports bootloader version and ... nothing else meaningful
  i = send_packet (tty, CMD_IDX_GET_VERSION_RPS, 0, buffer, 3, "Get Version");
  if (i < 0)
    return i;

  // get the bootloader version
  if (buffer[0] != boot_ver) {
    fprintf (stderr, "error: different bootloader versions reported: 0x%02X\n", buffer[0]);
    return 1;
  }
  // get the 'read protection status' which is not implemented in this device
//  if (verbose >= VERB_DEBUG)
//    printf("Read Protection Status: 0x%02X 0x%02X\n", buffer[1], buffer[2]); // always 0, 0

  // Get ID command returns chip ID (same as JTAG ID)
  i = send_packet (tty, CMD_IDX_GET_ID, 0, buffer, 4, "Get ID");
  if (i < 0)
    return i;
  {
    unsigned pid;
    pid = (buffer[0] << 8) | buffer[1];
    if (verbose>= VERB_DEBUG) {
      // report product ID
      printf ("Product ID: %04X", pid);
      if (i >= 4)
        printf ("-%02X%02X", buffer[2], buffer[3]); //??
    }
    // look for device ID in database
    for (i = 1; MCU_info[i].PID; i++) {
      if (pid == MCU_info[i].PID) {
        pid_idx = i;
        break;
      }
    }
    if (verbose>= VERB_DEBUG) {
      if (MCU_info[pid_idx].name)
        printf (": STM%s", MCU_info[pid_idx].name);
      putchar ('\n');
    }
  }
  return 0;
}

// ====================================
// main () - main program function
// ====================================

int stm32_load (COM_HANDLE tty, const file_data_type *data) {
// temporary variable
int i;

  if (!data || !data->data /*|| data->next*/) {
    printf ("Wrong/unsupported data!\n");
    return 1;
  }

  // reset & sync the chip
  i = sync (tty, SYNC_RESET);
  if (i) {
#if 0
    // try again
#warning "na linuxu se tu musi zkouset jeste druhy pokus, ale bez resetu...!"
// na windows 7 jsem podobny problem nezaznamenal...
    i = sync (tty, SYNC_RECONNECT);
    if (i)
#endif
      return i;
  }

  i = setup (tty);
  if (i)
    return i;

  // get readout protection state
  i = (stm32_protection & STM32_READ_UNPROTECT) ? -7 : send_packet (tty, CMD_IDX_READ_MEMORY, STM32_FLASH_START, buffer, 8, "Read Memory");
  if (i == -7) {
    // unprotect flash memory
    if (!(stm32_protection & STM32_READ_UNPROTECT))
      printf ("Readout protection detected! Memory automatically unprotected!\n");
    else
    if (verbose >= VERB_INFO)
      printf ("Flash readout unprotection...\n");
    i = send_packet (tty, CMD_IDX_READOUT_UNPROTECT, 0, 0, 0, "Readout Unprotect");
    if (i < 0)
      return i;
    // Reset pulse is driven from MCU itself - can be seen on RST signal!
    // re-sync communication
    i = sync (tty, SYNC_RECONNECT);
    if (i)
      return i;
    if (erase) {
      printf ("Erase disabled after read-out unprotecting...\n");
      erase = 0;
    }
  } else
  if (i > 0) {
    if (!erase && data->addr >= STM32_FLASH_START && data->addr <= STM32_FLASH_END) {
      printf ("Forcing optimized erase...\n");
      erase = 2;
    }
  }

  if (stm32_protection & STM32_WRITE_UNPROTECT) {
    if (verbose >= VERB_DEBUG)
      printf ("Flash write unprotection...\n");
    i = send_packet (tty, CMD_IDX_WRITE_UNPROTECT, 0, 0, 0, "Write Unprotect");
    if (i < 0)
      return i;
    // Reset pulse is driven from MCU itself - can be seen on RST signal!
    // re-sync communication
    i = sync (tty, SYNC_RECONNECT);
    if (i)
      return i;
  }

  if (erase == 1) {
    // send Erase command
    if (verbose >= VERB_INFO)
      printf ("Erasing device... ");
    // send erase page count (0xFF = infinity)
    if (CMDs[CMD_IDX_ERASE] != CMD_EXT_ERASE) {
      i = send_packet (tty, CMD_IDX_ERASE, 0, (unsigned char*)"\xFF", 1, "Erase");
    } else {
      if (verbose >= VERB_DEBUG)
	fprintf (stderr, "(using Extended Erase command) ");
      i = send_packet (tty, CMD_IDX_ERASE, 0, (unsigned char*)"\xFF\xFE", 2, "ExtErase all");
    }
    if (i < 0)
      return 1;
    if (verbose >= VERB_INFO)
      printf ("ok\n");
  } else
  if (erase > 1) {	// optimized erase based on file size
    unsigned page, pages;
    int j;
    if (verbose >= VERB_INFO)
      printf ("Erasing sectors... ");
    // build list of pages that need to be erased
    //pages = (file_size / MCU_info[pid_idx].page_size) + 1;	// the actual number of 4K pages that need to be erased
    //pages = (file_size / MCU_info[pid_idx].page[0].size) + 1;
    pages = 0;
    //j = data->len;
    j = file_block_size (data, STM32_FLASH_START, STM32_FLASH_END, &page);

#warning "pokud se nebude nahravat od zacatku Flash, tak to nebude fungovat spravne!!"
// viz promenna base_address > FLASH_START -> podle toho se urci prvni sektor...
// zatim to zarovnam od zacatku Flash... IMHO by bylo zbytecne programovat jen cast Flash, kdyz na zacatku jsou vektory...
    if (page > STM32_FLASH_START)
      j += page - STM32_FLASH_START;

    for (i = 0; i < STM32_PAGE_TYPE; i++) {
      int cnt;
      if (!MCU_info[pid_idx].page[i].size)
	break;
      cnt = ((j + MCU_info[pid_idx].page[0].size - 1) / MCU_info[pid_idx].page[i].size);
      if (cnt > MCU_info[pid_idx].page[i].cnt)
	cnt = MCU_info[pid_idx].page[i].cnt;
      pages += cnt;
      cnt *= MCU_info[pid_idx].page[i].size;
      if (j > cnt)
	j -= cnt;
      else
	j = 0;
    }
    if (j > 0) {
      printf ("File size overflow by %u bytes!\n", j);
    }
    if (verbose >= VERB_DEBUG)
      printf ("- %u sectors will be erased... ", pages);

#if 1
    page = 0;
    while (page < pages) {
      if (verbose >= VERB_DEBUG)
	printf ("[%u.", page);
      if (CMDs[CMD_IDX_ERASE] != CMD_EXT_ERASE) {
	if (verbose >= VERB_DEBUG)
	  printf ("Erasing pages %u", page);
	j = pages - page;
	if (j >= STM32_PACKET_LEN)
	  j = STM32_PACKET_LEN;
	buffer[0] = j - 1;
	for (j = 0; j <= buffer[0]; j++)
	  buffer[j+1] = page++;
	j++;
      } else {
	j = pages - page;
	if (j >= ((STM32_PACKET_LEN - 2) >> 1))
	  j = (STM32_PACKET_LEN - 2) >> 1;
	buffer[0] = (j - 1) >> 8;
	buffer[1] = (j - 1);
	for (j = 0; j <= buffer[1]; j++) {
	  buffer[2+2*j] = page >> 8;
	  buffer[3+2*j] = page++;
	}
	j = 2 + j * 2;
      }
      if (verbose >= VERB_DEBUG)
	printf (".%u] ", page - 1);
      i = send_packet (tty, CMD_IDX_ERASE, 0, buffer, j, "ErasePages");
      if (i < 0)
	return i;
    }
#else
#warning "original posila cisla bloku po 10..." // takze neposila jeden obrovsky paket... navic staci kratsi cekani na ACK...
// hlavne v MAP souborech je maximalni delka paketu omezena na 128 bytu (0x80)
// original omezuje delku dat pro programovani na 256 (0xFF)
// FILES_GetMemoryMapping + SetPaketSize
// po volani techto funkci se nastavi na pozadovanou delku...
    if (CMDs[CMD_IDX_ERASE] != CMD_EXT_ERASE) {
      buffer[0] = pages - 1;	// number of pages that need to be erased - 1
      //printf("[file size = %i, pages to erase = %i]\n", file_size, pages); // *** debug ***
      for (page = 0; page < pages; page++) {
	//printf("[erase page %i]\n", page); // *** debug ***
	buffer[page + 1] = page;
      }
      //printf("[checksum = 0x%02X]\n", checksum); // *** debug ***
      page = pages + 1;
    } else {
      buffer[0] = (pages - 1) >> 8;	// number of pages that need to be erased - 1
      buffer[1] = (pages - 1);
      //printf("[file size = %i, pages to erase = %i]\n", file_size, pages); // *** debug ***
// jak se resi smazani cele flash pres sektory, to netusim...
// ale IMHO tam je omezeni na delku dat posilanych do bootloaderu
      for (page = 0; page < pages; page++) {
	//printf("[erase page %i]\n", page); // *** debug ***
	buffer[(page + 1) << 1] = page >> 8;
	buffer[((page + 1) << 1) + 1] = page;
      }
      //printf("[checksum = 0x%02X]\n", checksum); // *** debug ***
      page = (pages << 1) + 2;
    }
// printf ("erase: 0..%u\n", pages-1); return 0;
    //for(i = 0; i < (pages + 2); i++) { printf("buffer[%i] = 0x%02X\n", i, buffer[i]); } // *** debug ***
    //printf("[sending %i bytes]\n", pages + 2); // *** debug ***
    if (verbose >= VERB_INFO)
      printf ("Erasing device... ");
    i = send_packet (tty, CMD_IDX_ERASE, 0, buffer, page, "ErasePages");
    if (i < 0)
      return i;
#endif
    if (verbose >= VERB_INFO)
      printf ("ok\n");
  }

  {
    unsigned tick;
    const file_data_type *ptr;

    // write to chip
    if (prog_flash && verbose >= VERB_INFO)
      printf ("Writing memory...\n");
    if (verify && verbose >= VERB_INFO)
      printf ("Verifying memory...\n");
    // initialize variables
    tick = os_tick ();
    ptr = data;
    if (ptr->next)
      ptr = ptr->next;

    while (ptr) {
      if (prog_flash) {
	unsigned pos;
	pos = 0;	// initialize byte counter
	if (verbose >= VERB_DEBUG)
	  printf ("Writing memory @ %08X - %u bytes...\n", ptr->addr, ptr->len);
	while (pos < ptr->len) {
	  int j;
	  if (verbose >= VERB_DEBUG) {
	    printf ("[writing to 0x%08X - %u bytes]  \r", ptr->addr + pos, ptr->len - pos); // *** debug ***
	    fflush (stdout);
	  } else
	  if (verbose >= VERB_INFO) {
	    printf (">%08X\r", ptr->addr + pos);
	    fflush (stdout);
	  }
	  // send Write Memory command
	  j = (ptr->len - pos > STM32_PACKET_LEN) ? STM32_PACKET_LEN : ptr->len - pos;	// a maximum of 256 bytes, or however many bytes are remaining to be sent
	  buffer[0] = j - 1;
	  memcpy (buffer + 1, ptr->data + pos, j);
	  i = send_packet (tty, CMD_IDX_WRITE_MEMORY, ptr->addr + pos, buffer, j+1, "Write Memory");
	  if (i < 0)
	    return i;
	  
	  // end-of-loop calculations
	  pos += j;
	}
      }

      // check if option area was modified
      if (ptr->addr >= STM32_SYSTEM_START && ptr->addr <= STM32_SYSTEM_END) {
	// re-sync communication
	if (verbose >= VERB_DEBUG)
	putchar ('\n');
	// Reset pulse is driven from MCU itself - can be seen on RST signal!
	i = sync (tty, SYNC_RECONNECT);
	if (i)
	  return i;
	// check if RDP is active
	if (verify) {
	  // try to read flash
	  i = send_packet (tty, CMD_IDX_READ_MEMORY, STM32_FLASH_START, buffer, 8, "Read Memory");
	  if (i == -7) {
	    printf ("Readout protection is active! Disabling further verification...\n");
	    verify = 0;
	    stm32_protection |= STM32_READ_PROTECTED;
	  }
	}
      }

      if (verify) {
	unsigned pos;
	pos = 0;	// initialize byte counter
	if (verbose >= VERB_DEBUG)
	  printf ("Verifying memory @ %08X - %u bytes...\n", ptr->addr, ptr->len);
	while (pos < ptr->len) {
	  int j;
	  if (verbose >= VERB_DEBUG) {
	    printf("[verifying to 0x%08X - %u bytes]  \r", ptr->addr + pos, ptr->len - pos); // *** debug ***
	    fflush (stdout);
	  } else
	  if (verbose >= VERB_INFO) {
	    printf (">%08X\r", ptr->addr + pos);
	    fflush (stdout);
	  }
	  // send the Read Memory command
	  j = (ptr->len - pos > STM32_PACKET_LEN) ? STM32_PACKET_LEN : ptr->len - pos;	// a maximum of 256 bytes, or however many bytes are remaining to be verified
	  i = send_packet (tty, CMD_IDX_READ_MEMORY, ptr->addr + pos, buffer, j, "Read Memory");
	  if (i < 0)
	    return i;
	  // compare memory and file contents
	  //buffer[0] = 0x55; // rig it to fail
	  if (memcmp (buffer, ptr->data + pos, j) != 0) {
	    fprintf (stderr, "error: verify error (%08X)\n", ptr->addr + pos);
	    return 1;
	  }
	  // end-of-loop calculations
	  pos += j;
	}
      }
      ptr = ptr->next;
    }
    if (verbose >= VERB_INFO)
      printf ("\nDownload finished... (%ld msec)\n", os_tick () - tick);
    else
    if (verbose >= VERB_INFO)
      printf ("OK.        \n");

    //printf("[sent %i bytes]\n", address.value - FLASH_START); // *** debug ***
  }

//verbose = VERB_COMM;
//com_set_signal (tty, BOOT_LOW, 0);
  if (stm32_protection & STM32_WRITE_PROTECT) {
    int j;
    if (verbose >= VERB_DEBUG)
      printf ("Flash write protection...\n");
    // prepare write protection sector list
#warning "dokoncit tento kod!!"
// IMHO je jenodussi resit toto pres data v HEX!
    buffer[0] = 0;
    buffer[1] = 0;
    j = 2;
    i = send_packet (tty, CMD_IDX_WRITE_PROTECT, 0, buffer, j, "Write Protect");
    if (i < 0)
      return i;
#warning "tady nedojde k synchronizaci komunikace!!"
// nicmene reset probehne - je to videt na signalu RST i na TxD vystupu
    // re-sync communication
    i = sync (tty, SYNC_RECONNECT);
    if (i)
      return i;
  }
  if (stm32_protection & STM32_READ_PROTECT) {
    if (verbose >= VERB_DEBUG)
      printf ("Flash readout protection...\n");
    i = send_packet (tty, CMD_IDX_READOUT_PROTECT, 0, 0, 0, "Readout Protect");
    if (i < 0)
      return i;
    // Reset pulse is driven from MCU itself - can be seen on RST signal!
    // re-sync communication
    i = sync (tty, SYNC_RECONNECT);
    if (i)
      return i;
  }

  if (execute) {
    unsigned addr;
//    const file_data_type *ptr;
    addr = file_offs;
    if (!addr)
      addr = data->addr;
    if (verbose >= VERB_INFO)
      printf ("Executing newly downloaded code... ");
    if (verbose >= VERB_DEBUG)
      printf ("[vectors @ %08X]", addr);
#if 0
    // load reset vector value
    ptr = data;
    if (ptr->next)
      ptr = ptr->next;
    while (ptr) {
      if (addr >= ptr->addr && addr + 3 <= ptr->addr + ptr->len) {
        addr -= ptr->addr;
        addr = *(unsigned long*)(ptr->data + addr);
        break;
      }
      ptr = ptr->next;
    }
    if (!ptr) {
      fprintf (stderr, "error: can't find boot vector @ %08X\n", addr);
      return 1;
    }
    if (verbose >= VERB_DEBUG)
      printf ("[reset vector = %08X]", addr);
#endif
    if (verbose >= VERB_INFO)
      putchar ('\n');
    // set BOOT pin for Flash reset
    com_set_signal (tty, BOOT_LOW | RST_HIGH, 100);
    // send the Go command
    i = send_packet (tty, CMD_IDX_GO, addr, NULL, 0, "Go"); // needs vector table address
    if (i < 0)
      return i;
  } else {
    // set BOOT pin for Flash reset
    com_set_signal (tty, BOOT_LOW | RST_LOW, 100);
    com_set_signal (tty, RST_HIGH, 0);
    if (verbose >= VERB_INFO)
      printf ("MCU released from reset...\n");
    if (stm32_protection & (STM32_READ_PROTECTED | STM32_READ_PROTECT))
      printf ("Read protection was activated.\nPower-on reset required to successfully start new firmware!\n");
  }
  return 0;			// done
}

int stm32_read (COM_HANDLE tty, const char *filename) {
// FLASH, SRAM memory size in KB
unsigned int flash_kb, sram_kb, pos;
// temp variable
int i;
// output binary file
FILE *f;

  // reset & sync the chip
  i = sync (tty, SYNC_RESET);
  if (i)
    return i;

  i = setup (tty);
  if (i)
    return i;

/**
  // values for STM32L152VD
  flash_kb = 384;
  sram_kb = 48;
  // values for STM32F407
  flash_kb = 512;
  sram_kb = 128-8;
**/

  // default memory sizes
  flash_kb = 0;
  for (i = 0; i < STM32_PAGE_TYPE; i++)
    flash_kb += (MCU_info[pid_idx].page[i].size * MCU_info[pid_idx].page[i].cnt) >> 10;
  sram_kb = 0xFFFF;
 
//#warning "opravit zjistovani velikosti pameti!"
  // now read & report memory sizes
  // 0x1FFFF7E0, 0x1FFFF7E1 (2 bytes) = FLASH in Kbytes (yes, it is)
  // 0x1FFFF7E2, 0x1FFFF7E3 (2 Bytes) = SRAM in Kbytes (not, it isn't)

  if (verbose >= VERB_DEBUG)
    printf ("Reading memory size... ");
  if (MCU_info[pid_idx].mem_size_addr) {
    // send Read Memory command @ information section
    i = send_packet (tty, CMD_IDX_READ_MEMORY, MCU_info[pid_idx].mem_size_addr, buffer, 4, "Read Memory");
    if (i > 0) {
      if (verbose >= VERB_DEBUG)
	printf ("ok\n");
      if (i > 0) {
	flash_kb = (buffer[1] << 8) + buffer[0];
	sram_kb = (buffer[3] << 8) + buffer[2];
      }
    } else {
      if (verbose >= VERB_DEBUG)
	printf ("using default values\n");
    }
  } else {
    if (verbose >= VERB_DEBUG)
      printf ("no memory info for this MCU\n");
  }

  if (sram_kb == 0xFFFF) {
    if (verbose >= VERB_DEBUG)
      printf ("Calculating default RAM size...\n");
    // Rev A silicon - assuming performance line parts
    if (flash_kb <= 32)
      sram_kb = 16;
    else
    if (flash_kb <= 128)
      sram_kb = 32;
    else
    if (flash_kb <= 256)
      sram_kb = 64;
    else
      sram_kb = 128;
  }

  if (verbose >= VERB_DEBUG) {
    printf ("Flash memory size: %i KB\n", flash_kb);
    printf ("SRAM memory size:  %i KB\n", sram_kb);
  }

  if (!filename || !*filename) {
    fprintf (stderr, "error: output filename not specified!\n");
    return 1;
  }
  // read file from chip and write to file

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
    printf ("Reading chip ");

  for (pos = 0; pos < (flash_kb * 1024); pos += STM32_PACKET_LEN) {
    if (verbose >= VERB_INFO)
      if ((pos & 0x03FF) == 0x00)
	printf (".");	// tick mark per 1K to show progress

    // send the Read Memory command
    i = send_packet (tty, CMD_IDX_READ_MEMORY, STM32_FLASH_START + (pos * STM32_PACKET_LEN), buffer, STM32_PACKET_LEN, "Read Memory");
    if (i < 0)
      return 1;
    // write data to file
    if (!fwrite (buffer, i, 1, f)) {
      fprintf (stderr, "error: could not write to output file\n");
      return 1;
    }
  }

  if (verbose >= VERB_DEBUG)
    printf (" done\n");
  fclose (f);
  return 0;
}
