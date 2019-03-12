#include <stdio.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include <fcntl.h> // O_RDWR ...
#include <unistd.h> // usleep
#include <sys/ioctl.h> // TIOCM_
#include <time.h> // time...
#include <dirent.h> // directory...

#include "com.h"

//char com_port[256] = {0}; //"/dev/ttyACM0";
char *com_port = NULL; 

unsigned usb_vid = 0x16D5; //0;
unsigned usb_pid = 0x0002; //0;
//char usb_serial[256] = {0}; //"000002";
char *usb_serial = NULL;

#if 1
static const unsigned long baud_spec[][2] = {
  {B50, 50},
  {B75, 75},
  {B110, 110},
  {B134, 134},
  {B150, 150},
  {B200, 200},
  {B300, 300},
  {B600, 600},
  {B1200, 1200},
  {B1800, 1800},
  {B2400, 2400},
  {B4800, 4800},
  {B9600, 9600},
  {B19200, 19200},
  {B38400, 38400},
  {B57600, 57600},
  {B115200, 115200},
  {B230400, 230400},
  {B460800, 460800},
  {B500000, 500000},
  {B576000, 576000},
  {B921600, 921600},
  {B1000000, 1000000},
  {B1152000, 1152000},
  {B1500000, 1500000},
  {B2000000, 2000000},
  {B2500000, 2500000},
  {B3000000, 3000000},
  {B3500000, 3500000},
  {B4000000, 4000000},
  {0}
};
#endif

COM_HANDLE com_open_port (const char *name, unsigned long speed, unsigned parity) {
COM_HANDLE tty;
struct termios setup;
#if 1
unsigned u;
  if (!name || !*name)
    return -1;
  // check speed
  for (u = 0; baud_spec[u][0]; u++) {
    if (baud_spec[u][1] == speed)
      break;
  }
  if (!baud_spec[u][0])
    return -1;
  speed = baud_spec[u][0];
#endif
  // open serial port
  tty = open (name, O_RDWR | O_NOCTTY | O_SYNC);
  if (tty < 0) {
    fprintf (stderr, "Error %d opening %s: %s\n", errno, name, strerror (errno));
    return tty;
  }
  // get current setting
  if (tcgetattr (tty, &setup)) {
    fprintf (stderr, "Error %d reading TTY attributes\n", errno);
    return -1;
  }
//  cfsetospeed (&setup, speed);
//  cfsetispeed (&setup, speed);
  cfsetspeed (&setup, speed);
  // disable BREAK processing
//  setup.c_iflag &= ~IGNBRK;
  setup.c_lflag = 0;
  setup.c_oflag = 0;
  setup.c_cc[VMIN] = 0; // non-blocking
  setup.c_cc[VTIME] = 0; // x100ms read timeout
#warning "tady by to asi chtelo nastavit to, co je potreba misto nulovani toho co neni potreba..."
  setup.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | INLCR | IGNCR);
  setup.c_cflag |= (CLOCAL | CREAD);
  setup.c_cflag &= ~(PARENB | PARODD);
  if (parity) {
    setup.c_cflag |= PARENB;
    if ((parity & 1))
      setup.c_cflag |= PARODD;
//    if (parity > 2)
//      setup.c_cflag |= CMSPAR;
  }
  setup.c_cflag &= ~CSTOPB;
  setup.c_cflag &= ~CRTSCTS;
  if (tcsetattr (tty, TCSANOW, &setup)) {
    fprintf (stderr, "Error %d writing TTY attributes", errno);
    return -1;
  }
  return tty;
}

void com_speed (COM_HANDLE tty, unsigned long speed, unsigned parity) {
struct termios setup;
int signal;
  // get DTR/RTS state
  ioctl (tty, TIOCMGET, &signal);
  // get current setting
  if (tcgetattr (tty, &setup)) {
    fprintf (stderr, "Error %d reading TTY attributes\n", errno);
    return;
  }
#if 0
printf ("Cflg=%08X\n", setup.c_cflag);
printf ("Iflg=%08X\n", setup.c_iflag);
printf ("Lflg=%08X\n", setup.c_lflag);
printf ("Oflg=%08X\n", setup.c_oflag);
printf ("Ispd=%08X\n", setup.c_ispeed);
printf ("Ospd=%08X\n", setup.c_ospeed);
#endif
//  cfsetospeed (&setup, speed);
//  cfsetispeed (&setup, speed);
  cfsetspeed (&setup, speed);
  setup.c_cflag &= ~(PARENB | PARODD);
  if (parity) {
    setup.c_cflag |= PARENB;
    if ((parity & 1))
      setup.c_cflag |= PARODD;
//    if (parity > 2)
//      setup.c_cflag |= CMSPAR;
  }
#if 0
printf ("Cflg=%08X\n", setup.c_cflag);
printf ("Iflg=%08X\n", setup.c_iflag);
printf ("Lflg=%08X\n", setup.c_lflag);
printf ("Oflg=%08X\n", setup.c_oflag);
printf ("Ispd=%08X\n", setup.c_ispeed);
printf ("Ospd=%08X\n", setup.c_ospeed);
#endif
  if (tcsetattr (tty, TCSANOW, &setup)) {
    fprintf (stderr, "Error %d writing TTY attributes", errno);
    return;
  }
  // set DTR/RTS state
  ioctl (tty, TIOCMSET, &signal);
  return;


}
unsigned com_set_signal (COM_HANDLE tty, unsigned signal, unsigned delay) {
int stat;
  ioctl (tty, TIOCMGET, &stat);
  if (signal & COM_RTS_CLR)
    stat &= ~TIOCM_RTS;
  else
  if (signal & COM_RTS_SET)
    stat |= TIOCM_RTS;
  else
  if (signal & COM_RTS_INV) {
    stat ^= TIOCM_RTS;
    if (stat & TIOCM_RTS)
      signal |= COM_RTS_SET;
    else
      signal |= COM_RTS_CLR;
  }
  if (signal & COM_DTR_CLR)
    stat &= ~TIOCM_DTR;
  else
  if (signal & COM_DTR_SET)
    stat |= TIOCM_DTR;
  else
  if (signal & COM_DTR_INV) {
    stat ^= TIOCM_DTR;
    if (stat & TIOCM_DTR)
      signal |= COM_DTR_SET;
    else
      signal |= COM_DTR_CLR;
  }
  ioctl (tty, TIOCMSET, &stat);
// pro delsi casy je asi vhodnejsi pouzivat msleep nebo lepe msleep_interruptible
// usleep si zapne hrtimers, msleep pouziva task scheduler
// ale nejak to (msleep) nechce vzit linker...
  if (delay)
    usleep (delay * 1000);
  return signal;
}

int com_send_buf (COM_HANDLE tty, const unsigned char *buf, unsigned len) {
  return write (tty, buf, len);
}

int com_rec_byte (COM_HANDLE tty, unsigned timeout) {
int status;
char buf;
unsigned long start = os_tick ();
  do {
    status = read (tty, &buf, sizeof (buf));
    if (status > 0)
      return (unsigned char)buf;
  } while (os_tick () - start < timeout);
  return COM_REC_TIMEOUT;
}

int com_rec_buf (COM_HANDLE tty, unsigned char *buf, unsigned len, unsigned timeout) {
unsigned long start = os_tick ();
unsigned pos = 0;
  do {
    int status;
#if 0
    status = read (tty, buf + pos, 1);
    if (status > 0)
      pos++;
#else
    status = com_rec_byte (tty, timeout - os_tick () + start);
    if (status >= 0)
      buf[pos++] = status;
#endif
  } while (pos < len && (os_tick () - start) < timeout);
  if (!pos) {
    printf ("[%lums]", os_tick () - start);
    return COM_REC_TIMEOUT;
  }
  return pos;
}

void com_flush (COM_HANDLE tty) {
  tcflush (tty, TCIOFLUSH);
}

void com_close (COM_HANDLE tty) {
  close (tty);
}

unsigned long os_tick (void) {
struct timespec ts;
  if (clock_gettime (CLOCK_MONOTONIC, &ts) != 0)
    return 0;
  return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

void os_sleep (unsigned long msec) {
  usleep (msec * 1000);
}

void os_time (TIME_TYPE *tm) {
time_t t;
  t = time (NULL);
  *tm = *localtime (&t);
}

// ====================================
// find tty port for selected VID/PID/serial
// based on http://stuge.se/find430.sh
// plus http://stackoverflow.com/questions/4204666/how-to-list-files-in-a-directory-in-a-c-program
// ------------------------------------

int com_find_usb (char *name, unsigned short vid, unsigned short pid, const char *serial) {
char path[256] = "/sys/bus/usb/devices/", *str;
char line[256];
DIR *d;
struct dirent *dir;
  // go through /sys/bus/usb/devices/
  d = opendir (path);
  if (!d)
    return -1;
  for (str = path; *str; str++);
  if (name)
    *name = 0;
  while ((dir = readdir (d))) {
    // check uevent file for PRODUCT=VID/PID[/<this_is_not_serial!!!>]
    FILE *f;
    DIR *sd;
    struct dirent *sdir;
    //printf ("name: %s\n", dir->d_name);
    sprintf (str, "%s/uevent", dir->d_name);
    if (!(f = fopen (path, "rt")))
      continue;
    sprintf (str, "PRODUCT=%x/%x", vid, pid);
    if (!vid && !pid && !name)
      printf ("USB connection: %s\n", dir->d_name);
    while (!feof (f)) {
      if (!fgets (line, 256, f))
        break;
      //printf ("%s", line);
      if (!strncmp (line, "PRODUCT=", 8)) {
        if (!strncmp (line, str, strlen (str))) {
          if (!name)
            printf ("VID/PID found.\n");
          break;
        }
        if (!vid && !pid && !name) {
          printf ("%s", line);
          break;
        }
      }
      *line = 0;
    }
    fclose (f);
    if (!*line)
      continue;
    // VID/PID has been found

#if 0
    // how to check serial ??
    // /sys/bus/usb/devices/%s/serial
// problem je, se serial je v adresari zarizeni, kdezto tty je v adresari (asi) interface nebo ceho....
// tj. /sys/bus/usb/devices/2-1.1/serial ale /sys/bus/usb/devices/2-1.1:1.0/ttyUSB0
// nebo /sys/bus/usb/devices/2-1.2/serial ale /sys/bus/usb/devices/2-1.2:1.0/tty/ttyACM0
// takze pokud najdu VID/PID/serial, tak nenajdu tty
// a pokud najdu tty, tak tam neni serial...
// takze bych to musel udelat nejak vic sofistikovane nebo se na serial vykaslat...
// slo by hledat stale hloubeji...
// /sys/bus/usb/devices/2-1.2/serial
// /sys/bus/usb/devices/2-1.2/2-1.2:1.0/tty/ttyACM0
//   jenze to by znamenalo nekolikrat otvirat adresar pro hledani... takze "nejak reentrantni" nebo neco podobneho...
//   ono by to slo: mit vnorene smycky s dalsimi lokalnimi promennymi a znova a znova zkuset jit dal...
//   jenze ten plochy model, kdy se projde jen jedna uroven adresare, je mnohem prijatelnejsi... :-)
// system jmen je nasledujici:
//   bus-port.port.port...port:config.interface
// mozna by to slo opacne: tam, kde najdu tty, tak jeste potvrdim serial
//   tj. v nazvu adresare najdu ':', pak jdu zpet az budu mit nazev bez ':' a doplnim serial...





    sprintf (str, "%s/serial", dir->d_name);
    if ((f = fopen (path, "rt"))) {
      if (!fgets (line+200, 56, f))
        line[200] = 0;
      fclose (f);
      if (serial && *serial && (vid || pid))
        if (strncmp (line+200, usb_serial, strlen (serial)))
          continue;
    } else {
      line[200] = 0;
    }



#else
#warning "kontrola serial...?"
#endif

    // look for tty subdirectory (CDC) or ttyUSB%u/tty/ttyUSB%u (CP2102) or...?
    // not really sure if another serial converters have another path to lookk for...
    sprintf (str, "%s/tty/", dir->d_name);
    // try to open tty
    if (!(sd = opendir (path))) {
      // tty not found; open parent directory and look for tty*
      sprintf (str, "%s", dir->d_name);
      if (!(sd = opendir (path)))
        continue;
    }
    // look for tty*
    while ((sdir = readdir (sd))) {
      if (*sdir->d_name == '.')
        continue;
      if (strncmp (sdir->d_name, "tty", 3))
        continue;
      if (!sdir->d_name[3])
        continue;
      if (!name || (!vid && !pid)) {
        if (!vid && !pid)
          printf ("%s", line);
        printf ("TTY port found: %s\n", sdir->d_name);
      }
      if (name)
        sprintf (name, "/dev/%s", sdir->d_name);
      if (vid || pid)
        break;
    }
    closedir (sd);
    // check USB serial if requested
    if (serial && *serial) {
      int i;
      for (i = 0; str[i]; i++);
      while (i && str[i] != ':') i--;
      if (str[i] == ':') {
        strcpy (str+i, "/serial");
//printf ("file: %s\n", path);
        i = 0;
        if ((f = fopen (path, "rt"))) {
          if (fgets (line, 256, f)) {
//printf ("serial: \"%s\"(%s)\n", line, serial);
            if (!strncmp (serial, line, strlen (serial)))
              i = 1;
          }
          fclose (f);
        }
      }
      if (!i)
        *name = 0;
    }
  }
  closedir (d);
  if (name)
    if (*name)
      return 0;
  return -1;
}

