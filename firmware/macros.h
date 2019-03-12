/* ========================================================================== */
/*                                                                            */
/*   macros.h                                                                 */
/*   (c) 2005 Hynek Sladky                                                    */
/*                                                                            */
/*   general macros                                                           */
/*                                                                            */
/* ========================================================================== */

// makra pro praci s bity
#define BITSET__(mem,bit) (mem|=(1<<bit))
#define BITRES__(mem,bit) (mem&=~(1<<bit))
#define BIT__(mem,bit) (mem&(1<<bit))

#define BITRES(...) BITRES__(__VA_ARGS__)
#define BITSET(...) BITSET__(__VA_ARGS__)
#define BIT(...) BIT__(__VA_ARGS__)

