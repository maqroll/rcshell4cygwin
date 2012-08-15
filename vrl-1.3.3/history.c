/*
 * $Id: history.c,v 1.3 1996/05/13 21:35:09 vons Exp $
 *
 * history.c -- handle history construction & access
 *
 * TODO: read/write history from/to file
 */

#include "config.h"
#include "vrl.h"

struct hist
{
    struct hist *next_older;
    struct hist *next_newer;
    struct str cmd;
};

/* history is stored in doubly-linked list, newest points to most recent elt */
static struct hist *oldest = NULL;
static struct hist *newest = NULL;
static struct hist *cur = NULL;

static int hist_cnt = 0;

const char *
hist_newest()
{
    /* return most recent hist cmd */
    cur = newest;
    return (cur == NULL ? NULL : cur->cmd.str);
}


const char *
hist_next_older()
{
    /* return next older hist cmd */
    ASSERT(cur != NULL);

    if (cur == oldest)
        return NULL;
    else
    {
        cur = cur->next_older;
        return cur->cmd.str;
    }
}

const char *
hist_next_newer()
{
    /* return next newer hist cmd */
    ASSERT(cur != NULL);

    if (cur == newest)
        return NULL;
    else
    {
        cur = cur->next_newer;
        return cur->cmd.str;
    }
}

void
hist_add(cmd)
const char *cmd;
{
    /* add given cmd to history */
    struct hist *hp;

    /* handle first insertion in list */
    if (newest == NULL)
    {
        newest = (struct hist *) malloc(sizeof(struct hist));
        ASSERT(newest != NULL);
        newest->next_older = newest->next_newer = NULL;
        str_init(&(newest->cmd), 0);
        str_assign(&(newest->cmd), cmd);
        oldest = newest;
        hist_cnt = 1;
        return;
    }

    /* List has at least 1 elt, find out whether cmd is already there */
    if (opt_histmode == OPT_HIST_NOREPEAT || opt_histmode == OPT_HIST_NODUPS)
    {
        /* nothing to do if cmd is same as last one */
        if (strcmp(newest->cmd.str, cmd) == 0)
            return;

        if (opt_histmode == OPT_HIST_NODUPS)
        {
            /* look further back */
            for (hp = newest; hp != NULL; hp = hp->next_older)
                if (strcmp(hp->cmd.str, cmd) == 0)
                    break;

            if (hp != NULL)
            {
                /* move older copy to head */
                if (hp == oldest)
                    oldest = oldest->next_newer;
                else
                    hp->next_older->next_newer = hp->next_newer;

                hp->next_newer->next_older = hp->next_older;
                hp->next_older = newest;
                hp->next_newer = NULL;
                newest->next_newer = hp;
                newest = hp;

                return;
            }
        }
    }

    /* cmd must be added to list */

    if (hist_cnt < opt_histmax)
    {
        /* allocate new elt */
        hp = (struct hist *) malloc(sizeof(struct hist));
        ASSERT(hp != NULL);
        str_init(&(hp->cmd), 0);
        hist_cnt++;
    }
    else
    {
        /* The list is full, recycle last entry */
        hp = oldest;
        oldest = oldest->next_newer;
        oldest->next_older = NULL;
    }

    hp->next_older = newest;
    newest = hp;
    newest->next_newer = NULL;
    newest->next_older->next_newer = newest;
    str_assign(&(newest->cmd), cmd);
}

const char *
hist_search(pat, mode)
const char *pat;
int       mode;
{
    static char *pattern = NULL;
    const char *s;
    int       start_only;
    size_t    plen;

    if (pat != NULL && pat[0] != '\0')
        pattern = utl_realloc_str(pattern, pat);

    /* no pattern given, and no pattern from previous run */
    if (pattern == NULL)
        return NULL;

    if (*pattern == '^')
    {
        start_only = 1;
        pat = pattern + 1;
    }
    else
    {
        start_only = 0;
        pat = pattern;
    }

    if (mode == HIST_SFIRST)
        s = hist_newest();
    else
        s = hist_next_older();

    plen = strlen(pat);

    if (start_only)
	while (s != NULL && strncmp(s, pat, plen) != 0)
	    s = hist_next_older();
    else
	while (s != NULL && strstr(s, pat) == NULL)
	    s = hist_next_older();

    return s;
}
