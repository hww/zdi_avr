/****************************************************************************
 *      A R G s                                                             *
 *  	-------                                                             *
 *  	Copyright (C) 2004 Jiri Osoba <osoba@email.cz>                      *
 *      This is a part of ARGs project. See ARGs.h for copyright info.      *
 ****************************************************************************/

/****************************************************************************
 *      A R G S . C						            *
 ****************************************************************************/


#define ARGS_VERSION_STRING "1.0.6"
#define ARGS_DATE_STRING "2005/04/03"
#define ARGS_COPYRIGHT_STRING "\
Copyright (C) 2004-2005 Jiri Osoba <osoba@email.cz>"

#include "args.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <float.h>


extern void _floatconvert();
#pragma extref _floatconvert



const char *args_version(void)
{
    return (ARGS_VERSION_STRING);
}



const char *args_date(void)
{
    return (ARGS_DATE_STRING);
}



const char *args_copyright(void)
{
    return (ARGS_COPYRIGHT_STRING);
}



static char *args_get_choose(char *param, int value)
{
    static char a[256];
    char *p = a, *q;
    strcpy(a, param);
    while (1) {
	q = strchr(p, '/');
	if (value-- == 0) {
	    if (q)
		*q = 0;
	    return (p);
	}
	if (q == NULL)
	    return ("");
	p = q + 1;
    }
}


void args_print_help(const ARGS * args)
{
    int i, j;
    for (i = j = 0; args[i].type; i++)
	if (args[i].help) {
	    if (args[i].option && *args[i].option) {
		j += printf("%s%s", j ? ", " : "", args[i].option);
		if (args[i].param && *args[i].param)
		    switch (args[i].type) {
		    case ARGS_OPTFLOAT:
		    case ARGS_OPTINTEGER:
		    case ARGS_OPTCHOOSE:
			j += printf(" [%s]", args[i].param);
			break;
		    case ARGS_MULTISTRING:
		    case ARGS_STRING:
		    case ARGS_FLOAT:
		    case ARGS_CHOOSE:
		    case ARGS_INTEGER:
		    case ARGS_FILE:
		    case ARGS_PRESETCHOOSE:
			j += printf(" %s", args[i].param);
			break;
		    }
	    }
	    if (*args[i].help) {
		printf("%s%s", j < 8 ? "" : "\n", j ? "\t" : "");
		switch (args[i].type) {
		case ARGS_STRING:
		    printf(args[i].help, *(char **) args[i].value);
		    break;
		case ARGS_INTEGER:
		case ARGS_OPTINTEGER:
		    printf(args[i].help, *(int *) args[i].value);
		    break;
		case ARGS_FLOAT:
		case ARGS_OPTFLOAT:
		    printf(args[i].help, *(float *) args[i].value);
		    break;
		case ARGS_CHOOSE:
		case ARGS_OPTCHOOSE:
		    printf(args[i].help,
			   args_get_choose(args[i].param,
					   *(int *) args[i].value));
		    break;
		case ARGS_PRESET:
		    printf(args[i].help, (char *) args[i].value);
		    break;
		default:
		    printf(args[i].help);
		    break;
		}
		printf("\n");
		j = 0;
	    }
	}
}



static int args_test_option(char *argv, char *option)
{
    if (option == NULL)
	return (0);
    if (*option == 0)
	return (1);
    if (strncmp(argv, option, strlen(option)))
	return (0);
    if (isalpha(argv[strlen(option) - 1]) && isalpha(argv[strlen(option)]))
	return (0);
    return (1);
}



int args_parse_args(int argc, char **argv, const ARGS * args, char *separators)
{
    int i, j, k, old_i, l;
    char a[256], *p, *q, c, **d;
    FILE *f;
    for (i = 1; i < argc; i++) {
	for (j = 0; args[j].type; j++)
	    if (args_test_option(argv[i], args[j].option)) {
		/*
		   if (args[j].option
		   && !strncmp(argv[i], args[j].option,
		   strlen(args[j].option))
		   && (*args[j].option == 0
		   || !isalpha(argv[i][strlen(args[j].option)]))) {
		 */

		p = argv[i] + strlen(args[j].option);
		if (*p && strchr(separators, *p))
		    p++;
		switch (args[j].type) {
		case ARGS_SWITCH:
		    *(int *) args[j].value = args[j].add_value;
		    goto NEXT_ARG;
		case ARGS_OPTINTEGER:
		    old_i = i;
		    if (p == NULL || *p == 0)
			if (++i != argc)
			    p = argv[i];
		    *(int *) args[j].value = args[j].add_value;
		    if (*p == 'x') {
		      if (!sscanf (p+1, "%x", (int*)args[j].value))
			i = old_i;
		    } else
		    if (!sscanf(p, "%d", (int*)args[j].value))
			i = old_i;
		    goto NEXT_ARG;
		case ARGS_INTEGER:
		    for (k = 0; k < args[j].add_value; k++) {
			if (p == NULL || *p == 0) {
			    if (++i == argc)
				return (1);
			    else
				p = argv[i];
			}
			if (*p == 'x') {
			  if (sscanf (p+1, "%x", (int*)args[j].value + k))
			    if ((p = strchr(p, ',')) != NULL)
				p++;
			} else
			if (sscanf(p, "%d", (int *) args[j].value + k)) {
			    if ((p = strchr(p, ',')) != NULL)
				p++;
			} else
			    return (1);
		    }
		    goto NEXT_ARG;
		case ARGS_FLOAT:
		    for (k = 0; k < args[j].add_value; k++) {
			if (p == NULL || *p == 0) {
			    if (++i == argc)
				return (1);
			    else
				p = argv[i];
			}
			if (sscanf
			    (p, "%f%c", (float *) args[j].value + k, &c)) {
			    if (c == '%')
				*((float *) args[j].value + k) /= 100;
			    if ((p = strchr(p, ',')) != NULL)
				p++;
			} else
			    return (1);
		    }
		    goto NEXT_ARG;
		case ARGS_OPTFLOAT:
		    old_i = i;
		    if (p == NULL || *p == 0)
			if (++i != argc)
			    p = argv[i];
		    *(float *) args[j].value = args[j].add_value;
		    if (!sscanf(p, "%f%c", (float*)args[j].value, &c))
			i = old_i;
		    else if (c == '%')
			*(float *) args[j].value /= 100;
		    goto NEXT_ARG;
		case ARGS_STRING:
		    if (p == NULL || *p == 0) {
			if (++i == argc)
			    return (1);
			else
			    p = argv[i];
		    }
		    *(char **) args[j].value = strdup(p);
		    goto NEXT_ARG;
		case ARGS_MULTISTRING:
		    if (p == NULL || *p == 0) {
			if (++i == argc)
			    return (1);
			else
			    p = argv[i];
		    }
		    d = *(char ***) args[j].value;
		    if (d) {
			for (k = 0; d[k]; k++);
			d = (char **) realloc(d, (k + 2) * sizeof(char *));
		    } else {
			d = (char **) malloc(2 * sizeof(char *));
			k = 0;
		    }
		    d[k] = strdup(p);
		    d[k + 1] = NULL;
		    *(char ***) args[j].value = d;
		    goto NEXT_ARG;
		case ARGS_FILE:
		    if (p == NULL || *p == 0) {
			if (++i == argc)
			    return (1);
			else
			    p = argv[i];
		    }
		    if (*p == 0 || (f = fopen(p, "r")) == NULL)
			return (1);
		    d = (char **) malloc(2 * sizeof(char **));
		    d[0] = "";
		    d[1] = NULL;
		    k = 0;
		    while (!feof(f)) {
			*a = 0;
			fscanf(f, "%s", a);
			if (*a) {
			    for (k = 1; d[k]; k++);
			    d = (char **) realloc(d,
						  (k +
						   2) * sizeof(char *));
			    d[k] = strdup(a);
			    d[k + 1] = NULL;
			}
		    }
		    fclose(f);
		    k = args_parse_args(k + 1, d, args, separators);
		    for (l = 0; d[l]; l++)
			free(d[l]);
		    free(d);
		    if (k)
			return (1);
		    goto NEXT_ARG;
		case ARGS_PRESET:
		    p = (char *) args[j].value;
		  PRESET:
		    d = (char **) malloc(2 * sizeof(char **));
		    d[0] = "";
		    d[1] = NULL;
		    k = 0;
		    do {
			while (*p == ' ')
			    p++;
			if (*p) {
			    if (*p == '\"') {
				sscanf(p + 1, "%[^\"]", a);
				p += strlen(a) + 2;
			    } else {
				sscanf(p, "%[^ ]", a);
				p += strlen(a);
			    }
			    for (k = 1; d[k]; k++);
			    d = (char **) realloc(d, (k +
						      2) * sizeof(char *));
			    d[k] = strdup(a);
			    d[k + 1] = NULL;
			}
		    } while (p && *p);
		    k = args_parse_args(k + 1, d, args, separators);
		    for (l = 0; d[l]; l++)
			free(d[l]);
		    free(d);
		    if (k)
			return (1);
		    goto NEXT_ARG;
		case ARGS_PRESETCHOOSE:
		    if (p == NULL || *p == 0) {
			if (++i == argc)
			    return (1);
			else
			    p = argv[i];
		    }
		    for (k = 0; *(q = args_get_choose(args[j].param, k));
			 k++)
			if (!strcmp(p, q)) {
			    p = *((char **) args[j].value + k);
			    goto PRESET;
			}
		    if (isdigit(*p) && atoi(p) < (int) args[j].add_value) {
			p = *((char **) args[j].value + atoi(p));
			goto PRESET;
		    }
		    return (1);
		case ARGS_OPTCHOOSE:
		case ARGS_CHOOSE:
		    old_i = i;
		    if (p == NULL || *p == 0) {
			if (++i == argc) {
			    if (args[j].type == ARGS_CHOOSE)

				return (1);
			} else
			    p = argv[i];
		    }
		    for (k = 0; *(q = args_get_choose(args[j].param, k));
			 k++)
			if (!strcmp(p, q)) {
			    *(int *) args[j].value = k;
			    goto NEXT_ARG;
			}
		    if (isdigit(*p) && atoi(p) < (int) args[j].add_value) {
			*(int *) args[j].value = atoi(p);
			goto NEXT_ARG;
		    }
		    if (args[j].type == ARGS_OPTCHOOSE) {
			*(int *) args[j].value = args[j].add_value;
			i = old_i;
			goto NEXT_ARG;
		    }
		    return (1);
		}
	    }
	return (1);
      NEXT_ARG:;
    }
    return (0);
}
