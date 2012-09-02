/*
 * $Id: vrl.c,v 1.8 1998/02/15 13:52:51 vons Exp $
 *
 * vrl.c -- contains entry points and main loop
 */

#include "config.h"
#include "vrl.h"

#include <signal.h>               /* kill() and SIGINT/... */


/* next prototypes are only to satisfy lint & co */
PROTOTYPE(char *readline, (const char *prompt));
PROTOTYPE(void add_history, (char *s));
PROTOTYPE(int rl_reset_terminal, (char *terminal_type));


PROTOTYPE(static const char *vrl_loop, (const char *prompt,
                                        int *sig, int no_hist));

/*--------------------------------------------------------------------
 * read line from tty. Allocate string, caller will free() the buffer
 * return NULL on EOF
 */
char     *
readline(prompt)
const char *prompt;
{
    const char *l;
    char     *duplicate;
    int       sig;

    {
        static int first_time = 1;

        if (first_time)
        {
            first_time = 0;

	    /*
	     * out_printf() is used for errormsgs, so make it work ASAP.
	     * It uses the tty output functions and calls scr_invalidate(),
	     * initialise those modules first.
	     */
            scr_init();
	    tty_get_capas((const char *) NULL);
            out_init();

            opt_read();
            iline_init();
        }
    }

    tty_setup();

    sigwinch_setup();

    l = vrl_loop(prompt, &sig, 0 /* hist allowed */ );

    sigwinch_restore();

    tty_newline();

    tty_restore();

    /* return a copy of the latest input line */
    if (l == NULL)
        duplicate = NULL;
    else
        duplicate = (char *) utl_realloc_str((char *) NULL, l);

    /*
     * If we need to send a signal, do it here, just before the return.
     * This way, the handler can call us without any problem.
     */
    if (sig != 0)
        (void) kill(getpid(), sig);

    return duplicate;
}


static const char *
vrl_loop(prompt, sig, no_hist)
const char *prompt;
int      *sig;
int       no_hist;
{
    int       sym, prevsym;
    int       in_history = 0;     /* user is moving in history ? */
    int       modified;           /* hist move ends on modif */


    /* tty is in raw mode. init iline stuff and start processing commands. */

    iline_replace((const char *) NULL); /* clear line */
    scr_setup(prompt);            /* initialise screen code */
    *sig = 0;                     /* default no signal */

    /* initialise sym variable so that prevsym will be initialised with
     * a reasonable value. Doesn't matter what sym, as long as it is
     * not SYM_DELETE.
     */
    sym = SYM_ENTER;

    for (;;)
    {
        int       break_while = 0;

        prevsym = sym;
        sym = sym_get();

        modified = 1;             /* modified unless stated otherwise */

        switch (sym)
        {
          case SYM_EOF:          /* end of file */
            if (strlen(iline_linebuf()) == 0)
                return NULL;
            /* FALLTHROUGH */

          case SYM_ENTER:        /* enter key, accept input */
            break_while = 1;
            break;

          case SYM_LINE_START:   /* cursor to start of line */
            iline_start(0);
            break;

          case SYM_LINE_END:     /* cursor to end of line */
            iline_end(0);
            break;

          case SYM_LEFT:         /* cursor left */
            iline_setpos(-1);
            break;

          case SYM_RIGHT:        /* cursor right */
            iline_setpos(1);
            break;

          case SYM_DELETE:       /* del char under cursor, leave cursor
                                  * in place */
	    /* if line is empty, and prev key was not a delete, return EOF */
            if (strlen(iline_linebuf()) == 0 && prevsym != SYM_DELETE)
                return NULL;

            iline_edit(0, 1, (const char *) NULL);
            break;

          case SYM_BACKSPACE:    /* del char left of cursor, cursor moves
                                   * left */
            if (iline_getpos() > 0)
                iline_edit(-1, 1, (const char *) NULL);
            else
                tty_beep();
            break;

          case SYM_KILL_LINE:    /* clear iline */
            iline_replace((const char *) NULL);
            break;

          case SYM_DEL_TO_EOL:   /* clear until end of line */
            iline_del2eol();
            break;

          case SYM_DELETE_WORD:  /* delete previous word */
            {
                size_t    wordlen;
                size_t    curpos = iline_getpos();

                wordlen = iline_curword_start(ILINE_CW_SKIP,
                                              (const char *) NULL);

                /*
                 * The wordlen does not include any whitespace between the
                 * end of the word and the cursor position. Adapt it if
                 * needed.
                 */
                if (iline_getpos() + wordlen < curpos)
                    wordlen = curpos - iline_getpos();

                iline_edit(0, wordlen, (const char *) NULL);
            }
            break;

          case SYM_PREVHIST:     /* move to previous line in history */
            if (no_hist)
            {
                tty_beep();
                break;
            }
            else
            {
                const char *hist_line;

                if (!in_history)
                {
                    /* move to most recent cmd in hist */
                    hist_line = hist_newest();

                    if (hist_line != NULL)
                    {
                        /* copy current line to save buffer */
                        iline_save();

                        in_history = 1;
                    }
                }
                else
                {
                    hist_line = hist_next_older();
                }

                modified = 0;
                if (hist_line == NULL)
                    tty_beep();
                else
                    iline_replace(hist_line);
            }
            break;

          case SYM_NEXTHIST:     /* move to next line in history */
            if (no_hist)
            {
                tty_beep();
                break;
            }
            else
            {
                const char *hist_line;

                if (!in_history)
                    tty_beep();
                else
                {
                    /* move to most next cmd in hist */
                    hist_line = hist_next_newer();

                    if (hist_line != NULL)
                    {
                        iline_replace(hist_line);
                        modified = 0;
                    }
                    else
                    {
                        /* there is no next, restore the iline */
                        in_history = 0;
                        iline_undo();
                    }
                }
            }
            break;

          case SYM_HIST_SEARCH:  /* search in history */
            if (no_hist)
            {
                tty_beep();
                break;
            }
            else
            {

                /*
                 * Need to read a pattern from the tty. Just recurse, the
                 * old line contents is lost. but that doesn't seem to be
                 * a problem.
                 */
                const char *old_prompt;
                const char *pat;
                int       mysig;


                old_prompt = prompt;
                prompt = "Search: ";
                pat = vrl_loop(prompt, &mysig, 1 /* no hist */ );
                prompt = old_prompt;
                scr_setup(prompt);

                /* if the pattern entry was interrupted, return ... */
                if (mysig != 0)
                {
                    *sig = mysig;
                    break_while = 1;
                }
                else
                {
                    const char *hist_line;

                    /*
                     * if we're not already in the history, start at the
                     * end.
                     */
                    if (!in_history)
                    {
                        hist_line = hist_search(pat, HIST_SFIRST);
                        in_history = 1;
                    }
                    else
                    {
                        hist_line = hist_search(pat, HIST_SNEXT);
                    }

		    /* either display match or clear on failure */
		    iline_replace(hist_line);

                    if (hist_line == NULL)
                    {

                        /*
                         * Empty pattern and no pattern from previous run,
                         * or no match.
                         */
                        tty_beep();
                        in_history = 0;
                    }
                    else
                    {

                        /*
                         * State that line is not modified, avoids reset
                         * of in_history below.
                         */
                        modified = 0;
                    }
                }
            }
            break;

          case SYM_LIST_COMPLETE:/* list possible completions of current
                                   * word */
            if (comp_complete(COMP_LIST_ONLY))
                scr_invalidate();
            break;

          case SYM_DO_COMPLETE:  /* complete current word on cmdline */
            if (comp_complete(opt_autolist))
                scr_invalidate();
            break;

          case SYM_DO_COMPLETE_ALL:  /* insert all possible completions */
            (void) comp_complete(COMP_INSERT_ALL);
            break;

          case SYM_REDISPLAY:    /* redisplay the iline on the screen */
            scr_invalidate();
            break;

          case SYM_TOGGLE_INSMODE: /* toggle insert/overwrite mode */
            opt_insmode = (opt_insmode == 1 ? 0 : 1);
            break;

          case SYM_SIGINT:       /* send SIGINT to process */
            *sig = SIGINT;
            break_while = 1;
            iline_replace("");
            break;

          case SYM_SIGQUIT:      /* send SIGQUIT to process */
            *sig = SIGQUIT;
            break_while = 1;
            iline_replace("");
            break;

          case SYM_SIGTSTP:      /* send SIGTSTP to process */
            *sig = SIGTSTP;
            break_while = 1;
            iline_replace("");
            break;

          case SYM_INTR:	/* got intr, handle sigwinch */
            if (sigwinch_seen())
            {
		/* calculate new width, invalidate screen if changed */
                if (tty_update_term_width(1))
		    scr_invalidate();

#ifdef SIGWINCH
                /* pass signal to caller as well, before we return */
                *sig = SIGWINCH;
#endif
            }
            break;

	  case SYM_TRANSPOSE:	/* transpose char before&under cursor */
	    if (iline_getpos() == 0 ||
		(iline_peekch(0) == '\0' && iline_getpos() == 1))
		tty_beep();
	    else
	    {
		char b[2];

		if (iline_peekch(0) == '\0')
		     iline_setpos (-1);

		b[0] = (char) iline_peekch(-1); b[1] = '\0';
		iline_edit(-1, 1, (const char *) NULL);
		iline_edit( 1, 0, b);
	    }
	    break;

	  case SYM_REREAD_OPTIONS: /* re-read the options file */
	    opt_read();
	    break;

	  case SYM_VERSION: /* display version of vrl */
	    out_printf("Vrl version 1.3.3");
	    break;
          default:
            /* self-insert */
            {
                char      b[2];
                b[0] = (char) sym;
                b[1] = '\0';

                ASSERT(sym <= UCHAR_MAX);
                if (opt_insmode)
                    iline_edit(0, 0, b);  /* insert before cur char */
                else
                    iline_edit(0, 1, b);  /* replace cur char */
            }
            break;
        }

        /* end the loop ? */
        if (break_while)
            break;

        /*
         * If line is modified, we're no longer moving around in the
         * history.
         */
        if (modified)
            in_history = 0;

        /* make screen correspond to iline */
        scr_update(iline_linebuf(), iline_getpos());
    }

    /* return a ptr, readline() will allocate a copy */
    return iline_linebuf();
}

/*----------------------------------------------------------------------
 * add_history - add (copy of) given line to history
 */
void
add_history(li)
char     *li;
{
    hist_add(li);
}

/*----------------------------------------------------------------------
 * rl_reset_terminal - called when $TERM or $TERMCAP changes
 */
int
rl_reset_terminal(terminal_name)
char     *terminal_name;
{
    /* tty_setup() checks for this itself */
    tty_get_capas(terminal_name);
    return 0;
}
