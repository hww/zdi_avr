#ifndef _FILE_H_
#define _FILE_H_

//#include "os.h"

#define	ARGS_FILE_PARAMS \
  {ARGS_INTEGER, "--addr", 0, &file_offs, 1, "set address; default %08X"}, \
  {ARGS_INTEGER, "--start", 0, &file_run, 1, "run address; default set from --addr"}, \
  {ARGS_CHOOSE, "--mode", "auto/bin/hex", &file_mode, 2, "file mode; default %s"}, \
  {ARGS_OPTINTEGER, "--unsort", 0, &file_unsort, 1, "disable file block sorting"},

extern unsigned file_offs;
extern unsigned file_run;
enum {FILE_AUTO, FILE_BIN, FILE_HEX};
extern unsigned file_mode;
extern unsigned file_unsort;

typedef struct data_struct {
  struct data_struct *next;
  unsigned int4 addr;
  unsigned int4 len;
  unsigned char *data;
} file_data_type;

extern file_data_type *file_load (const char *filename);
extern unsigned file_block_size (const file_data_type *data, unsigned start, unsigned end, unsigned *addr_ptr);
extern void file_free (file_data_type *data);
extern void make_hex_line (char *str, unsigned char cnt, unsigned addr, unsigned char type, unsigned char *data);

#endif
