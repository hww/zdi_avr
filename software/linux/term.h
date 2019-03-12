#ifndef _TERM_H_
#define	_TERM_H_

#define	TERM_HANDLE	FILE*
#define	TERM_INVALID(h)	!h

extern TERM_HANDLE term_open (void);
extern int term_read (TERM_HANDLE con);
extern void term_close (TERM_HANDLE con);
extern void term_title (const char *title);

#define	term_write	putchar

#endif

