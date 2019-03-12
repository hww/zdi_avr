#ifndef _LOAD_H_
#define	_LOAD_H_

extern int verbose;
enum {VERB_NONE, VERB_INFO, VERB_DEBUG, VERB_COMM};

//extern int reset_before;
extern int read_out;
extern int erase;
extern int blank_check;
extern int prog_flash;
extern int verify;
//extern int reset_after;
extern int cpu_freq; //[kHz]
extern int execute;
extern int terminal;


#endif
