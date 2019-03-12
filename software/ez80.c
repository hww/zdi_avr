// ====================================
// eZ80 bootloader
// (C) FEB 2016 Hynek Sladky
// ====================================
/**
Bugs:
====


ToDo:
====
- ucesat vypisy a chybove hlasky
    dle nastaveni verbose...
  take kontrolu a vraceni chybovych kodu


**/
// ====================================
// standard includes
// ------------------------------------
#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "os.h"
#include "file.h"
#include "load.h"
#include "ez80.h"
// ------------------------------------
// compile options
// ------------------------------------
//#define	ShowComm
#define	SendBlock
// ------------------------------------
// module locals
// ------------------------------------
#define	REG_DEFAULT	((unsigned)-1)
// iFlash default setup: 0x0088 (0x000000-0x01FFFF)
// iRAM: 0xFF80 (0xFFF800-0xFFFFFF)
// CS0: 0x00FFE802 (0x000000/0x020000-0xFFFFFF/0xFFFF7FF)
// CS1..CS3 disabled
unsigned ez_CS[4] = {REG_DEFAULT, REG_DEFAULT, REG_DEFAULT, REG_DEFAULT}, ez_iRAM = REG_DEFAULT, ez_iFLASH = REG_DEFAULT;

// ====================================
// ZDI command
// ====================================
static int ez80_cmd (COM_HANDLE tty, const char *cmd, char *reply) {
unsigned tick;
  if (verbose >= VERB_COMM && cmd)
    printf ("%s\n", cmd);
  tick = os_tick ();
  if (cmd && *cmd)
    com_send_buf (tty, (unsigned char*)cmd, strlen (cmd));
  do {
    int i;
    i = com_rec_byte (tty, 200);
    if (i < 0) {
      if (reply)
	*reply = 0;
      return i;
    }
    if (i == '>')
      break;
    // "eliminate" echo
    if (cmd && *cmd && (*cmd == i || (i == '\n' && *cmd == '\r')))
      cmd++;
    else {
      if (verbose >= VERB_COMM)
	putchar (i);
      if (reply) {
	*reply++ = i;
      }
    }
  } while (1);
  if (reply)
    *reply = 0;
  if (verbose >= VERB_DEBUG) {
    printf ("(executed in %lu msec)\n", os_tick () - tick);
  }
  return 0;
}

static int ez80_hex (COM_HANDLE tty, const char *line) {
int i;
  com_send_buf (tty, (unsigned char*)line, strlen (line));
  // cekani na potvrzeni
  do {
    i = com_rec_byte (tty, 200);
    if (i < 0) // == COM_REC_TIMEOUT
      return i;
    if (verbose >= VERB_COMM) {
      putchar (i);
      fflush (stdout);
    }
  } while (i != '*');
  return 0;
}

// ====================================
// send Intel-HEX file
// ====================================
// ------------------------------------
static int send_hex (COM_HANDLE tty, const file_data_type *data) {
#define	LINE_LEN	256
#define	HEX_LINE	0x40 //0x40
char line[LINE_LEN];
const file_data_type *ptr;
unsigned tick, page;
  // initialize variables
  tick = os_tick ();
  ptr = data;
  if (ptr->next)
    ptr = ptr->next;
  page = -1;
  while (ptr) {
    unsigned pos = 0;
    while (pos < ptr->len) {
      // prepare Intel-HEX line
      if (verbose >= VERB_INFO && verbose < VERB_COMM) {
	printf ("\r%X", ptr->addr + pos);
	fflush (stdout);
      }
      if (((page ^ (ptr->addr + pos)) & 0xFFFF0000)) {
	// send page info
	page = (ptr->addr + pos) & 0xFFFF0000;
	line[LINE_LEN - 2] = page >> 24;
	line[LINE_LEN - 1] = page >> 16;
	make_hex_line (line, 2, 0, 0x04, (unsigned char*)(line + LINE_LEN - 2));
      } else {
	unsigned len = HEX_LINE;
	if (((ptr->addr + pos) & (HEX_LINE - 1)))
	  // align to row boundary
	  len -= (ptr->addr + pos) & (HEX_LINE - 1);
	if (pos + len > ptr->len)
	  len = ptr->len - pos;
	make_hex_line (line, len, ptr->addr + pos, 0x00, ptr->data + pos);
	pos += len;
      }
      // send line
      if (ez80_hex (tty, line)) {
	// timeout or other error
	printf ("Download error!\n");
	return 1;
      }
    }
    ptr = ptr->next;
  }
  if (ez80_hex (tty, ":00000001FF\r")) {
    // timeout or other error
    printf ("Download error!\n");
    return 2;
  }
  if (verbose >= VERB_COMM)
    putchar ('\n');
  else
  if (verbose >= VERB_INFO)
    printf ("\rDownload finished... (%ld msec)\n", os_tick () - tick);
  return 0;
}

// ====================================
// HEX download
// ====================================
int eZ80_load (COM_HANDLE tty, const file_data_type *data) {
#define	REPLY_LEN	256	// Break! message can be quite long...
char reply[REPLY_LEN];
int i;
static const struct {
  unsigned *var;
  const char *prefix;
  struct {
    unsigned short port;
    unsigned char shift;
    const char *name;
  } reg[4];
} reg_def[] = {
  // disable/remap internal Flash
  // FLASH_ADDR_U @ 0x00F7; default=0x00
  // FLASH_CTRL @ 0x00F8; default=0x88; disable=0x80; cp_m=0x28 (1WS)
  {&ez_iFLASH, "FLASH_", {{0x00F7, 8, "ADDR_U"}, {0x00F8, 0, "CTRL"}}},
  // disable/remap internal RAM
  // RAM_CTL @ 0x00B4; default=0x80; disable=0x00
  // RAM_ADDR_U @ 0x00B5; default=0xFF; cp_m=0xB7
  {&ez_iRAM, "RAM_", {{0x00B5, 8, "ADDR_U"}, {0x00B4, 0, "CTL"}}},
  // CS0 for external Flash
  // CS0_LBR @ 0x00A8; default=0x00; cp_m=0x06
  // CS0_UBR @ 0x00A9; default=0xFF; cp_m=0x07
  // CS0_CTL @ 0x00AA; default=0xE8; cp_m=0x20 (7WS en -> 1WS dis)
  // CS0_BMC @ 0x00F0; default=0x02; cp_m=0x01 (eZ80 2cycle -> 1cycle)
  {&ez_CS[0], "CS0_", {{0x00A8, 24, "LBR"}, {0x00A9, 16, "UBR"}, {0x00AA, 8, "CTL"}, {0x00F0, 0, "BMC"}}},
  // set CS1 for external RAM
  // CS1_LBR @ 0x00AB; default=0x00; cp_m=0x06
  // CS1_UBR @ 0x00AC; default=0x00; cp_m=0x07
  // CS1_CTL @ 0x00AD; default=0x00; cp_m=0x28 (-> 1WS en)
  // CS1_BMC @ 0x00F1; default=0x02; cp_m=0x01 (eZ80 2cycle -> 1cycle)
  {&ez_CS[1], "CS1_", {{0x00AB, 24, "LBR"}, {0x00AC, 16, "UBR"}, {0x00AD, 8, "CTL"}, {0x00F1, 0, "BMC"}}},
  // CS2 not used
  // CS2_LBR @ 0x00AE; default=0x00; cp_m=0xFF
  // CS2_UBR @ 0x00AF; default=0x00; cp_m=0xFF
  // CS2_CTL @ 0x00B0; default=0x00; cp_m=0x00
  // CS2_BMC @ 0x00F2; default=0x02; cp_m=0x02 (eZ80 2cycle)
  {&ez_CS[2], "CS2_", {{0x00AE, 24, "LBR"}, {0x00AF, 16, "UBR"}, {0x00B0, 8, "CTL"}, {0x00F2, 0, "BMC"}}},
  // CS3 for I/O
  // CS3_LBR @ 0x00B1; default=0x00; cp_m=0x10
  // CS3_UBR @ 0x00B2; default=0x00; cp_m=0x10
  // CS3_CTL @ 0x00B3; default=0x00; cp_m=0x18 (-> 0WS en I/O)
  // CS3_BMC @ 0x00F3; default=0x02; cp_m=0x42 (ez80 2cycle -> Z80 2cycle)
  {&ez_CS[3], "CS3_", {{0x00B1, 24, "LBR"}, {0x00B2, 16, "UBR"}, {0x00B3, 8, "CTL"}, {0x00F3, 0, "BMC"}}},
  {0}
};

  if (!data || !data->data /*|| data->next*/) {
    printf ("Wrong/unsupported data!\n");
    return 99;
  }

  if (verbose >= VERB_INFO)
    printf ("ZDI initialization...\n");
  // run bootloader
  com_set_signal (tty, RST_LOW | BOOT_LOW, 50);
  // wait for bootloader prompt
  com_set_signal (tty, RST_HIGH | BOOT_LOW, 500);
  com_flush (tty);

  // communication test
  *reply = 0;
  if (ez80_cmd (tty, "ver\r", reply) || strncmp (reply, "ZDI ", 4))
    *reply = 0;
  if (!*reply)
    if (ez80_cmd (tty, "ver\r", reply))
      *reply = 0;
  if (strncmp (reply, "ZDI ", 4)) {
    //if (verbose >= VERB_)
    printf ("Bootloader communication error!\n");
    return 1;
  }
  if (verbose >= VERB_DEBUG)
    printf ("ZDI version: %s", reply + 4);

  if (verbose >= VERB_INFO)
    printf ("eZ80 initialization...\n");
  // stop CPU
  if (ez80_cmd (tty, "stop\r", NULL)) {
    printf ("Can't stop CPU!\n");
    return 2;
  }
  // wait for (possible) Break! message
  os_sleep (150);
  ez80_cmd (tty, NULL, NULL);

  // reset CPU
  if (ez80_cmd (tty, "rst\r", NULL)) {
    printf ("Can't reset CPU!\n");
    return 3;
  }

  // switch to ADL
  if (ez80_cmd (tty, "adl 1\r", NULL)) {
    printf ("Can't switch to ADL!\n");
    return 4;
  }

  // adjust default memory mapping
  if (ez_CS[0] == REG_DEFAULT && (ez_CS[1] != REG_DEFAULT || ez_CS[2] != REG_DEFAULT || ez_CS[3] != REG_DEFAULT)) {
    if (verbose >= VERB_DEBUG)
      printf ("CS0 automatically disabled!\n");
    ez_CS[0] = 0;
  }

  // set memory mapping
  for (i = 0; reg_def[i].var; i++) {
    int j;
    if (*reg_def[i].var == REG_DEFAULT)
      continue;
    for (j = 0; j < 4 && reg_def[i].reg[j].name; j++) {
      unsigned char val;
      val = ((*reg_def[i].var) >> reg_def[i].reg[j].shift);
      if (verbose >= VERB_DEBUG)
	printf ("%s%s=%02X\n", reg_def[i].prefix, reg_def[i].reg[j].name, val);
      sprintf (reply, "out %02X %02X\r", reg_def[i].reg[j].port, val);
      if (ez80_cmd (tty, reply, NULL)) {
	printf ("Can't set register %s%s!\n", reg_def[i].prefix, reg_def[i].reg[j].name);
	return 5;
      }
    }
  }
  if (verbose >= VERB_COMM) {
    // read memory mapping
    if (ez80_cmd (tty, "map\r", NULL)) {
      printf ("Can't check memory mapping!\n");
      return 9;
    }
  }

  // check if internal Flash is enabled for erase...
  if (erase && ez_iFLASH != REG_DEFAULT && !(ez_iFLASH & 0x08)) {
    if (verbose >= VERB_INFO)
      printf ("Internal flash disabled! Erase cannot be executed!\n");
    erase = 0;
  }
  
  // internal Flash management
  if (erase) {
    // Flash mode
    if (verbose >= VERB_INFO)
      printf ("Flash erase/download...\n");
    // setup flash
    // unlock flash divider register
    if (ez80_cmd (tty, "out F5 B6 49\r", NULL)) {
      printf ("Can't unlock flash access!\n");
      return 10;
    }
#define	FLASH_FREQ	196U
    // out F9 <div> = (lpc_freq + 195999) / 196000
    // writing to this register locks it again...
    sprintf (reply, "out F9 %02X\r", (cpu_freq + FLASH_FREQ - 1) / FLASH_FREQ);
    if (ez80_cmd (tty, reply, NULL)) {
      printf ("Can't set flash divider!\n");
      return 11;
    }
    // unlock flash protection register
    if (ez80_cmd (tty, "out F5 B6 49\r", NULL)) {
      printf ("Can't unlock flash access!\n");
      return 12;
    }
    // disable flash protection; writing to this register locks it again...
    sprintf (reply, "out FA 0\r");
    if (ez80_cmd (tty, reply, NULL)) {
      printf ("Can't disable flash protection!\n");
      return 13;
    }
    // erase internal flash
    if (ez80_cmd (tty, "out FF 1\r", NULL)) {
      printf ("Can't start flash erase!\n");
      return 14;
    }
  } else {
    if (verbose >= VERB_INFO)
      printf ("RAM download...\n");
  }
  
  if (prog_flash) {
    // set Write/Program mode
    if (ez80_cmd (tty, "mode 1\r", NULL)) {
      printf ("Can't set write mode!\n");
      return 20;
    }
    // send HEX file
    i = send_hex (tty, data);
    if (i)
      return i;
  }

  if (erase) {
    // unlock flash protection register
    if (ez80_cmd (tty, "out F5 B6 49\r", NULL)) {
      printf ("Can't unlock flash access!\n");
      return 22;
    }
    // enable flash protection; writing to this register locks it again...
    sprintf (reply, "out FA FF\r");
    if (ez80_cmd (tty, reply, NULL)) {
      printf ("Can't enable flash protection!\n");
      return 23;
    }
  }

  // set Verify mode
  if (ez80_cmd (tty, "mode 0\r", NULL)) {
    printf ("Can't set verify mode!\n");
    return 25;
  }

  if (verify) {
    // verify HEX
    if (verbose >= VERB_INFO)
      printf ("HEX verify...\n");
    // send HEX file
    i = send_hex (tty, data);
    if (i)
      return i;
  }

  // switch back to ADL=0 (reset state)
  // reset can't be used here - will reset also CS mapping so RAM debugging wouldn't be possible...
  if (ez80_cmd (tty, "adl 0\r", NULL)) {
    printf ("Can't reset ADL!\n");
    return 40;
  }

  // run application
  if (execute) {
    if (verbose >= VERB_INFO)
      printf ("Running application...\n");
    //if (erase) // just reset CPU
    if (ez80_cmd (tty, "pc 0\r", NULL)) {
      printf ("Can't set PC register!\n");
      return 41;
    }
    if (ez80_cmd (tty, "run\r", NULL)) {
      printf ("Can't run application!\n");
      return 42;
    }
    //com_set_signal (tty, RST_HIGH | BOOT_HIGH, 20);
  }
  // switch terminal to application
  com_set_signal (tty, RST_HIGH | BOOT_HIGH, 20);

  return 0;
}
