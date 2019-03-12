#include <stdio.h>

#include "os.h"

// ====================================
// platform specific libraries
// ====================================

#if defined(_WIN32) && !defined(__CYGWIN__)
// windows / mingw
#define COMPILE_FOR_WINDOWS
#include "win/term.c"
#include "win/com.c"
#elif defined(__CYGWIN__)
// cygwin
#define COMPILE_FOR_CYGWIN
#else
// linux
#define COMPILE_FOR_LINUX
#include "linux/term.c"
#include "linux/com.c"
#endif

