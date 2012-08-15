/*
 * $Id: rltest.c,v 1.2 1996/05/12 21:28:05 vons Exp $
 *
 * rltest.c -- test tool, ANSI-C only
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

extern char *readline(const char *p);
extern void add_history(const char *p);

int       main(int argc, char **argv);  /* shut up lint & co */

int
main(argc, argv)
int       argc;
char    **argv;
{
    char     *s;
    const char *prompt = "[rltest]: ";

    while ((s = readline(prompt)) != NULL)
    {
        add_history(s);
        (void) printf("%s\n", s);
        (void) free(s);
    }

    return 0;
}
