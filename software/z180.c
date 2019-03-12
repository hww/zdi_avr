// ====================================
// Z180 bootloader
// (C) JAN 2015 Hynek Sladky
// ====================================
/**
Bugs:
====

ToDo:
====

**/
// ====================================
// standard includes
// ------------------------------------
#include <stdio.h>
#include <string.h>
#include "hardware.h"
#include "os.h"
// ------------------------------------
// compile options
// ------------------------------------
//#define	ShowComm
#define	SendBlock
// ------------------------------------
// module locals
// ------------------------------------

// ====================================
// HEX download
// ====================================
int Z180_load (COM_HANDLE tty, const char *name) {
FILE *f;
#define	LINE_LEN	1024
static char line[LINE_LEN];
int i;

  // spusteni bootloaderu
  com_set_signal (tty, RST_LOW, 50);
  // a cekani na vypsani informaci
  com_set_signal (tty, RST_HIGH, 500);
  com_flush (tty);

  // odesilani HEXu
  if ((f = fopen (name, "rt")) == NULL) {
    printf ("Can't open file %s!\n", name);
    return 1;
  }
  while (!feof (f)) {
    if (fgets (line, LINE_LEN, f) == NULL)
      break;
    if (*line != ':')
      continue;
    // odeslani radku
    com_send_buf (tty, (unsigned char*)line, strlen (line));
    // cekani na potvrzeni
    i = com_rec_byte (tty, 200);
    putchar (i);
    fflush (stdout);
  }
  putchar ('\n');
  return 0;
}

