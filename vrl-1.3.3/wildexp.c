/*
 * $Id: wildexp.c,v 1.3 1996/10/02 19:11:56 vons Exp $
 *
 * wildexp.c - expand wildcard and return matching filenames
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "vrl.h"

#include "wildexp.h"

typedef struct _we_priv
{
    char     *nambuf;             /* address of fname buffer */
    size_t    nambuf_size;        /* alloced size */
    size_t    nambuf_used;        /* #bytes used */
    size_t    cnt;                /* #fnames in buf */
    int       err;
}         we_priv;

static const char dirsep_str[] = {DIRSEP, '\0'};

#define ADD_DIR 1
#define ADD_FILE 2

#define is_alnum(c)     (('a' <= c && c <= 'z') || \
                         ('A' <= c && c <= 'Z') || \
                         ('0' <= c && c <= '9') || \
                         ( c == '_'))

PROTOTYPE(static void var_expand, (const char *, char *));
PROTOTYPE(static void recurse, (char *, char *, we_priv *));
PROTOTYPE(static int is_match, (const char *, const char *));
PROTOTYPE(static void add_to_list, (we_priv *, const char *, int));
PROTOTYPE(static int qcmp, (const void *s1, const void *s2));

int
wildexp(wildcard, we)
const char *wildcard;
wildexp_t *we;
{
    char     *expr;
    we_priv  *wep;
    char     *path;

    if (wildcard == NULL || wildcard[0] == '\0')
    {
        we->cnt = 0;
        return 0;
    }

    if ((wep = (we_priv *) malloc(sizeof(we_priv))) == NULL)
        return WE_EMEM;

    wep->cnt = 0;
    wep->err = 0;
    wep->nambuf_used = 0;
    wep->nambuf_size = 1024;      /* start with 1024-byte buffer */
    wep->nambuf = malloc(wep->nambuf_size);

    /* Alloc buffer in which the filenames will be constructed */
    path = malloc(utl_maxpathlen() + 1);

    /* Alloc buffer for expanded expression */
    expr = malloc(utl_maxpathlen() + 1);

    if (wep->nambuf == NULL || path == NULL || expr == NULL)
    {
        if (wep->nambuf != NULL)
            (void) free(wep->nambuf);
        if (path != NULL)
            (void) free(path);
        (void) free(wep);
        return WE_EMEM;
    }

    /*
     * Expand any environment variables in the wildcard string. If not
     * requested, we still need to copy the string since it will be
     * modified.
     */
    var_expand(wildcard, expr);

    (void) strcpy(path, "");
    recurse(path, expr, wep);

    (void) free(expr);
    (void) free(path);

    /* If there was a problem, clean up and return error */
    if (wep->err != 0)
    {
        int       err = wep->err;

        (void) free(wep->nambuf);
        (void) free(wep);
        return err;
    }

    /* Everything is ok, return the list of files */
    we->cnt = wep->cnt;
    we->nambuf = wep->nambuf;
    (void) free(wep);

    /* build ptr table */
    {
        int       i;
        char     *f = we->nambuf;

        we->fname = (char **) malloc((we->cnt + 1) * sizeof(char *));

        if (we->fname == NULL)
        {
            (void) free(we->nambuf);
            return WE_EMEM;
        }

        for (i = 0; i < we->cnt; i++)
        {
            we->fname[i] = f;
            f += strlen(f) + 1;
        }

        /* list ends with null-ptr */
        we->fname[we->cnt] = NULL;

        /* sort */
        qsort(we->fname, (size_t) we->cnt, sizeof(char *), qcmp);
    }

    return 0;
}

void
wildexp_free(we)
wildexp_t *we;
{
    (void) free(we->nambuf);
    (void) free(we->fname);
}

/*
 * var_expand - expand all references to existing environment variables.
 *              If a variable doesn't exist, its name is left as is.
 *
 * This handles $name and ${name}, where name is supposed to be made up of
 * alphanumeric characters and underscores.
 *
 * Note: there is no way to escape the $ character
 *
 * XXX: does not handle overflow in exp_out buffer and varname string.
 */
static void
var_expand(exp_in, exp_out)
const char *exp_in;
char     *exp_out;
{
    const char *pi = exp_in;
    char     *po = exp_out;


    while (*pi)
    {
        if (*pi != '$')
            *po++ = *pi++;
        else
        {
            int       in_brackets = 0;
            char      varname[256];
            char     *vp = varname;
            const char *p = pi + 1;
            const char *varval;

            if (*p == '{')
            {
                in_brackets = 1;
                p++;
            }

            while (is_alnum(*p))
                *vp++ = *p++;

            if (in_brackets && *p == '}')
                p++;

            *vp = '\0';

            if ((varval = getenv(varname)) != NULL)
            {
                /* copy contents of variable */
                while (*varval)
                    *po++ = *varval++;
                pi = p;
            }
            else
            {
                /* leave name as is, brackets included */
                while (pi != p)
                    *po++ = *pi++;
            }
        }
    }

    *po = '\0';
}

/*
 * recurse - scan given dir for files matching the wildcard.
 */
static void
recurse(path, wildcard, wep)
char     *path;
char     *wildcard;
we_priv  *wep;
{
    char     *wildcard_rest;
    DIR      *dirp;
    struct dirent *de;
    char     *path_end;


    /*
     * If nothing to match at this level, make sure the path is valid and
     * add it to the list of matches.
     *
     * If the stat fails, it might be a link to nowhere. In that case,
     * add it as a file nontheless so that completion will work.
     */
    if (wildcard == NULL)
    {
        struct stat sb;
        if (stat(path, &sb) == 0)
            add_to_list(wep, path, S_ISDIR(sb.st_mode) ? ADD_DIR : ADD_FILE);
	else if (lstat(path, &sb) == 0)
            add_to_list(wep, path, ADD_FILE);

        return;
    }

    /* find start of expr, up to / or \0 */
    if ((wildcard_rest = strchr(wildcard, DIRSEP)) != NULL)
        *wildcard_rest = '\0';

    /*
     * If the wildcard for this level is constant, go on to next level. If
     * the file/dir at this level does not exist, it will be detected
     * during the recursion.
     */
    if (strchr(wildcard, '?') == NULL && strchr(wildcard, '*') == NULL)
    {
        (void) strcat(path, wildcard);

        if (wildcard_rest == NULL)
            recurse(path, NULL, wep);
        else
        {

            /*
             * Add trailing (back)slash if there is another level to
             * match, but not if there already is one and this wildcard is
             * empty (avoids //// sequences)
             */
            if (wildcard[0] != '\0' || path[0] == '\0')
                (void) strcat(path, dirsep_str);

            *wildcard_rest = DIRSEP;
            recurse(path, wildcard_rest + 1, wep);
        }

        return;
    }

    /* If at top level with wildcard, open current dir */
    if (strlen(path) == 0)
        dirp = opendir(".");
    else
        dirp = opendir(path);

    /* Just return on error */
    if (dirp == NULL)
    {
        /* restore wildcard */
        if (wildcard_rest != NULL)
            *wildcard_rest = DIRSEP;
        return;
    }

    /* path_end points to trailing nul, i.e. after last dirsep */
    path_end = path + strlen(path);

    while ((de = readdir(dirp)) != NULL)
    {
        if (is_match(de->d_name, wildcard))
        {
            /* Only accept dot-files if wildcard starts with it */
            if (de->d_name[0] == '.' && wildcard[0] != '.')
                continue;

            /*
             * construct full path, add trailing slash if it should be
             * directory (i.e. if there is a wildcard_rest)
             */
            (void) strcpy(path_end, de->d_name);

            if (wildcard_rest == NULL)
                recurse(path, NULL, wep);
            else
            {
                (void) strcat(path_end, dirsep_str);
                recurse(path, wildcard_rest + 1, wep);
            }
        }
    }

    (void) closedir(dirp);
    *path_end = '\0';

    /* restore wildcard */
    if (wildcard_rest != NULL)
        *wildcard_rest = DIRSEP;
}

static int
is_match(nam, expr)
const char *nam;
const char *expr;
{
    while (*expr)
    {
        if (*expr == '?')
        {
            if (*nam == '\0')
                return 0;

            nam++, expr++;
        }
        else if (*expr != '*')
        {
            if (*nam == *expr)
                nam++, expr++;
            else
                return 0;
        }
        else
        {
            /* find out whether there is something behind the '*' */
            while (*expr == '*' || *expr == '?')
            {
                if (*expr == '?')
                {
                    if (*nam == '\0')
                        return 0;
                    nam++;
                }

                expr++;
            }

            if (*expr == '\0')
                return 1;
            else
            {
                const char *s = nam - 1;

                while ((s = strchr(s + 1, *expr)) != NULL)
                {
                    if (is_match(s, expr))
                        return 1;
                }
                return 0;
            }
        }

    }

    return (*nam == '\0');
}

/*
 * add_to_list - add given file/dir to the list of matches
 */
static void
add_to_list(wep, path, flg)
we_priv  *wep;
const char *path;
int       flg;
{
    size_t    to_add = strlen(path) + 1;
    char     *p;

    /*
     * If it is a dir, it might have the trailing slash already. (e.g.
     * when the pattern ended with a slash)
     */
    if (flg == ADD_DIR && path[to_add - 1] != DIRSEP)
        to_add++;

    /*
     * If it doesn't fit in the buffer, make it bigger so that it can
     * contain this fname, and 1 extra KB for those names that will follow
     */
    if (wep->nambuf_used + to_add > wep->nambuf_size)
    {
        wep->nambuf_size = wep->nambuf_size + to_add + 1024;

        p = realloc(wep->nambuf, wep->nambuf_size);
        if (p == NULL)
        {
            wep->err = WE_EMEM;
            return;
        }
        wep->nambuf = p;
    }
    p = wep->nambuf + wep->nambuf_used;

    (void) strcpy(p, path);
    if (flg == ADD_DIR)
        (void) strcat(p, dirsep_str);

    wep->nambuf_used += to_add;
    wep->cnt++;
}

/*
 * qcmp - compare two strings. used by qsort()
 */
static int
qcmp(s1, s2)
const void *s1;
const void *s2;
{
    return strcmp(*(const char *const *) s1, *(const char *const *) s2);
}
