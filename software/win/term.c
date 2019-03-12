#include <windows.h>
#include <string.h>
#include "com.h"
//#include "term.h"

// ====================================
// open terminal for reading
// ------------------------------------
TERM_HANDLE term_open (void) {
TERM_HANDLE hndl;
DWORD mode;
  //hndl = CreateFile ("CON", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
  hndl = GetStdHandle (STD_INPUT_HANDLE);
  // enable ^C to be reported as key
  GetConsoleMode (hndl, &mode);
  SetConsoleMode (hndl, mode & ~ENABLE_PROCESSED_INPUT);
  return hndl;
}

// ====================================
// read from terminal
// ------------------------------------
int term_read (TERM_HANDLE con) {
//int term_read (TERM_HANDLE con, COM_HANDLE tty) {
INPUT_RECORD ir;
DWORD size;
#define	ESC_LEN		1024 //8
static char esc[ESC_LEN] = {0};
static unsigned esc_pos = 0;
static const struct {
  WORD code;
  DWORD ctrl;
  char sequence[ESC_LEN];
} esc_tab[] = {
  {VK_UP, 0, "\x1B[A"},
  {VK_DOWN, 0, "\x1B[B"},
  {VK_LEFT, (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED), "\x1BOC"},
  {VK_LEFT, 0, "\x1B[C"},
  {VK_RIGHT, (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED), "\x1BOD"},
  {VK_RIGHT, 0, "\x1B[D"},
  {VK_HOME, 0, "\x1B[1~"},
  {VK_INSERT, 0, "\x1B[2~"},
  {VK_DELETE, 0, "\x7F"}, // "\x1B[3~"
  {VK_END, 0, "\x1B[4~"},
  {VK_PRIOR, 0, "\x1B[5~"}, // PG UP
  {VK_NEXT, 0, "\x1B[6~"}, // PG DOWN
  {VK_F1, 0, "\x1B[11~"},
  {VK_F2, 0, "\x1B[12~"},
  {VK_F3, 0, "\x1B[13~"},
  {VK_F4, 0, "\x1B[14~"},
  {VK_F5, 0, "\x1B[15~"},
  {VK_F6, 0, "\x1B[17~"},
  {VK_F7, 0, "\x1B[18~"},
  {VK_F8, 0, "\x1B[19~"},
  {VK_F9, 0, "\x1B[20~"},
  {VK_F10, 0, "\x1B[21~"},
  {VK_F11, 0, "\x1B[23~"},
  {VK_F12, 0, "\x1B[24~"},
  {0}
};

  // return saved ESC sequence
  if (esc[esc_pos] && esc_pos < sizeof (esc))
    return esc[esc_pos++];
  if (esc_pos) {
    esc[0] = 0;
    esc_pos = 0;
  }
  // check console input
  if (!PeekConsoleInput (con, &ir, 1, &size) || !size)
    return 0;
  ReadConsoleInput (con, &ir, 1, &size);
  if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown)
    return 0;
  // paste from clipboard
//  if (ir.Event.KeyEvent.wVirtualKeyCode == VK_INSERT && ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)
//    return 0x16;
// tady by slo ulozit text do esc[] -> ale nevi, jak by melo byt toto pole velke; bude stacit 1KB?
  if (ir.Event.KeyEvent.uChar.AsciiChar == '\x16' || (ir.Event.KeyEvent.wVirtualKeyCode == VK_INSERT && ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED)) { // ^V
    if (OpenClipboard (NULL)) {
      HANDLE hndl = GetClipboardData (CF_TEXT);
//      if (hndl && tty != INVALID_HANDLE_VALUE)
//	WriteFile (tty, hndl, strlen (hndl), &size, NULL);
      if (hndl)
	strncpy (esc, hndl, ESC_LEN - 1);
      CloseClipboard ();
    }
    return 0;
  }
  // ASCII key
  if (ir.Event.KeyEvent.uChar.AsciiChar)
    return ir.Event.KeyEvent.uChar.AsciiChar;
  // Fn/virtual key processing
  for (size = 0; esc_tab[size].code; size++) {
    if (ir.Event.KeyEvent.wVirtualKeyCode == esc_tab[size].code && (!esc_tab[size].ctrl || (ir.Event.KeyEvent.dwControlKeyState & esc_tab[size].ctrl))) {
      strcpy (esc, esc_tab[size].sequence);
      return (esc[esc_pos++]);
    }
  }
  return 0;
}

// ====================================
// close terminal
// ------------------------------------
void term_close (TERM_HANDLE con) {
  //CloseHandle (con); // just in case of CreateFile in term_open; otherwise, console input is totally disabled when using GetStdHandle...
}

// ====================================
// supporting function for ANSI ESC sequences
// ------------------------------------
static int getnum (const char *str, int *val) {
int i = 0;
  *val = 0;
  while (*str >= '0' && *str <= '9') {
    *val *= 10;
    *val += *str++ - '0';
    i++;
  }
  return i;
}

// ====================================
// write data on terminal
// ------------------------------------
void term_write (unsigned char c) {
#if 0
  putchar (c);
#else
// vystup znaku s podporou ANSI
static char esc[32];
static int escl = 0;
static int last_char, last_line;
int x,y;
CONSOLE_SCREEN_BUFFER_INFO bfin;
DWORD cnt;
static const WORD foreground[8] = {
  0,
  FOREGROUND_RED,
  FOREGROUND_GREEN,
  FOREGROUND_RED | FOREGROUND_GREEN,
  FOREGROUND_BLUE,
  FOREGROUND_BLUE | FOREGROUND_RED,
  FOREGROUND_BLUE | FOREGROUND_GREEN,
  FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED
};
static const WORD background[8] = {
  0,
  BACKGROUND_RED,
  BACKGROUND_GREEN,
  BACKGROUND_RED | BACKGROUND_GREEN,
  BACKGROUND_BLUE,
  BACKGROUND_BLUE | BACKGROUND_RED,
  BACKGROUND_BLUE | BACKGROUND_GREEN,
  BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED
};
static unsigned char fore = 7, back = 0;
static COORD cursor;
//static WORD attr;
static char wrap = 1;
static HANDLE hConOut = INVALID_HANDLE_VALUE;
  if (hConOut == INVALID_HANDLE_VALUE)
    hConOut = GetStdHandle (STD_OUTPUT_HANDLE);
  if (c=='\x1B') {
    escl=0;
    esc[escl++]=c;
  } else
  if (escl) {
    esc[escl++]=c;
    if ((c>='A' && c<='Z') || (c>='a' && c<='z')) {
      // vykonani ESC prikazu
      if (esc[1]=='[') {
	switch (c) {
	case 'H':
	  // cursor positioning
	  escl = 2;
	  x = y = 0;
	  escl += getnum (&esc[escl], &y);
	  if (esc[escl]==';') escl++;
	  escl += getnum (&esc[escl], &x);
	  if (x) x--;
	  if (y) y--;
	  GetConsoleScreenBufferInfo (hConOut, &bfin);
	  bfin.dwCursorPosition.X = x;
	  bfin.dwCursorPosition.Y = bfin.srWindow.Top+y;
	  SetConsoleCursorPosition (hConOut, bfin.dwCursorPosition);
	  break;
	case 'J':
	  // erasing screen
	  GetConsoleScreenBufferInfo (hConOut, &bfin);
	  switch (esc[2]) {
	    case 'J':
	    case '0': // from cursor to end of screen
	      FillConsoleOutputCharacter (hConOut, 0, bfin.dwSize.X * (bfin.srWindow.Bottom - bfin.srWindow.Top + 1) - bfin.dwCursorPosition.X - bfin.dwSize.X * bfin.dwCursorPosition.Y, bfin.dwCursorPosition, &cnt);
	      break;
	    case '1': // from beginning of screen to cursor
	      x = bfin.dwCursorPosition.X + bfin.dwSize.X * bfin.dwCursorPosition.Y;
	      bfin.dwCursorPosition.X = 0;
	      bfin.dwCursorPosition.Y = bfin.srWindow.Top;
	      FillConsoleOutputCharacter (hConOut, 0, x, bfin.dwCursorPosition, &cnt);
	      break;
	    case '2': // entire screen
	      bfin.dwCursorPosition.X = 0;
	      bfin.dwCursorPosition.Y = bfin.srWindow.Top;
	      SetConsoleCursorPosition (hConOut, bfin.dwCursorPosition);
	      FillConsoleOutputCharacter (hConOut, 0, bfin.dwSize.X * (bfin.srWindow.Bottom - bfin.srWindow.Top + 1), bfin.dwCursorPosition, &cnt);
	      break;
	    default:
	      break;
	  }
	  break;
	case 'K':
	  // erasing line
	  GetConsoleScreenBufferInfo (hConOut, &bfin);
	  switch (esc[2]) {
	    case 'K':
	    case '0': // from cursor to end of line
	      FillConsoleOutputCharacter (hConOut, 0, 80-bfin.dwCursorPosition.X, bfin.dwCursorPosition, &cnt);
	      break;
	    case '1': // from beginning to cursor
	      x=bfin.dwCursorPosition.X;
	      bfin.dwCursorPosition.X=0;
	      FillConsoleOutputCharacter (hConOut, 0, x, bfin.dwCursorPosition, &cnt);
	      break;
	    case '2': // entire line
	      bfin.dwCursorPosition.X=0;
	      FillConsoleOutputCharacter (hConOut, 0, 80, bfin.dwCursorPosition, &cnt);
	      break;
	    default:
	      break;
	  }
	  break;
	case 'm':
	  // character attributes...
	  // unsupported but w/o reporting them
	  escl = 2;
	  do {
	    x = 0;
	    escl += getnum (&esc[escl], &x);
	    if (x >= 40 && x <= 47) {
	      SetConsoleTextAttribute (hConOut, foreground[fore] | background[back = x - 40]);
	    } else
	    if (x >= 30 && x <= 37) {
	      SetConsoleTextAttribute (hConOut, foreground[fore = x - 30] | background[back]);
	    } else
//	0	//Reset all attributes
//	1	//Bright
//	2	//Dim
//	4	//Underscore	
//	5	//Blink
//	7	//Reverse
//	8	//Hidden
	    if (x == 7) {
	      x = back;
	      back = fore;
	      fore = x;
	      SetConsoleTextAttribute (hConOut, foreground[fore] | background[back]);
	    } else
	    if (x == 0) {
	      fore = 7;
	      back = 0;
	      SetConsoleTextAttribute (hConOut, foreground[fore] | background[back]);
	    }
	    if (esc[escl]==';') escl++;
	  } while (esc[escl] != 'm');
	  break;
	case 'l': // ESC[?7l vypnout wrap -> toto asi pred editaci
	  if (esc[2] == '?' && esc[3] == '7')
	    wrap = 0;
	  break;
	case 'h': // ESC[?7h zapnout wrap -> toto asi po editaci
	  if (esc[2] == '?' && esc[3] == '7')
	    wrap = 1;
	  break;
	case 's': // ESC[s save
	  GetConsoleScreenBufferInfo (hConOut, &bfin);
	  cursor = bfin.dwCursorPosition;
	  break;
	case 'u': // ESC[u restore (unsave)
	  SetConsoleCursorPosition (hConOut, cursor);
	  break;
	//case '7': // ESC7 save cursor & attributes
	//case '8': // ESC8 restore cursor & attributes
	default:
	  esc[escl]=0;
	  //fprintf (stderr, "\nUnsupported ESC sequence \"%s\"!\n", &esc[1]);
	  printf ("%s", esc);
	  break;
	}
      } else {
	esc[escl]=0;
	//fprintf (stderr, "\nUnsupported ESC sequence \"%s\"!\n", &esc[1]);
	printf ("%s", esc);
      }
      escl=0;
    }
  } else {
    GetConsoleScreenBufferInfo (hConOut, &bfin);
    if (last_char && (c=='\r' || c=='\n')) {
      bfin.dwCursorPosition.Y--;
      SetConsoleCursorPosition (hConOut, bfin.dwCursorPosition);
    }
    if (c == '\f') {
#if 0
      // clear screen
      bfin.dwCursorPosition.X = 0;
      bfin.dwCursorPosition.Y = bfin.srWindow.Top;
      SetConsoleCursorPosition (hConOut, bfin.dwCursorPosition);
      FillConsoleOutputCharacter (hConOut, 0, bfin.dwSize.X * (bfin.srWindow.Bottom - bfin.srWindow.Top + 1), bfin.dwCursorPosition, &cnt);
#else
      // scroll to empty screen
      for (y = bfin.srWindow.Bottom - bfin.srWindow.Top + 1; y; y--)
        putchar ('\n');
      bfin.dwCursorPosition.X = 0;
      bfin.dwCursorPosition.Y++;
      if (bfin.dwCursorPosition.Y > bfin.dwSize.Y - bfin.srWindow.Bottom + bfin.srWindow.Top - 1)
        bfin.dwCursorPosition.Y = bfin.dwSize.Y - bfin.srWindow.Bottom + bfin.srWindow.Top - 1;
      SetConsoleCursorPosition (hConOut, bfin.dwCursorPosition);
#endif
    } else {
      if (wrap || bfin.dwCursorPosition.X < 79)
        putchar (c);
    }
    last_char = (bfin.dwCursorPosition.X==79 && c>=' ');
    last_line = (bfin.dwCursorPosition.Y==24 && c>=' ');
/**
    // pri rucnim tisku se museji zpracovavat znaky \r\n
    GetConsoleScreenBufferInfo (hConOut, &bfin);
    FillConsoleOutputCharacter (hConOut, c, 1, bfin.dwCursorPosition, &cnt);
    bfin.dwCursorPosition.X++;
    if (bfin.dwCursorPosition.X>=80) {
      bfin.dwCursorPosition.X=0;
      bfin.dwCursorPosition.Y++;
    }
    SetConsoleCursorPosition (hConOut, bfin.dwCursorPosition);
**/
  }
#endif
}

void term_title (const char *str) {
  SetConsoleTitle (str);
}

