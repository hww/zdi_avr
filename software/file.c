#include <stdio.h>
#include <stdlib.h>

#include "os.h"
#include "load.h"
#include "file.h"

// binary file offset
unsigned file_offs = 0; //0x20000000;
// manual start address
unsigned file_run = 0; //0x0000030D; //0x2000027D;
unsigned file_mode = FILE_AUTO;
unsigned file_unsort = 0;

// ====================================
// IntelHEX functions
// ====================================
// load hex digits
// ------------------------------------
static int get_hex (const char *str, unsigned digit) {
unsigned val = 0;
  if (!*str)
    return -1;
  while (digit--) {
    char c = *str++;
    if (!c)
      return -2;
    val <<= 4;
    if (c >= 'a' && c <= 'z')
      c &= ~0x20;
    if (c >= '0' && c <= '9')
      val |= c - '0';
    else
    if (c >= 'A' && c <= 'F')
      val |= c - 'A' + 0xA;
    else
      return -3;
  }
  return val;
}

// ====================================
// Intel HEX record data type
// ------------------------------------
typedef struct {
  unsigned char cnt;
  unsigned short addr;
  unsigned char type;
  unsigned char *data;
} ihex_type;

// ====================================
// load Intel HEX record
// ------------------------------------
static int ihex_load (const char *line, ihex_type *var) {
unsigned char sum = 0, idx;
int val;

  // check input data
  if (!line || !var)
    return -1;
  // check ihex line prefix
  if (line[0] != ':')
    return -2;
  // load data length
  val = get_hex (line + 1, 2);
  if (val < 0)
    return -3;
  var->cnt = val;
  sum += val;
  // load address
  val = get_hex (line + 3, 4);
  if (val < 0)
    return -4;
  var->addr = val;
  sum += val;
  sum += val >> 8;
  // load record type
  val = get_hex (line + 7, 2);
  if (val < 0)
    return -5;
  var->type = val;
  sum += val;
  for (idx = 0; idx < var->cnt; idx++) {
    // load data bytes
    val = get_hex (line + 9 + (idx << 1), 2);
    if (val < 0)
      return -6;
    if (var->data)
      var->data[idx] = val;
    sum += val;
  }
  // load checksum
  val = get_hex (line + 9 + (idx << 1), 2);
  if (val < 0)
    return -7;
  sum += val;
  if (sum)
    return -8;
  return var->cnt;
}

// ====================================
// load Intel HEX or BINary  file
// ------------------------------------
file_data_type *file_load (const char *filename) {
FILE *f;
unsigned size;
file_data_type *mem = NULL;
#define	LINE_LEN	550 // min.(255*2+11+3) ":LLAAAATTddd...dddCC\r\n\0"
char line[LINE_LEN];

  f = fopen (filename, "rb");
  if (f == NULL) {
    printf ("Can't open file %s!\n", filename);
    return NULL;
  }
  if (verbose >= VERB_INFO)
    printf ("Loading file %s...\n", filename);
  // try to auto-detect file format
  if (file_mode == FILE_AUTO) {
    fread (line, LINE_LEN, 1, f);
    //fseek (f, 0, SEEK_SET);
    if (line[0] == ':') {
      unsigned pos = 0;
      char c;
      do {
        c = line[++pos];
      } while (pos < LINE_LEN && ((c >= '0' && c <= '9') || ((c & ~0x20) >= 'A' && (c & ~0x20) <= 'F')));
      if (pos > 10) {
	file_mode = FILE_HEX;
	if (verbose >= VERB_INFO)
	  printf ("Intel HEX format detected.\n");
      }
    }
    if (file_mode == FILE_AUTO) {
      file_mode = FILE_BIN;
      if (verbose >= VERB_INFO)
        printf ("BINary format forced.\n");
    }
  }
  if (file_mode == FILE_BIN) {
    // load binary file
    fseek (f, 0, SEEK_END);
    size = ftell (f);
    fseek (f, 0, SEEK_SET);
    mem = malloc (sizeof (file_data_type) + size);
    if (mem == NULL) {
      printf ("Can't allocate memory!\n");
    } else {
      mem->next = NULL;
      mem->addr = file_offs;
      mem->len = size;
      mem->data = ((unsigned char*)mem) + sizeof (file_data_type);
      if (!fread (mem->data, size, 1, f)) {
        printf ("Can't read file %s!\n", filename);
        free (mem);
        mem = NULL;
      }
    }
  } else
  if (file_mode == FILE_HEX) {
    // parse IntelHEX file and convert it to file_data_type format
    unsigned addr = 0x0000FFFF, cnt = 0;
    ihex_type ihex;
    file_data_type *ptr;
    // reopen file in text mode
    fclose (f);
    f = fopen (filename, "rt");
    if (f == NULL) {
      printf ("Can't re-open file %s!\n", filename);
      return NULL;
    }
    // analyze file
    size = 0;
    ihex.data = (unsigned char*)line;
    while (!feof (f)) {
      if (!fgets (line, LINE_LEN, f))
        break;
      if (line[0] != ':')
        continue;
      if (ihex_load (line, &ihex) < 0) {
        printf ("Intel HEX format error!\n");
        fclose (f);
        return NULL;
      }
      if (ihex.type == 0x00) {
        // data record
        if ((addr & 0xFFFF) != ihex.addr) {
          // new section
          cnt++;
          addr = (addr & 0xFFFF0000) | ihex.addr;
        }
        size += ihex.cnt;
        // wrap check
        if ((addr & 0xFFFF) + ihex.cnt > 0x10000) {
          printf ("Page wrap error!\n");
          fclose (f);
          return NULL;
        }
        addr += ihex.cnt;
        if (!(addr & 0xFFFF))
          addr -= 0x10000;
      } else
      if (ihex.type == 0x01) {
        // end of file record
        break;
      } else
      if (ihex.type == 0x04) {
        // extended linear address
        addr = (addr & 0xFFFF) | (ihex.data[0] << 24) | (ihex.data[1] << 16);
      } else
      if (ihex.type != 0x05) {
        // 05 = start linear address
        if (verbose >= VERB_DEBUG)
          printf ("Unsupported record type! (%02X)\n", ihex.type);
      }
    }
    if (verbose >= VERB_DEBUG)
      printf ("%s: %u bytes, %u sections\n", filename, size, cnt);
    // create memory
    mem = malloc (sizeof (file_data_type) * (cnt + 1) + size + 256);
    if (mem == NULL) {
      printf ("Can't allocate memory!\n");
      fclose (f);
      return NULL;
    }
    // prepare all pointers
    // master block pointer (important for later freeing memory)
    mem[0].data = (unsigned char*)mem;
    mem[0].data += (cnt + 1) * sizeof (file_data_type);
    mem[0].len = size;
    mem[0].addr = 0; // min addr ??
    mem[0].next = NULL;
    // rewind file to begin
    fseek (f, 0, SEEK_SET);
    // load HEX file
    ihex.data = mem[0].data;
    addr = 0x0000FFFF;
    cnt = 0;
    while (!feof (f)) {
      if (!fgets (line, LINE_LEN, f))
        break;
      if (line[0] != ':')
        continue;
      if (ihex_load (line, &ihex) < 0) {
        // shouldn't be here - file is already checked...
        printf ("Intel HEX format error!\n");
        free (mem);
        mem = NULL;
        break;
      }
      if (ihex.type == 0x00) {
        // data record
        if ((addr & 0xFFFF) != ihex.addr) {
          // new section
          cnt++;
          addr = (addr & 0xFFFF0000) | ihex.addr;
          mem[cnt].data = ihex.data;
          mem[cnt].addr = addr;
          mem[cnt].len = 0;
          mem[cnt].next = NULL;
        }
        mem[cnt].len += ihex.cnt;
        // wrap check
        if ((addr & 0xFFFF) + ihex.cnt > 0x10000) {
          printf ("Page wrap error!\n");
          free (mem);
          mem = NULL;
          break;
        }
        addr += ihex.cnt;
        if (!(addr & 0xFFFF))
          addr -= 0x10000;
        ihex.data += ihex.cnt;
      } else
      if (ihex.type == 0x01) {
        // end of file record
        break;
      } else
      if (ihex.type == 0x04) {
        // extended linear address
        addr = (addr & 0xFFFF) | (ihex.data[0] << 24) | (ihex.data[1] << 16);
      } else
      if (ihex.type == 0x05) {
        // start linear address
        if (!file_run || file_run == file_offs) {
          file_run = (ihex.data[0] << 24) | (ihex.data[1] << 16) | (ihex.data[2] << 8) | ihex.data[3];
          if (verbose >= VERB_DEBUG)
            printf ("Using start address %08X\n", file_run);
        } else
          printf ("Start address from file ignored!\n");
      } /*else {
        if (verbose >= VERB_DEBUG)
          printf ("Unsupported record type! (%02X)\n", ihex.type);
      }*/
    }
    if (verbose >= VERB_DEBUG) {
      printf ("Unsorted sections:\n");
      for (size = 0; size <= cnt; size++) {
        printf ("[%u] %08X : %u\n", size, mem[size].addr, mem[size].len);
        if (!size) continue;
        // check if all memory blocks are not overlaping
        if ((mem[size].addr + mem[size].len) < mem[size].addr) {
          printf ("Sector address overflow!\n");
          break;
        }
        for (addr = 1; addr <= cnt; addr++) {
          if (addr == size) continue;
          if ((mem[addr].addr + mem[addr].len) > mem[size].addr && mem[addr].addr < (mem[size].addr + mem[size].len)) {
            printf ("Overlap found with block! [%u] %08X : %u\n", addr, mem[addr].addr, mem[addr].len);
          }
        }
      }
      if (size <= cnt) {
        free (mem);
        fclose (f);
        return NULL;
      }
    }
    // sorting sections
    if (!file_unsort) {
      ptr = mem;
      do {
	for (size = 1; size <= cnt; size++) {
	  if (mem[size].next != NULL || ptr == &mem[size])
	    continue;
	  if (ptr->next == NULL || mem[size].addr < ptr->next->addr)
	    ptr->next = &mem[size];
	}
	ptr = ptr->next;
      } while (ptr != NULL);
      if (mem[0].next && !file_offs)
        mem[0].addr = mem[0].next->addr;
      if (verbose >= VERB_DEBUG) {
	printf ("Sorted sections:\n");
	ptr = mem;
	do {
	  printf ("[?] %08X : %u\n", ptr->addr, ptr->len);
	  ptr = ptr->next;
	} while (ptr);
      }
    } else {
      // fill pointers for unsorted array
      if (cnt && !file_offs)
        mem[0].addr = mem[1].addr;
      for (size = 1; size <= cnt; size++) {
	mem[size-1].next = &mem[size];
	if (mem[size].addr < mem[0].addr)
	  mem[0].addr = mem[size].addr;
      }
    }
  } else {
    printf ("Unknown file format!\n");
  }
  fclose (f);
  return mem;
}

// ====================================
// calculate memory size in selected region
// ------------------------------------
unsigned file_block_size (const file_data_type *data, unsigned start, unsigned end, unsigned *addr_ptr) {
unsigned size = 0, addr = 0;
//const file_data_type *ptr;

  if (data->next)
    data = data->next;
  while (data) {
    unsigned blk_start = data->addr, blk_end = data->addr + data->len - 1;
    // clip block to selected region
    if (blk_start < start)
      blk_start = start;
    if (blk_end > end)
      blk_end = end;
    if (blk_start < blk_end) {
      if (!size) {
        addr = blk_start;
        size = blk_end - blk_start;
      }
      if (blk_start < addr) {
        size += addr - blk_start;
        addr = blk_start;
      }
      if (blk_end >= (addr + size)) {
        size += blk_end - addr - size + 1;
      }
//      if (verbose >= VERB_DEBUG)
//	printf ("Adding (%08X..%08X) @ %08X = %u bytes\n", blk_start, blk_end, addr, size);
    }
    data = data->next;
  }
//  if (verbose >= VERB_DEBUG)
//    printf ("Block size in (%08X..%08X) @ %08X = %u bytes\n", start, end, addr, size);
  if (addr_ptr)
    *addr_ptr = addr;
  return size;
}

// ====================================
// release data memory
// ------------------------------------
void file_free (file_data_type *ptr) {
  if (ptr != NULL)
    free (ptr);
}

// ====================================
// make Intel-HEX line
// ------------------------------------
void make_hex_line (char *str, unsigned char cnt, unsigned addr, unsigned char type, unsigned char *data) {
  str += sprintf (str, ":%02X%04X%02X", cnt, addr & 0xFFFF, type);
  type = 0 - cnt - (unsigned char)addr - (unsigned char)(addr >> 8) - type;
  while (cnt--) {
    str += sprintf (str, "%02X", *data);
    type -= *data++;
  }
  str += sprintf (str, "%02X\r", type);
}
