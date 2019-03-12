#ifndef _OS_H_
#define	_OS_H_


// ====================================
// common definitions
// ====================================

// ====================================
// platform specific libraries
// ====================================

#if defined(_WIN32) && !defined(__CYGWIN__)
// windows / mingw
#define COMPILE_FOR_WINDOWS
#include <windows.h>
#include "win/term.h"
#include "win/com.h"
#define	int4	int
#elif defined(__CYGWIN__)
// cygwin
#define COMPILE_FOR_CYGWIN
#else
// linux
#define COMPILE_FOR_LINUX
#include <stdio.h>
#include "linux/term.h"
#include "linux/com.h"
#define	int4	__INT32_TYPE__
#endif

#endif

