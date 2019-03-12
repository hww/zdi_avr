#ifndef _COM_H_
#define	_COM_H_

#include <time.h>

#define	COM_HANDLE	int
#define	COM_INVALID(h)	(h < 0)

//extern char com_port[256];
extern char *com_port;
extern unsigned usb_vid;
extern unsigned usb_pid;
//extern char usb_serial[256];
extern char *usb_serial;

#define	COM_PORT_FMT	"%s"

#define	ARGS_COM_PARAMS	\
  {ARGS_STRING, "-p", NULL, &com_port, 1, ""}, \
  {ARGS_STRING, "--com", "<port>", &com_port, 1, "select COM port name [%s]"}, \
  {ARGS_INTEGER, "--vid", "<vendor_id>", &usb_vid, 1, "select USB VendorID for port auto-detect [%04X]"}, \
  {ARGS_INTEGER, "--pid", "<product_id>", &usb_pid, 1, "select USB ProductID for port auto-detect [%04X]"}, \
  {ARGS_STRING, "--serial", "<serial>", &usb_serial, 1, "select USB serial for port auto-detect [%s]"},

extern COM_HANDLE com_open_port (const char *name, unsigned long speed, unsigned parity);
#define	COM_PAR_NONE	0
#define	COM_PAR_ODD	1
#define	COM_PAR_EVEN	2
//#define	COM_PAR_MARK
//#define	COM_PAR_SPACE
extern void com_speed (COM_HANDLE tty, unsigned long speed, unsigned parity);
extern void com_close (COM_HANDLE tty);

//#define	SET_RST		0x0001
//#define	CLR_RST		0x0002
//#define	SET_BOOT	0x0004
//#define	CLR_BOOT	0x0008
#define	COM_RTS_CLR	0x0001
#define	COM_RTS_SET	0x0002
#define	COM_DTR_CLR	0x0004
#define	COM_DTR_SET	0x0008
#define	COM_RTS_INV	0x0010
#define	COM_DTR_INV	0x0020
 
extern unsigned com_set_signal (COM_HANDLE tty, unsigned signal, unsigned delay);

extern int com_send_buf (COM_HANDLE tty, const unsigned char *buf, unsigned len);
extern int com_rec_byte (COM_HANDLE tty, unsigned timeout);
#define	COM_REC_TIMEOUT	-1
extern int com_rec_buf (COM_HANDLE tty, unsigned char *buf, unsigned len, unsigned timeout);
extern void com_flush (COM_HANDLE tty);

extern unsigned long os_tick (void);
extern void os_sleep (unsigned long msec);
#define	TIME_TYPE	struct tm
//#define	os_time		localtime 
extern void os_time (TIME_TYPE *tm);
#define	TM_YEAR		tm_year + 1900
#define	TM_MONTH	tm_mon + 1
#define	TM_DAY		tm_mday
#define	TM_HOUR		tm_hour
#define	TM_MIN		tm_min
#define	TM_SEC		tm_sec

extern int com_find_usb (char *name, unsigned short vid, unsigned short pid, const char *serial);

#endif

