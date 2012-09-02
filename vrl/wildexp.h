/*
 * $Id: wildexp.h,v 1.1 1996/08/07 19:41:44 vons Exp $
 *
 * wildexp.h - defines data structure & prototypes
 */

#ifndef __WILDEXP_H__
#define __WILDEXP_H__

typedef struct _wildexp
{
    int cnt;		/* # of filenames returned in names */
    char **fname;
    char *nambuf;	/* buf with \0-separated file names */
} wildexp_t;

PROTOTYPE(int wildexp, (const char *, wildexp_t *));
PROTOTYPE(void wildexp_free, (wildexp_t *));

#define WE_EMEM (-1)

#endif /*  __WILDEXP_H__ */
