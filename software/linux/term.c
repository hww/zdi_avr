#include <termios.h>
#include <stdio.h>
#include <unistd.h> // close
#include <fcntl.h> // O_RDWR ...
#include <sys/ioctl.h>
//#include <errno.h>
#include <string.h>

#include "term.h"

static struct termios con_old;
static int con_id = 0;

TERM_HANDLE term_open (void) {
TERM_HANDLE input;
struct termios con_new;
  input = fopen("/dev/tty", "r");      //open the terminal keyboard
//  output = fopen("/dev/tty", "w");     //open the terminal screen

  con_id = open ("/dev/tty", O_RDWR | O_NOCTTY | O_NONBLOCK);
  tcgetattr (con_id, &con_old); // save current port settings   //so commands are interpreted right for this program
  memcpy (&con_new, &con_old, sizeof (con_new));
#if 0
  con_new.c_cflag = B115200 | CRTSCTS | CS8 | CLOCAL | CREAD;
  con_new.c_iflag = IGNPAR;
  con_new.c_oflag = 0; //ONLCR
  con_new.c_lflag = 0;       //ICANON;
#endif
#if 0
printf ("Cflg=%08X\n", con_new.c_cflag);
printf ("Iflg=%08X\n", con_new.c_iflag);
printf ("Lflg=%08X\n", con_new.c_lflag);
printf ("Oflg=%08X\n", con_new.c_oflag);
printf ("Ispd=%08X\n", con_new.c_ispeed);
printf ("Ospd=%08X\n", con_new.c_ospeed);
#endif

/**
// default values
Cflg=0x00004BF = 002277 = HUPCL | CREAD | CS8 | B38400;
Iflg=0x0006D02 = 066402 = IUTF8 | IMAXBEL | IXANY | IXON | ICRNL | BRKINT;
Lflg=0x0008A3B = 105073 = 0100000 | ECHOKE | ECHOPRT | ECHOK | ECHOE | ECHO | ISIG | ICANON;
Oflg=0x0000005 = 000005 = ONLCR | OPOST;
Ispd=0x000000F = 000017 = B38400;
Ospd=0x000000F = 000017 = B38400;
**/

  // ICANON = 1: waiting for user input until Enter is pressed
  // ECHO* = 1: echoing input on output
  // ISIG = 1: enabling ^C processing
  con_new.c_lflag = 0; //ISIG; //&= ~(ICANON);
  con_new.c_iflag &= ~ICRNL;
  con_new.c_cc[VMIN]=0;
  con_new.c_cc[VTIME]=0;
  tcflush (con_id, TCIFLUSH);
  tcsetattr (con_id, TCSANOW, &con_new);

  return input;
}

int term_read (TERM_HANDLE con) {
int status;
char buf;
  status = fread (&buf, 1, 1, con); // toto je stale blokujici...
//    status = read (con_id, buf, 1); // takto nefunguje (nebo nejak divne) stdout
  if (!status)
    return 0;
  return (unsigned char)buf;
}

void term_close (TERM_HANDLE con) {
  if (con != NULL)
    fclose (con);
  if (con_id > 0) {
    tcsetattr (con_id, TCSANOW, &con_old);
    close (con_id);
    con_id = 0;
  }
}

void term_title (const char *title) {
  if (!title || !*title)
    title = "Terminal";
  //term_title
  printf ("\033]0;%s\007", title);
}
