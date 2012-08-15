/*
 * $Id: complete.c,v 1.8 1998/09/16 21:55:00 vons Exp $
 *
 * This file implements filename completion.
 */

#include "config.h"
#include "vrl.h"
#include "wildexp.h"
#include <ctype.h>

PROTOTYPE(static int do_glob, (wildexp_t * we, const char *expr));
PROTOTYPE(static void columnize, (wildexp_t * we));
PROTOTYPE(static int comp_comprefix, (wildexp_t * we));
PROTOTYPE(static void comp_print, (size_t l, const char *s));
PROTOTYPE(static void comp_outstr, (const char *s));

#ifdef MAX
#undef MAX
#endif

#define MAX(a,b)	((a) < (b) ? (b) : (a))

/*-------------------------------------------------------------------------
 * comp_complete -- implements filename completion. returns 1 if the
 *                  cmdline must be redrawn.
 */
int
comp_complete(flag)
int       flag;
{
    const char *linebuf = iline_linebuf();
    char     *glob_expr;
    size_t    expr_start;
    size_t    expr_len;
    size_t    curpos;
    int       i, num_matches;
    size_t    com_prefix;
    wildexp_t we;
    int       retval;

    curpos = iline_getpos();

    /*
     * Get the start and the length of the current word.
     *
     * The filename may contain slashes, dots, etcetera, so extend the word
     * definition with those chars.
     */
    expr_len = iline_curword_start(ILINE_CW_DONT_SKIP, "/\\-+.,~*?$");
    expr_start = iline_getpos();

    /* +2, one for '*' and one for '\0' */
    glob_expr = (char *) malloc((size_t) (expr_len + 2));

    /* Can't strcpy/strcat, since string is not nul-terminated */
    (void) memcpy(glob_expr, &(linebuf[expr_start]), expr_len);

    glob_expr[expr_len] = '\0';

    /* Add trailing '*' if and only if the user didn't use wildcards in
     * the last part of the expression.
     *
     * A "conf[TAB]" should match config.cache, but that name should
     * not turn up when using something like "conf*.c[TAB]". One
     * can argue that conf*.c is a prefix of config.cache, but it probably
     * isn't what a user would expect.
     */
    { 
	const char *p;

	/* get last part of the expression */
	if ((p = strrchr(glob_expr, DIRSEP)) == NULL)
	    p = glob_expr;

	/* add * if there's no wildcard */
	if (strchr(p, '*') == NULL && strchr(p, '?') == NULL)
	    (void) strcat (glob_expr, "*");
    }

    num_matches = do_glob(&we, glob_expr);

    /* Special case: user requested for all matches to be inserted
     * on the cmdline.
     */
    if (flag == COMP_INSERT_ALL && num_matches >= 1)
    {
        (void) free(glob_expr);

	/* move back to start of expr and replace it with first fname */
        iline_start(expr_start);
        iline_edit(0, expr_len, we.fname[0]);

	/* insert all matches, separated by a space */
	for(i = 1; i < we.cnt; i++)
	{
	    iline_edit(0, 0, " ");
	    iline_edit(0, 0, we.fname[i]);
	}

	wildexp_free(&we);

        return COMP_OK;
    }

    if (num_matches == 0)         /* no match at all... */
    {
        (void) free(glob_expr);
	wildexp_free(&we);

        iline_start(curpos);      /* restore cursor position */
        tty_beep();
        return COMP_OK;
    }
    else if (num_matches == 1)
    {
        com_prefix = strlen(we.fname[0]);
    }
    else
    {
        int       n_off;
        char     *fnam0;

        /*
         * Make sure all files are located in same dir. If not (i.e. we
         * got "/a/b1/c1" and "/a/b2/c2"), re-glob() again with the common
         * prefix ("/a/b" in the example).
         *
         * Could use strrchr(), but that would need a work-around for the
         * case where fnam[0] ends with a slash.
         */
        for (i = 0, n_off = 0, fnam0 = we.fname[0]; fnam0[i] != '\0'; i++)
            if (fnam0[i] == DIRSEP && fnam0[i + 1] != '\0')
                n_off = i + 1;

        com_prefix = comp_comprefix(&we);
        if (com_prefix < n_off)
        {

            /*
             * Files not all in same dir. Need to glob again using the
             * common prefix.
             */
            glob_expr = utl_realloc_str(glob_expr, we.fname[0]);
            glob_expr[com_prefix] = '*';
            glob_expr[com_prefix + 1] = '\0';

            wildexp_free(&we);

            num_matches = do_glob(&we, glob_expr);

            /* There are guaranteed to be > 1 matches */
            ASSERT(num_matches > 1);
        }
    }

    retval = COMP_OK;
    switch (flag)
    {
      case COMP_NOLIST:
        break;

      case COMP_LIST_NOCHANGE:
        /* if com prefix != glob_expr, something changed so no list */
        if (strncmp(glob_expr, we.fname[0], com_prefix) != 0)
            break;

        /* FALLTHROUGH */

      case COMP_LIST_IF_AMBIGUOUS:
        if (num_matches > 1)
        {
            columnize(&we);
            retval = COMP_DID_LIST;
        }
        break;
      case COMP_LIST_ONLY:
        /* Also list when there is only one completion possible */
        columnize(&we);
        retval = COMP_DID_LIST;
    }

    (void) free(glob_expr);

    /*
     * Replace the expr on the cmdline by the max unique expansion, as
     * defined by com_prefix.
     *
     * Don't modify the line if it was a list-only request, or when the
     * com_prefix is empty (it would remove the pattern from the line!)
     */

    if (flag == COMP_LIST_ONLY || com_prefix == 0)
    {
        /* restore the cursor position */
        iline_start(curpos);
    }
    else
    {
        /* replace expr on cmdline with new string */
        we.fname[0][com_prefix] = '\0';
        iline_start(expr_start);
        iline_edit(0, expr_len, (const char *) we.fname[0]);

        if (num_matches != 1)
            tty_beep();           /* ambiguous, tell user */
        else
        {

            /*
             * If the result of the expansion is a file, add a space. If
             * it is a directory, the slash is already there.
             *
             * However, this should only be done if there is no slash or
             * space on the line after the expr.
             */
            if (we.fname[0][com_prefix - 1] != DIRSEP)
            {
                /* Add space if it isn't there yet. If it is, put the 
		 * cursor after it.
		 */
                if (linebuf[iline_getpos()] != ' ')
                    iline_edit(0, 0, " ");
		else
		    iline_setpos(1);
            }
            else
            {

                /*
                 * remove slash if there are two of them, and put cursor
                 * after the one that 'survives'.
                 */
                if (linebuf[iline_getpos()] == DIRSEP)
                {
                    iline_edit(0, 1, (const char *) NULL);
                    iline_setpos(1);
                }
            }
        }
    }

    wildexp_free(&we);

    return retval;
}

/*----------------------------------------------------------------------------
 * do_glob -- do actual glob, return #matches
 */
static int
do_glob(we, expr)
wildexp_t  *we;
const char *expr;
{
    int       i;

    /* glob will expand ~user and add / to matching dirs */
    i = wildexp(expr, we);

    if (i != 0)
        out_printf("wildexp() failed");

    return we->cnt;
}

/*----------------------------------------------------------------------------
 * columnize -- display all files in glob struct in 1 or more columns
 */
static void
columnize(we)
wildexp_t   *we;
{
    char    **fnam = we->fname; /* ptr to filenames */
    size_t    nfnam = we->cnt;  /* number of filenames */

    int       ncol;               /* number of columns to use for display */
    int      *colwid;             /* will point to a table with column
                                   * widths */
    int       wid_accum;          /* accum width of all columns */
    int       inter_col;          /* space between columns */
    int       per_col;            /* number of files per column */
    size_t    n_off;              /* offset of fname in path(s) */
    int       i;
    int       num_cols = tty_width();

    /*
     * Apparently, the match is not unique, so we have to display all
     * possible expansions in one or more columns, like 'ls' does.
     *
     * Note that this is not a trivial thing: The number of columns depends
     * on the width of the screen, and the width of the columns. That
     * width however depens in turn on how the files are distributed, i.e.
     * on the number of columns.
     *
     * This chicken and egg problem is solved by calculating the max number
     * of columns based on the screen width and the shortest filenames in
     * the list. If this initial number is too high, the trial is
     * restarted with one column less.
     *
     * A pass over the filenames is needed to find out whether the
     * accumulated length of all filenames allows them to be placed on a
     * single line, and to find the shortest length so the max number of
     * columns can be calculated.
     */
    {
        size_t    l_accum, l_shortest;

        l_accum = l_shortest = strlen(fnam[0]);

        /* start with index 1, the first has been processed above */
        for (i = 1; i < nfnam; i++)
        {
            size_t    l = strlen(fnam[i]);

            l_accum += l;

            if (l < l_shortest)
                l_shortest = l;
        }

        /*
         * Find length of dir prefix (is same for all files) Could use
         * strrchr(), but that would need a work-around for the case where
         * fnam[0] ends with a slash.
         */
        for (i = 0, n_off = 0; fnam[0][i] != '\0'; i++)
            if (fnam[0][i] == DIRSEP && fnam[0][i + 1] != '\0')
                n_off = i + 1;

        /* Leave room for INTER_COL spaces between the columns */
#define INTER_COL	3

        /* common dir prefix will not be displayed, so exclude it */
        l_accum -= n_off * nfnam;
        l_shortest -= n_off;

        /* Do they fit on a single line ? */
        if (l_accum + (nfnam - 1) * INTER_COL < num_cols)
        {

            /*
             * Redistribute the whitespace. Try to get some between the
             * last fname and the margin, might look better.
             */
            inter_col = MAX((num_cols - l_accum) / nfnam, INTER_COL);

            out_nl();

            for (i = 0; i < nfnam - 1; i++)
            {
                comp_outstr(&(fnam[i][n_off]));
                comp_print((size_t) inter_col, "");
            }

            comp_outstr(&(fnam[i][n_off]));
            out_nl();

            return;
        }

        /*
         * They don't all fit on a single line. Calc max number of columns
         * that might be needed. The MAX(.., 1) is needed in case there is
         * a filename that is longer than the screen is wide...
         */
        ncol = MAX(num_cols / (l_shortest + INTER_COL), 1);
    }

    /*
     * Start with ncol columns. Calculate the width of every column, and
     * check whether the accumulated width fits on the screen.
     *
     * If not, try it with one column less.
     */
    colwid = (int *) malloc(ncol * sizeof(int));

    for (;;)
    {
        int       col_start;
        int       co;

        /* calculate #files/column, don't forget to round up! */
        per_col = (nfnam + (ncol - 1)) / ncol;

        /*
         * Not all combinations of #cols and #files are possible. All
         * columns except the rightmost must be full, so distributing,
         * say, 14 files over 8 cols is impossible, you need at least 15
         * files for that.
         */
        if (!(per_col * (ncol - 1) < nfnam && nfnam <= per_col * ncol))
        {
            ncol--;
            continue;
        }

        wid_accum = 0;
        for (co = 0, col_start = 0; co < ncol; co++, col_start += per_col)
        {
            colwid[co] = 0;

            /* calc width of column */
            for (i = 0; i < per_col && col_start + i < nfnam; i++)
            {
                int       l = strlen(fnam[col_start + i]);

                if (l > colwid[co])
                    colwid[co] = l;
            }

            colwid[co] -= n_off;
            wid_accum += colwid[co];
        }

        /*
         * Break out when the columns fit, or when they don't but we only
         * have 1 column...
         */
        if (wid_accum + ncol * INTER_COL <= num_cols || ncol == 1)
            break;
        else
            ncol--;
    }

    /*
     * Redistribute the whitespace. Note that if there is a lot of
     * whitespace left, the rightmost column should also get some of it.
     * However, in some cases, there is no extra space, only the strict
     * minimum is available.
     */
    inter_col = MAX((num_cols - wid_accum) / ncol, INTER_COL);

    out_nl();

    for (i = 0; i < per_col; i++)
    {
        int       f = i;
        int       co;

        for (co = 0; co < ncol - 1; co++, f += per_col)
            comp_print((size_t) (colwid[co] + inter_col), &(fnam[f][n_off]));

        /* if there is a file for the last col, put it there */
        if (f < nfnam)
            comp_outstr(&(fnam[f][n_off]));

        out_nl();
    }

    (void) free(colwid);
}

/*----------------------------------------------------------------------------
 * comp_comprefix -- find prefix common to all file names.
 */
static int
comp_comprefix(we)
wildexp_t  *we;
{
    int       i, j;
    int       max_prefix;

    max_prefix = strlen(we->fname[0]);
    for (i = 1; i < we->cnt; i++)
    {
        for (j = 0; j < max_prefix; j++)
            if (we->fname[i][j] != we->fname[0][j])
                break;

        max_prefix = j;
    }

    return max_prefix;
}

/*----------------------------------------------------------------------------
 * comp_print -- output given string in fixed # of spaces
 */
static void
comp_print(l, s)
size_t    l;
const char *s;
{
    size_t    i, j;

    for (i = 0, j = 0; i < l; i++)
    {
        if (s[j] != '\0')
            (void) out_char(s[j++]);
        else
            (void) out_char((int) ' ');
    }
}
/*----------------------------------------------------------------------------
 * comp_outstr -- output given null-terminated string
 */
static void
comp_outstr(s)
const char *s;
{
    size_t    l = strlen(s);
    out_str(s, l);
}
