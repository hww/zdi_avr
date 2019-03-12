#ifndef _TERM_H_
#define	_TERM_H_

#define	TERM_HANDLE	HANDLE
#define	TERM_INVALID(h)	(h == INVALID_HANDLE_VALUE)

extern TERM_HANDLE term_open (void);
extern int term_read (TERM_HANDLE con);
//extern int term_read (TERM_HANDLE con, COM_HANDLE tty);
extern void term_write (unsigned char c);
extern void term_close (TERM_HANDLE con);
extern void term_title (const char *str);

#endif

