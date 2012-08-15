/*
 * $Id: screen.c,v 1.8 1996/10/02 20:09:30 vons Exp $
 *
 * screen.c -- manage the display of the command line.
 */

#include "config.h"
#include "vrl.h"


#define MARGIN	3	/* use margin of 3 chars */

PROTOTYPE(static void calc_wind, (size_t curpos,
                                  size_t *ws, size_t *wl, size_t *wcp));
PROTOTYPE(static size_t convert, (const char *, size_t));
PROTOTYPE(static void wipe, (size_t));

static int force_full_update;
static struct str ol, *oline = &ol; /* complete output line */
static size_t oline_len;

static size_t cur_wcurpos;
static size_t cur_wstart;
static size_t cur_wlen;
static struct str cur_wline;

static const char *prompt;
static size_t prompt_len;

void
scr_init()
{

    /*
     * init the buffers only once, will be re-used at every call.
     *
     * The initial sizes are random, too big is useless, and too small may
     * cause a lot of realloc's when they grow.
     *
     * (Using different value makes malloc-debugging easier, the size
     * identifies the buffer)
     */
    oline = &ol;
    str_init(oline, 98);
    str_init(&cur_wline, 99);
}

void
scr_setup(prompt_str)
const char *prompt_str;
{
    cur_wcurpos = 0;
    cur_wstart = 0;
    cur_wlen = 0;

    str_assign(&cur_wline, "");

    /* A null str means no prompt, i.e. a prompt of length zero */
    if (prompt_str == NULL)
	prompt_str = "";

    prompt = prompt_str;
    prompt_len = strlen(prompt);

    scr_invalidate();
    scr_update("", 0);
}

void
scr_invalidate()
{
    /* assume screen contents has changed */
    force_full_update = 1;
}

void
scr_update(li, in_curpos)
const char *li;                   /* line contents in internal format */
size_t    in_curpos;              /* pos of cursor on line, 0-based */
{
    size_t    new_wstart, new_wlen;
    size_t    out_curpos, new_wcurpos;
    const char *new_wline;

    /* convert li to readable output format, (i.e. ^X for control chars) */
    out_curpos = convert(li, in_curpos);

    /* Calculate window start position */
    calc_wind(out_curpos, &new_wstart, &new_wlen, &new_wcurpos);

    /*
     * Display output line, but only send the part of the line that
     * actually changed. Same for cursor, don't touch it if its position
     * is right.
     */
    new_wline = &(oline->str[new_wstart]);

    if (force_full_update)
    {
        int       clr_ok;

        force_full_update = 0;

        /*
         * Move cursor to leftmost position on line and clear it. If the
         * clear fails, rewrite the line and fill remainder with spaces.
         */
        tty_cr();
        clr_ok = tty_clrtoeol();

        out_str(prompt, prompt_len);
        out_str(new_wline, new_wlen);

        if (clr_ok)
            cur_wcurpos = prompt_len + new_wlen;
        else
        {
            wipe(tty_width() - new_wlen - prompt_len);
            cur_wcurpos = tty_width();
        }

        /* cursor will be positioned below */
    }
    else
    {
        int       i;
        ssize_t   diffstart;

        /*
         * Compare the current screen contents with the new one, trailing
         * nul included. If the current one is shorter, its trailing nul
         * will result in a difference.
         */
        for (i = 0, diffstart = -1; i <= new_wlen; i++)
            if (new_wline[i] != cur_wline.str[prompt_len + i])
            {
                diffstart = i;
                break;
            }


        /* Move to position of difference */
        if (diffstart != -1)
        {
            size_t    wdiffstart = prompt_len + diffstart;

            /* move cursor to first char to change */
            tty_setcurpos(wdiffstart, cur_wcurpos, cur_wline.str);

            /* send new data */
            out_str(&(new_wline[diffstart]), new_wlen - diffstart);
            cur_wcurpos = prompt_len + new_wlen;


            /*
             * clear the remainder (curpos included) if the new line is
             * shorter than the old one.
             */
            if (prompt_len + new_wlen < cur_wlen)
            {

                /*
                 * Try to clear, wipe() if needed.
                 */
                if (!tty_clrtoeol())
                {
                    wipe(cur_wlen - new_wlen - prompt_len);

                    /* wipe() leaves cursor at end of old line */
                    cur_wcurpos = cur_wlen;
                }
            }
        }
    }


    /* update current info */
    cur_wlen = prompt_len + new_wlen;

    str_assign(&cur_wline, prompt);
    str_add(&cur_wline, new_wline);

    /* reposition cursor if needed */
    if (prompt_len + new_wcurpos != cur_wcurpos)
    {
        tty_setcurpos(prompt_len + new_wcurpos, cur_wcurpos, cur_wline.str);
        cur_wcurpos = prompt_len + new_wcurpos;
    }

    out_flush();
}

static size_t
convert(li, icurpos)
const char *li;
size_t    icurpos;
{
    size_t    ii, oi;
    size_t    ocurpos = 0;        /* =0 just to shut up lint */

    oi = 0;

    for (ii = 0; li[ii]; ii++)
    {
        int       c = li[ii];

        str_grow(oline, oi + 4);  /* max bytes to add is 4 */

        if (c <= 26)
        {
            oline->str[oi++] = '^';
            oline->str[oi++] = (char) ('A' + c - 1);
        }
        else if (c == ESC)        /* ESC */
        {
            oline->str[oi++] = '^';
            oline->str[oi++] = '[';
        }
        else if (c == 127)        /* DEL */
        {
            oline->str[oi++] = '^';
            oline->str[oi++] = '?';
        }
        else if (c > 127)
        {
            oline->str[oi++] = '<';
            oline->str[oi++] = TOHEX((c >> 4) & 0x0f);
            oline->str[oi++] = TOHEX(c & 0x0f);
            oline->str[oi++] = '>';
        }
        else
        {
            oline->str[oi++] = (char) c;
        }

        if (icurpos == ii)
            ocurpos = oi - 1;
    }

    /* add trailing 0 */
    str_grow(oline, oi + 1);
    oline->str[oi] = 0;

    oline_len = oi;

    /* curpos may be after last char */
    if (icurpos == ii)
        ocurpos = oline_len;

    return ocurpos;
}

static void
calc_wind(curpos, new_wstart, new_wlen, new_wcurpos)
size_t    curpos;
size_t   *new_wstart;
size_t   *new_wlen;
size_t   *new_wcurpos;
{

    /*-
     * This calculation is based on the following rules:
     *
     * - right-align the line end if possible
     * - avoid scrolling the line
     * - avoid placing the cursor in the margins
     */
    size_t    ttywid = tty_width() - prompt_len;
    size_t    olen;

    /* if cursor is after EOL, include an extra char for it */
    olen = oline_len + (curpos == oline_len ? 1 : 0);

    /*
     * A big change has taken place if the old startpos is now after
     * the line-end. Just reset it and let the code below make the
     * best of it.
     */
    if (olen <= cur_wstart + MARGIN)
	cur_wstart = 0;

    /* Did cursor move off-screen on the left ? */
    if (curpos < cur_wstart + MARGIN)
    {
        if (curpos < MARGIN)
            *new_wstart = 0;
        else
            *new_wstart = curpos - MARGIN;
    }

    /* or did cursor move off on the right side ? */
    else if (curpos > cur_wstart + ttywid - MARGIN - 1)
    {
        *new_wstart = curpos - (ttywid - MARGIN - 1);
    }

    /* or maybe cursor still visible, but screen may be wider than last time */
    else if (cur_wstart + ttywid - MARGIN > olen)
    {
        /* line may be shorter than new screen width */
        if (olen < ttywid - MARGIN)
            *new_wstart = 0;
	else
	    *new_wstart = olen - (ttywid - MARGIN);
    }

    /* Else, no change, keep same window */
    else
    {
        *new_wstart = cur_wstart;
    }


    /* calculate length of text to display */
    *new_wlen = oline_len - *new_wstart;
    if (*new_wlen > ttywid)
        *new_wlen = ttywid;

    /* calculate cursor position on screen */
    *new_wcurpos = curpos - *new_wstart;

    cur_wstart = *new_wstart;
}

static void
wipe(cnt)
size_t    cnt;
{
    while (cnt--)
        (void) out_char(' ');
}
