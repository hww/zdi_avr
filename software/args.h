/****************************************************************************
 *        ___  ____   ___                                                   *
 *       / _ \|  _ \ / __\                                                  *
 *  	| | | | |_| | |___ ___                                              *
 *  	| |_| |    /| |_  | __|                                             *
 *  	|  _  | |\ \| |_| |__ |                                             *
 *  	|_| |_|_| \_|\___/|___|	Command line parser                         *
 *      ===========================================                         *
 *      Copyright (C) 2004 Jiri Osoba <osoba@emai.cz>                       *
 *                                                                          *
 *	This program is free software; you can redistribute it              *
 *	and/or modify it under the terms of the GNU General Public          *
 *	License as published by the Free Software Foundation;               *
 *	either version 2 of the License, or (at your option) any            *
 *	later version.                                                      *
 *                                                                          *
 *	This program is distributed in the hope that it will be             *
 *	useful, but WITHOUT ANY WARRANTY; without even the implied          *
 *	warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR             *
 *	PURPOSE. See the GNU General Public License for more                *
 *	details.                                                            *
									    *
 *	You should have received a copy of the GNU General Public           *
 *	License along with this program; if not, write to the Free          *
 *	Software Foundation, Inc., 675 Mass Ave, Cambridge, MA              *
 *	02139, USA.                                                         *
 *                                                                          *
 ****************************************************************************/

/****************************************************************************
 *      A R G S . H                                                         *
 ****************************************************************************/


#ifndef __ARGS_H__
#define __ARGS_H__

#define	ARGS_NONE       	0
#define	ARGS_SWITCH     	1
#define	ARGS_CHOOSE     	2
#define	ARGS_OPTCHOOSE  	3
#define	ARGS_INTEGER    	4
#define	ARGS_OPTINTEGER 	5
#define	ARGS_FLOAT      	6
#define	ARGS_OPTFLOAT   	7
#define	ARGS_STRING     	8
#define	ARGS_MULTISTRING        9
#define	ARGS_FILE       	10
#define ARGS_PRESET 		11
#define ARGS_PRESETCHOOSE	12

typedef struct {
    int type;
    char *option;
    char *param;
    void *value;
    float add_value;
    char *help;
} ARGS;


void args_print_help(const ARGS * args);
int args_parse_args(int argc, char **argv, const ARGS * args, char *separators);

const char *args_version(void);
const char *args_date(void);
const char *args_copyright(void);


#endif
