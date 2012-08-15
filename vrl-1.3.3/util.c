/*
 * $Id: util.c,v 1.5 1996/05/22 20:08:24 vons Exp $
 *
 * util.c -- misc utility functions
 */

#include "config.h"
#include "vrl.h"

#define VRL_MAXPATHLEN_DEF	1024

/*----------------------------------------------------------------------------
 * utl_reallocate -- reallocate 'size' bytes of memory.
 */
void     *
utl_reallocate(oldptr, size)
void     *oldptr;
size_t    size;
{
    void     *x;

    if (oldptr == (void *) NULL)
        x = (void *) malloc(size);
    else
        x = (void *) realloc(oldptr, size);

    return (x);
}


/*----------------------------------------------------------------------------
 * utl_realloc_str --  duplicate the given string, storing it in a realloc-ed
 *                     buffer. Returns the address of the copy.
 */
char     *
utl_realloc_str(old, str)
char     *old;
const char *str;
{
    /* utl_reallocate() will allocate when old == NULL */
    old = (char *) utl_reallocate(old, strlen(str) + 1);

    (void) strcpy(old, str);
    return (old);
}

/*--------------------------------------------------------------------------
 * utl_maxpathlen -- find decent value for buffers containing file names
 */
size_t
utl_maxpathlen()
{
    static size_t cached_pathmax = 0;

#ifdef PATH_MAX                   /* POSIX, but voluntary */
    size_t    pathmax_default = PATH_MAX;
#else
#ifdef MAXPATHLEN                 /* BSD */
    size_t    pathmax_default = MAXPATHLEN;
#else
    size_t    pathmax_default = VRL_MAXPATHLEN_DEF; /* Just guess ... */
#endif
#endif

    if (cached_pathmax != 0)
        return (cached_pathmax);

#ifdef HAVE_PATHCONF
    {
        long      pathmax;

        pathmax = pathconf("/", _PC_PATH_MAX);
        if (pathmax < 0)
        {
            cached_pathmax = pathmax_default;
        }
        else
        {
            /* add one to pathmax since it's relative to root */
            cached_pathmax = (size_t) pathmax + 1;
        }
    }
#else
    /* use the default selected above */
    cached_pathmax = pathmax_default;
#endif

    return (cached_pathmax);
}


#ifndef HAVE_MEMMOVE
/*-------------------------------------------------------------------------
 * memmove -- copy possibly overlapping memory areas.
 *	      Amazing that there are still systems that do not have this
 *	      function! (SunOS...)
 */
void     *
memmove(s1, s2, n)
void     *s1;
const void *s2;
size_t    n;
{
    char     *sc1;
    const char *sc2;

    sc1 = (char *) s1;
    sc2 = (const char *) s2;
    if (sc2 < sc1 && sc1 < sc2 + n) /* copy backwards */
        for (sc1 += n, sc2 += n; 0 != n; --n)
            *--sc1 = *--sc2;
    else if (sc1 != sc2)          /* copy forwards */
        for (; 0 != n; --n)
            *sc1++ = *sc2++;
    return (s1);
}
#endif                            /* HAVE_MEMMOVE */

#ifndef HAVE_STRERROR
/*--------------------------------------------------------------------------
 *
 * strerror() function for those who don't have it. Given an errno value,
 * it returns a description of the error.
 */

#include <stdio.h>
#include <errno.h>

extern int sys_nerr;
extern char *sys_errlist[];

static char msg[80];

char     *
strerror(errnum)
int       errnum;
{

    if ((errnum >= 0) && (errnum < sys_nerr))
        return sys_errlist[errnum];

    (void) sprintf(msg, "Unknown errno value %i\n", errnum);
    return msg;
}
#endif

#ifndef HAVE_STRSTR
/*--------------------------------------------------------------------------
 *
 * strstr() function for those who don't have it. given str1 and str2,
 * return adress of str2 in str1 if it matches, NULL if not.
 */
#include <string.h>

char     *
strstr(big, little)
const char *big;
const char *little;
{
    size_t    l_l = strlen(little);

    for (; *big != '\0'; big++)
        if (*big == *little && strncmp(big, little, l_l) == 0)
            return big;

    return NULL;
}
#endif
