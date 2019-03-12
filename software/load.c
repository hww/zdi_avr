//#include "global.h"
#include "hardware.h"
#include "os.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "args.h"
#include "file.h"
#include "load.h"
// MCU libraries
#include "msp430.h"
#include "tiva.h"
#include "stm32.h"
#include "lpc17xx.h"
//#include "lpc21xx.h"
#include "z180.h"
#include "ez80.h"


/**
Bugs:
====
- taky jsem nekde musel davat CRLF...
  podivat se, jak to kde funguje a kde nefunguje...
  no, uz to zas nejak funguje...
  jen jsem daval preklad jeste do terminalu...
- nefunguje fast rezim pro MSP430
  pri zmene rychlosti com_speed se aktivuje RST!!
http://www.embeddedlinux.org.cn/EmbLinux/ch06lev1sec2.htm
  zminujou tam info->MCR, ale tykka se to embedded linuxu na konkretnim HW...
  
https://github.com/MSP-EricLoeffler/MSPBSL_Library
https://pythonhosted.org/python-msp430-tools/commandline_tools.html
https://code.google.com/p/propgcc/source/browse/loader/src/osint_linux.c
http://forums.parallax.com/archive/index.php/t-143533.html
  tady to je zajimave - nekdo shani neco pro Qt co nebude menit DTR/RTS
  ale nejak jsem tam nenasel nic pro me... jen ze po dvou letech 20012->2014 se objevila nova knihovna...
  pisou neco o NOHUP, 2x otevreni portu...
  mozna zkusit drzeni stavu pri zavreni portu...?
  zminuji tam simpleIDE-0-8-5 terminal nebo gtkterm
http://comments.gmane.org/gmane.linux.usb.general/60663
  tady je zminka o PL2303
  driver by mel sahat na DTR jen pri zmene z B0
  treba se to tyka "jen" CDC ??
https://www.winehq.org/pipermail/wine-patches/2004-April/010558.html
  tady je zminka tykajici se opravy SetCommState u Wine...
  viz dll/kernel:comm.c
  - vola se funkce DeviceIoControl ?? parametr je win handle... viz winbase.h
  - kernel32/vxd.c
  - ... nevim kudy to putuje
  - nakonec je volani tcsetattr v souboru dlls/ntdll/serial.c
    konecne tu je funkce set_baud_rate, tak ji musim prostudovat...
    no, nic jsem tu nenasel... zatim...
    set_baud_rate se vola pres io_control, kde je parametr IOCTL_SERIAL_SET_BAUD_RATE, ten se taky pouziva ve funkci SetCommState...
    krome _SET_BAUD_RATE se nastavuje _SET_LINE_CONTROL, _SET_HANDFLOW a _SET_CHARS
  - EscapeCommFunction
    pouzivaji se konstanty IOCTL_SERIAL_CLR_ a _SET_, ktere zas konci v io_control funkci
  - cele to na mne pusobi tak, ze se sice signaly DTR a RTS na chvili zmeni, ale pak se vratii zpet...
    v seriove komunikaci to asi nevadi, ale pri programovani to vadi

- MSP430 obsahuje tento radek v HEXu
:0400000300001100E8, coz je start segment address 0000:1100
  je otazka, jak to vypada pro MCU s vetsi pameti (MSP430X)

- reload funkce
  pokud se detekuje zmena zrovna behem zapisu, tak se konci s chybou "Intel HEX format error!"
  je to IMHO malo pravdepodobne, ale stalo se mi to...

ToDo:
====
- podpora pro eZ80
  x nutne vyresit prepinani rychlosti bootloader vs. terminal
    pozor! linux pri prepnuti rychlosti aktivuje RST signal !!
    -> IMHO: baud udava rychlost pro terminal; bootloader to ma uvnitr napevno...
  o dale by bylo potreba vyresit nastaveni pinu BOOT pro terminal
    ale s moznosti uzivatelske zmeny... aby slo spustit terminal pro rezim bootloaderu i aplikace
    tj. k tomu asi bude potreba, aby terminal umel aktivovat take RST pin, aby vubec mohl spustit bootloader...
      asi by sla udelat tabulka, ktera toto bude popisovat...
      LPC:L; MSP430:pulsy dle typu; STM32:H; Tiva/CC13:nastavitelne/zvolene H;Z180:n.u.;eZ80:?
      kdyz to vidim, tak to nebude az tak jednoduche...
    po pripojeni CDC jsou vsechny signaly v H
- moznost zmenit ovladani ^R, ^X, ^B apod.
o moznost nastavit datum/cas v zarizeni automaticky
  tj. --date "date %D.%M.%YY" --time "time %H:%M:%S"
  pro RF ctecku asi bude nutne take zadat nejaky cas zpozdeni... i kdyz by to treba mohlo fungovat i ihned...?
- nejde pustit jen ERASE
  ani kdyz zadam -P=0 a file.hex
  -> prog_flash se totiz nastavuje podle dalsich parametru...
  workaround: erase.hex s jedinym radkem
- sledovani zmeny HEX souboru
  pokud se HEX zmeni, provede se automaticky znovu spusteni a nahrati aplikace
  i kdyz to je trochu nebezpecne - pokud si delam jen kontrolni preklad...
  tak mozna jen info o zmene HEX a nejaka kombinace pro reload (F5 nebo ^-neco?)
o snizovani zateze systemu...
  pridani os_delay
  ale nevim, zda to je takto rozumne
  -> asi by bylo dobre se podivat v praci na terminal, kde jsem to uz resil...
o logovani + flushovani
  sledovani USB pripojeni a uzavreni spojeni pri odpojeni
- vystup ve formatu hex nebo half-hex (non-printable...)

- vyuziti voleb reset, erase, flash, verify, run u jednotlivych procesoru
  optimized erase bude uz specificky pro STM32, keep info A bude pro MSP430 apod...
  hlavne bude potreba udelat automaticke mazani
- moznost vycteni pameti u dalsich procesoru
  (zatim je jen STM32)

- mozna by se mohla zmenit filosofie ovladani
  promenna load_func = FUNC_PROG;
  erase bude automaticke pred programovanim flash
  maximalne bude mozne zmenit erase_all/erase_optimized (lze i u MSP430 - tam je potreba keep_info)
  pripadne povolit verify
  load_func se muze nastavit na FUNC_ERASE nebo FUNC_VERIFY nebo FUNC_READ, kdy se provede pouze smazani/verifikace/vycteni
  - pak je otazka, jak nejlepe zadat modifikace (verify po programovani nebo erase_optimized/keep_infoA)
- proc tu jsou volby file_offs a file_run ??
  navic by to slo spojit s volbou execute...
  default by ale muselo byt -1
  IMHO file_run se nacita z HEX souboru a mel by byt nastaveny na file_offs
  file_offs by se mozna mohl nastavit podle HEX na data->addr (nejnizsi adresa)
  je otazka, jak by se mel program chovat pri pouziti HEX a zadani file_run, file_offs z prikazove radky
  a jak to cele propojit s volbou execute...
  -> tj. zas to sjednotit napric jednotlivymi typy procesoru... execute & file_run

- chtelo by to nejake automaticke nastaveni nekterych parametru (treba parita) dle vybraneho MCU
  o uz jsem udelal automaticke nastaveni flash_prog a execute, pokud se zada nazev a neni pozadovano vycteni
  - file_offs by se melo nastavit na zacatek Flash
  o parita podle typu MCU (STM32; MSP430 si to nastavuje samo... protoze tam se zacina na jine nez nastavene rychlosti...)

- podpora LPC21xx
- vyresit platformne nezavisle adresovani COM portu...
  tj. win: COM1..9 \\.\COM%d pro >9
  ale i linux /dev/tty%d nebo /dev/ttyACM0 apod.
- hledani USB portu
http://stackoverflow.com/questions/4192974/how-to-find-which-device-is-attached-to-a-usb-serial-port-in-linux-using-c
https://www.rfc1149.net/blog/2013/03/05/what-is-the-difference-between-devttyusbx-and-devttyacmx/

**/

// ====================================
// definice argumentu programu
// ====================================
// shared parameters
// ------------------------------------
int verbose = VERB_INFO;

// ====================================
// local parameters
// ------------------------------------
static int help = 0;

static unsigned long baud = -1;//115200;
static unsigned parity = 0;
static int boot_term = -1, rst_term = 0;

//int reset_before = 0;
int read_out = 0;
int erase = 0; // krome STM32 se nikde nepouziva!
//int blank_check = 0; // nikde se nepouziva
int prog_flash = 0; // opet pouze STM32...
int verify = 0; // pouze STM32, MSP430, eZ80...
int reload = 0;
int cpu_freq = 0;

#warning "chtelo by to sjednotit pouziti erase/verify/prog_flasg/read_out..."

//int reset_after = 0;
int execute = 0;
int terminal = 0;

static char *file_name = NULL;

static char *cmds = NULL;

enum {MCU_NONE, MCU_TIVA, MCU_STM32, MCU_LPC17xx, MCU_LPC21xx, MCU_CC13xx, MCU_MSP430, MCU_Z180, MCU_EZ80, MCU_CNT};
static int MCU = MCU_NONE;

static char *log_name = NULL;

enum {KEY_RESET, KEY_BOOT, KEY_EXIT, KEY_LOAD, KEY_CNT};
static char ctl_key[KEY_CNT] = {0x12, 0x02, 0x18, 0x0C};

const static ARGS args[] = {
  {ARGS_OPTINTEGER, "-?", 0, &help, 1, ""},
  {ARGS_OPTINTEGER, "--help", 0, &help, 1, "print this help"},

  ARGS_COM_PARAMS

  {ARGS_OPTINTEGER, "--verbose", 0, &verbose, 1, "print debug information"},
//  {ARGS_OPTINTEGER, "-R", 0, &execute, 0, ""},
//  {ARGS_OPTINTEGER, "--reset", 0, &execute, 0, "print debug information"},
  {ARGS_OPTINTEGER, "--read", 0, &read_out, 1, "read out flash data"},
  {ARGS_OPTINTEGER, "-E", 0, &erase, 1, ""},
  {ARGS_OPTINTEGER, "--erase", 0, &erase, 1, "erase flash"},
  {ARGS_OPTINTEGER, "-P", 0, &prog_flash, 1, ""},
  {ARGS_OPTINTEGER, "--program", 0, &prog_flash, 1, "program flash"},
//  {ARGS_OPTINTEGER, "-C", 0, &blank_check, 1, ""},
//  {ARGS_OPTINTEGER, "--check", 0, &blank_check, 1, "blank-check erased flash"},
  {ARGS_OPTINTEGER, "-V", 0, &verify, 1, ""},
  {ARGS_OPTINTEGER, "--verify", 0, &verify, 1, "verify downloaded file"},
  {ARGS_OPTINTEGER, "-X", 0, &execute, 1, ""},
  {ARGS_OPTINTEGER, "--execute", 0, &execute, 1, "execute downloaded file"},
  {ARGS_OPTINTEGER, "-R", 0, &reload, 1, ""},
  {ARGS_OPTINTEGER, "--reload", 0, &reload, 1, "check for file modification & automatically reload"},
  // programming options/functions
  {ARGS_OPTINTEGER, "-T", 0, &terminal, 1, ""},
  {ARGS_OPTINTEGER, "--term", 0, &terminal, 1, "runs terminal after successful programming"},
//  {ARGS_OPTINTEGER, "--termonly", 0, &mode, MODE_TERM, "runs terminal only"},
//  {ARGS_INTEGER, "-B", 0, &baud, 1, ""},
  {ARGS_INTEGER, "--baud", 0, &baud, 1, "set baudrate; default %dBd"},
  {ARGS_CHOOSE, "--parity", "none/odd/even/mark/space", &parity, 4, "set parity; default %s"},
  {ARGS_INTEGER, "--boot", 0, &boot_term, 1, "set BOOT/RTS pin state for terminal"},
  {ARGS_INTEGER, "--rst", 0, &rst_term, 1, "activate RST/DTR pin before terminal"},
  {ARGS_INTEGER, "-F", "<freq>", &cpu_freq, 1, ""},
  {ARGS_INTEGER, "--frequency", "<freq>", &cpu_freq, 1, "set MCU oscillator frequency; default: %dkHz"},

//  {ARGS_INTEGER, "-A", 0, &addr, 1, ""},
  {ARGS_STRING, "--log", 0, &log_name, 1, "LOG filename"},
//  {ARGS_STRING, "--log-append", 0, &log_name, 1, "LOG filename"},

  {ARGS_STRING, "--command", 0, &cmds, 1, "commands to execute; \\n and \\r are supported; %%D, %%M, %%Y or %%YY, %%h, %%m, %%s are replaced with current date or time; _ is replaced with space"},

  ARGS_FILE_PARAMS

  {ARGS_CHOOSE, "--mcu", "-/tiva/stm32/lpc17xx/lpc21xx/cc13xx/msp430/z180/ez80", &MCU, 8, "select MCU; default %s"},

  ARGS_TIVA_PARAMS
  
  ARGS_STM32_PARAMS

  ARGS_MSP430_PARAMS
  
  ARGS_LPC17XX_PARAMS
  
  ARGS_Z180_PARAMS

  ARGS_EZ80_PARAMS

  {ARGS_SWITCH, "-", 0, &help, 1, NULL},
  {ARGS_STRING, "", 0, &file_name, 1, "<file>\tHEX filename"},
  {0}
};

#if 0
// default communication parameters for each supported MCU
static const struct {
  unsigned long speed;
  unsigned char parity;
} mcu_setup[7] = {
  {0, 0},
  {115200, 0}, // Tiva
  {115200, 2}, // STM32
  {115200, 0}, // LPC17xx
  {115200, 0}, // LPC21xx
  {9600, 2}, // MSP430
  {57600, 0}, // Z180
};
#endif

// ====================================
// hlavni funkce
// ====================================

int main (int argc, char *argv[]) {
COM_HANDLE hCom;
unsigned long time;
TIME_TYPE sysTime;
  if (verbose > VERB_NONE)
    printf ("This is MCU downloader for debug; (c) 2015-2016 Hynek Sladky\n");

  if (args_parse_args (argc, argv, args, ":=#") || help || !MCU) {
    printf ("Usage: %s [options] file\n", argv[0]);
    args_print_help (args);
    return 0;
  }
  // parameter check & default presets
  // if readout is not requested and filename is entered, programming is automatically set
  if (!read_out && file_name && *file_name)
    prog_flash = /*execute =*/ 1;
  if (MCU == MCU_STM32 /*|| MCU == MCU_MSP430*/) {
    if (!parity)
      parity = COM_PAR_EVEN;
    //if (!erase && addr_in_flash...) erase = 2;
  }
  if (baud == (unsigned long)(-1)) {
    if ((MCU == MCU_Z180 || MCU == MCU_EZ80))
      baud = 57600;
    else
      baud = 115200;
  }
  if (file_offs && !file_run)
    file_run = file_offs;

  // look for USB port
  if ((usb_vid || usb_pid) && (!com_port || !*com_port)) {
    com_port = malloc (128); // this is never free'd !! as args never are !!
    if (com_port)
      *com_port = 0;
#if 0
//com_find_usb (NULL, 0, 0, NULL);
//com_find_usb (com_port, 0, 0, NULL);
//com_find_usb (NULL, 0x16D5, 0x0002, NULL);
com_find_usb (com_port, 0x16D5, 0x0002, NULL);
//com_find_usb (com_port, 0x16D5, 0x0002, "000002");
//com_find_usb (com_port, 0x10C4, 0xEA60, NULL);
printf ("port: %s\n", com_port);
return 0;
#endif
    if (com_find_usb (com_port, usb_vid, usb_pid, usb_serial)) {
      printf ("Can't find USB device!\n");
      return 1;
    }
    if (verbose >= VERB_INFO)
      printf ("USB port found: " COM_PORT_FMT "\n", com_port);
  }

  // open serial port
  hCom = com_open_port (com_port, baud, parity);
  if (COM_INVALID (hCom)) {
    printf ("Error opening " COM_PORT_FMT " port!\n", com_port);
    return 1;
  }

  // set console title
  //term_title
//  printf ("\033]0;MCUload @ "COM_PORT_FMT "\007", com_port);

  do {
    os_time (&sysTime);
    printf ("\n[%02d:%02d:%02d] MCUload started.\n", sysTime.TM_HOUR, sysTime.TM_MIN, sysTime.TM_SEC);

    time = os_tick ();

    if (read_out) {
      int err = 0;
      if (!file_name || !*file_name) {
	printf ("Filename is not entered!\n");
	com_close (hCom);
	return 1;
      }
      // vycteni MCU
      switch (MCU) {
	case MCU_STM32:
	  err = stm32_read (hCom, file_name);
	  break;
	case MCU_CC13xx:
	  tiva_flags |= TIVA_CC13xx;
	  err = tiva_read (hCom, file_name);
	  break;
      }
      if (verbose >= VERB_INFO)
	printf ("Done in %lums.\n", os_tick () - time);
      com_close (hCom);
      return err;
    }

    if (file_name && *file_name) {
      int err = 0;
      file_data_type *data;
      // nacteni souboru
      data = file_load (file_name);
      if (data == NULL) {
//	printf ("Can't load file %s!\n", file_name);
	com_close (hCom);
	return 2;
      }
      if (verbose == 11) {
	printf ("Data check finished.\n");
	if (data != NULL)
	  file_free (data);
	return 0;
      }
      // programovani MCU
      switch (MCU) {
	case MCU_TIVA:
	  tiva_flags &= ~TIVA_CC13xx;
	  err = tiva_load (hCom, data);
	  break;
	case MCU_CC13xx:
	  tiva_flags |= TIVA_CC13xx;
	  err = tiva_load (hCom, data);
	  break;
	case MCU_STM32:
	  err = stm32_load (hCom, data);
	  break;
	case MCU_MSP430:
	  err = msp430_load (hCom, data, argv[0]);
	  if (!err) {
	    // reset
	    com_set_signal (hCom, RST_LOW, 50);
	    // changing baudrate/parity
	    com_speed (hCom, baud, parity);
	    com_set_signal (hCom, RST_HIGH, 0);
	  }
	  break;
	case MCU_Z180:
	  err = Z180_load (hCom, file_name);
	  break;
	case MCU_EZ80:
	  err = eZ80_load (hCom, data);
#if 0
	  if (!err) {
	    // switch to ZDI to avoid application reset
	    com_set_signal (hCom, BOOT_LOW, 50);
	    // this generates reset activation in Linux
	    com_speed (hCom, baud, parity);
	    // deactivate reset...
	    com_set_signal (hCom, RST_HIGH | BOOT_LOW, 50);
	  }
#endif
	  break;
	case MCU_LPC17xx:
	  err = LPC17xx_load (hCom, data);
	  break;
      }
      file_free (data);
      if (err)
	return 2;
      if (verbose >= VERB_INFO)
	printf ("Done in %lums.\n", os_tick () - time);
    } else { // if (MCU != MCU_NONE) ???
      // set signals for terminal only
#warning "toto by mel resit kazdy driver samostatne!..."
      if (MCU == MCU_MSP430) {
	// flash BOOT low MCUs
	// MSP430: TEST has pull-down
	// STM32
	// Tiva, CC13xx: polarity is configurable...
	if (boot_term < 0)
	  boot_term = 0;
      } else {
	// flash BOOT high MCUs
	// LPC17xx
	if (boot_term < 0)
	  boot_term = 1;
      }
      // Z180: BOOT is not used!
    }
    if (terminal) {
      // terminal
      TERM_HANDLE hCon;
      FILE *log = NULL;
      unsigned flags = 0;
      struct stat finfo;
#define	FLG_FLUSH	0x0001
      char title[32];
      // pre-terminal BOOT/RST control
      if (boot_term >= 0)
	flags = (boot_term) ? BOOT_HIGH : BOOT_LOW;
      if (rst_term) {
	com_set_signal (hCom, RST_LOW | flags, 50);
	//flags |= RST_HIGH; // linux has RST pin always activated!!
      }
      if (flags)
	com_set_signal (hCom, flags | RST_HIGH, 50); // linux needs always RST to be deactivated
      flags = 0;
      // opening console for reading
      hCon = term_open ();
      if (TERM_INVALID (hCon)) {
	printf ("Error opening console!\n");
	return 1;
      }
      //term_title
      //printf ("\033]0;MCUload @ "COM_PORT_FMT "\007", com_port);
      sprintf (title, "MCUload @ "COM_PORT_FMT, com_port);
      term_title (title); // slo by dat tento parametr do term_open a zrusit v ramci term_close...
      // log file
      os_time (&sysTime);
      if (log_name)
	if (*log_name) {
	  log = fopen (log_name, "at");
	  if (log == NULL)
	    printf ("Error creating log file \"%s\"!\n", log_name);
	  else {
	    printf ("Append log to file \"%s\"...\n", log_name);
	    fprintf (log, "=============\n[%d.%d.%04d %02d:%02d:%02d] Append log...\n=============\n", sysTime.TM_DAY, sysTime.TM_MONTH, sysTime.TM_YEAR, sysTime.TM_HOUR, sysTime.TM_MIN, sysTime.TM_SEC);
	  }
	}

      if (verbose >= VERB_INFO)
	printf ("[%02d:%02d:%02d] Terminal mode. To exit press ^%c; to reset MCU press ^%c;\nto change BOOT pint press ^%c; to reload HEX file press ^%c.\r\n", sysTime.TM_HOUR, sysTime.TM_MIN, sysTime.TM_SEC,
	  ctl_key[KEY_EXIT] | 0x40, ctl_key[KEY_RESET] | 0x40, ctl_key[KEY_BOOT] | 0x40, ctl_key[KEY_LOAD] | 0x40);

      if (cmds) {
	// execute initial commands
	char *s = cmds;
	if (verbose >= VERB_INFO)
	  printf ("Executing commands...\n");
	if (verbose >= VERB_DEBUG)
	  printf ("%s\n", cmds);
	while (*s) {
	  char c = *s++;
	  char line[8];
	  if (c == '\\') {
	    c = *s;
	    if (!c)
	      break;
	    s++;
	    if (c == 'r')
	      line[0] = '\r';
	    else
	    if (c == 'n')
	      line[0] = '\n';
	    else
	    if (c == 't')
	      line[0] = '\t';
	    else
	    if (c == 'd' && !*s) {
	      cmds = NULL;
	      break;
	    } else
	      line[0] = c;
	    c = 1;
	  } else
	  if (c == '%') {
	    c = *s;
	    if (!c)
	      break;
	    s++;
	    if (c == 'D') {
	      c = '0' + sysTime.TM_DAY/10;
	      line[1] = '0' + sysTime.TM_DAY%10;
	      if (c > '0') {
		line[0] = c;
		c = 2;
	      } else {
		line[0] = line[1];
	        c = 1;
	      }
	    } else
	    if (c == 'M') {
	      c = '0' + (sysTime.TM_MONTH)/10;
	      line[1] = '0' + (sysTime.TM_MONTH)%10;
	      if (c > '0') {
		line[0] = c;
		c = 2;
	      } else {
		line[0] = line[1];
	        c = 1;
	      }
	    } else
	    if (c == 'Y') {
	      line[0] = '0' + ((sysTime.TM_YEAR)/10)%10;
	      line[1] = '0' + (sysTime.TM_YEAR)%10;
	      if (*s == 'Y') {
		s++;
		line[2] = line[0];
		line[3] = line[1];
		line[0] = '0' + (sysTime.TM_YEAR)/1000;
		line[1] = '0' + ((sysTime.TM_YEAR)/100)%10;
		c = 4;
	      } else {
		c = 2;
	      }
	    } else
	    if (c == 'h') {
	      line[0] = '0' + sysTime.TM_HOUR/10;
	      line[1] = '0' + sysTime.TM_HOUR%10;
	      c = 2;
	    } else
	    if (c == 'm') {
	      line[0] = '0' + sysTime.TM_MIN/10;
	      line[1] = '0' + sysTime.TM_MIN%10;
	      c = 2;
	    } else
	    if (c == 's') {
	      line[0] = '0' + sysTime.TM_SEC/10;
	      line[1] = '0' + sysTime.TM_SEC%10;
	      c = 2;
	    } else {
	      line[0] = c;
	      c = 1;
	    }
	  } else
	  if (c == '_') {
	    line[0] = ' ';
	    c = 1;
	  } else {
	    line[0] = c;
	    c = 1;
	  }
	  com_send_buf (hCom, (unsigned char*)line, c);
	}
      }
      time = os_tick ();
      if (reload > 0 && file_name && *file_name) {
	// initialize file info
	stat (file_name, &finfo);
      } else {
	reload = 0;
      }

      do {
	unsigned char b;
	// reading console keys
	b = term_read (hCon);
	// processing control codes
	// BEL 7 \a ^G
	// BS  8 \b ^H
	// HT  9 \t ^I (horizontal tab)
	// LF 10 \n ^J
	// VT 11 \v ^K (vertical tab)
	// FF 12 \f ^L
	// CR 13 \r ^M
	// ESC 27   ^[
	// CP/M specific control codes
	//     3    ^C reboot system (when pressed at start of line)
	//          ^E physical end of line
	// tab      ^I terminates current input
	// cr       ^M terminates current input
	//          ^P copies all subsequent console output to current list device until ^P pressed again
	//          ^R retypes current command line (eg. print clean line after character deletion)
	//             -> not usable in MCUload -> reset function
	//          ^S stops console output temporarily until any key pressed
	//          ^U delete the entire line
	//          ^X same as ^U
	//             -> not usable in MCUload -> exit function
	//          ^Z ends input from console (eg. in PIP and ED)
	//         del deletes and echoes last character typed at the console
	// MCUload specific control codes
	//          ^B switch BOOT pin polarity
	//          ^R assert RST pin to reset user application
	//          ^X exit terminal/MCUload
	//          ^  reload HEX file
	if (b == ctl_key[KEY_EXIT]) { // ^X
	  reload = 0;
	  break;
	}
	if (b == ctl_key[KEY_RESET]) { // ^R
	  printf ("MCU reset...\r\n");
	  com_set_signal (hCom, RST_LOW, 50);
	  com_set_signal (hCom, RST_HIGH, 0);
	  if (log) {
	    fprintf (log, "MCU reset...\n");
	    flags &= ~FLG_FLUSH;
	  }
	} else
	if (b == ctl_key[KEY_BOOT]) { // ^B
	  printf ("BOOT = %u\r\n", (com_set_signal (hCom, BOOT_INV, 0) & BOOT_HIGH) != 0);
	} else
	if (b == ctl_key[KEY_LOAD]) { // ^L
	  if (!reload)
	    reload = -1;
	  break;
	} else
	if (b) {
	  com_send_buf (hCom, &b, sizeof (b));
	}
	// reading serial data
	b = com_rec_byte (hCom, 0);
	if (b != (unsigned char)COM_REC_TIMEOUT) {
	  if (b >= ' ' || b == '\t' || b == '\r' || b == '\n' || b == '\b' || b == '\f' || b == '\a' || b == 0x1B)
	    term_write (b); //putchar
	  else
	    printf ("{%02X}", b);
	  if (log) {
	    fputc (b, log);
	    flags &= ~FLG_FLUSH;
	  }
	} else {
	  os_sleep (10);
	  if (log && !(flags & FLG_FLUSH)) {
	    fflush (log);
	    flags |= FLG_FLUSH;
	  }
	}
	if (reload && os_tick () - time >= 1000) {
	  static time_t last;
	  struct stat tmp;
	  time += 1000;
	  stat (file_name, &tmp);
	  if (tmp.st_mtime > finfo.st_mtime) {
	    if (tmp.st_mtime == last)
	      break;
	  }
	  last = tmp.st_mtime;
	}
      } while (1);
      term_close (hCon);
      term_title (NULL);
      os_time (&sysTime);
      printf ("\n[%02d:%02d:%02d] Terminal finished.\n", sysTime.TM_HOUR, sysTime.TM_MIN, sysTime.TM_SEC);
      if (log) {
	fprintf (log, "\n[%d.%d.%04d %02d:%02d:%02d] Terminal finished.\n", sysTime.TM_DAY, sysTime.TM_MONTH, sysTime.TM_YEAR, sysTime.TM_HOUR, sysTime.TM_MIN, sysTime.TM_SEC);
	fclose (log);
      }
    }
  } while (reload);
  if (verbose >= VERB_INFO)
    printf ("\r\nDone.\r\n");
  com_close (hCom);
  return 0;
}

