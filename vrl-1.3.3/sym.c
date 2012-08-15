/*
 * $Id: sym.c,v 1.8 1998/09/13 11:00:35 vons Exp $
 *
 * sym.c -- read keys from input and map them to symbols.
 *
 * Problems like one keyseq being a prefix of another one, or the usage
 * of the same keyseq for multiple symbols is the responsability of the
 * one that fills the keyseq_to_key[] table.
 */

#include "config.h"
#include "vrl.h"

#define SYM_KEYSEQ_MAX	10        /* max len for key sequence */

#define EOFCHAR	0

/*
 * Next table will contain the mapping from input bytes to keys
 */
struct t_keyseq_to_key
{
    int       key;                /* key id */
    const char *keyseq;           /* key sequence */
};

static struct t_keyseq_to_key keyseq_to_key[] =
{
    { KEY_LEFT, NULL },           /* Tty special keys, depending on the */
    { KEY_RIGHT, NULL },          /* tty's type from the termcap file.  */
    { KEY_PREV, NULL },
    { KEY_NEXT, NULL },
    { KEY_DELETE, NULL },
    { KEY_INSERT, NULL },
    { KEY_BACKSPACE, NULL },
    { KEY_HOME, NULL },
    { KEY_END, NULL },

    { KEY_VERASE, NULL },          /* chars that have a special function */
    { KEY_VKILL, NULL },
    { KEY_VEOF, NULL },
    { KEY_VWERASE, NULL },
    { KEY_VLNEXT, NULL },
    { KEY_VREPRINT, NULL },

    { KEY_VINTR, NULL },           /* chars that send signals */
    { KEY_VQUIT, NULL },
    { KEY_VSUSP, NULL },

    { 0, NULL }
};

/* ANSI cursor keys (for ansicursorkeys option) */
static struct t_keyseq_to_key ansicur_to_key[] =
{
    { KEY_PREV , "\033[A" },
    { KEY_NEXT , "\033[B" },
    { KEY_RIGHT, "\033[C" },
    { KEY_LEFT , "\033[D" },
    { 0, 0}
};

/*
 * Next table maps keys to symbols/functions.
 *
 * It is a mix of the tty's special keys (Insert, Home, Arrow-keys, ...),
 * emacs-style commands, and the tty's configured erase/kill/... keys.
 */
static struct t_key_to_sym
{
    int       sym;                /* sym it generates */
    int       key;                /* key id */
}
          key_to_sym[] =
{
/* First the tty's special keys */

    { SYM_LEFT, KEY_LEFT },             /* cursor-left */
    { SYM_RIGHT, KEY_RIGHT },           /* cursor-right */
    { SYM_PREVHIST, KEY_PREV },         /* previous-history */
    { SYM_NEXTHIST, KEY_NEXT },         /* next-history */
    { SYM_DELETE, KEY_DELETE },         /* delete-char */
    { SYM_BACKSPACE, KEY_BACKSPACE },   /* delete-backward */
    { SYM_TOGGLE_INSMODE, KEY_INSERT }, /* toggle insert/overwr mode */
    { SYM_LINE_START, KEY_HOME },       /* goto start-of-line */
    { SYM_LINE_END, KEY_END },          /* goto end-of-line */

/* The emacs-style keys */

    { SYM_LINE_START, CNTRL('A') },     /* beginning-of-line */
    { SYM_LEFT, CNTRL('B') },           /* backward-char */
/*              CNTRL('C')            *//* reserved, usually the tty's intr char */
    { SYM_DELETE, CNTRL('D') },         /* delete-char (EOF on empty line) */
    { SYM_LINE_END, CNTRL('E') },       /* end-of-line */
    { SYM_RIGHT, CNTRL('F') },          /* forward-char */
    { SYM_BACKSPACE, CNTRL('H') },      /* delete-backward */
    { SYM_DO_COMPLETE, CNTRL('I') },    /* do-complete */
    { SYM_ENTER, CNTRL('J') },          /* accept-line */
    { SYM_DEL_TO_EOL, CNTRL('K') },     /* delete-to-end-of-line */
    { SYM_REDISPLAY, CNTRL('L') },      /* redisplay-line */
    { SYM_ENTER, CNTRL('M') },          /* accept-line */
    { SYM_NEXTHIST, CNTRL('N') },       /* next-history */
    { SYM_REREAD_OPTIONS, CNTRL('O') }, /* re-read options file */
    { SYM_PREVHIST, CNTRL('P') },       /* previous-history */
    { SYM_HIST_SEARCH, CNTRL('R') },    /* emacs-like hist search */
    { SYM_TRANSPOSE, CNTRL('T') },      /* transpose chars */
    { SYM_KILL_LINE, CNTRL('U') },      /* kill whole line */
    { SYM_LITERAL_NEXT, CNTRL('V') },   /* literal-next */
    { SYM_DELETE_WORD, CNTRL('W') },    /* word-delete */

/* The tty's (w)erase/kill/eof keys. */

    { SYM_KILL_LINE, KEY_VKILL },       /* kill whole line (usually ^U) */
    { SYM_BACKSPACE, KEY_VERASE },      /* delete-backward (  ,,  ^H/^?) */
    { SYM_EOF, KEY_VEOF },              /* end-of-input    (  ,, ^D) */
    { SYM_DELETE_WORD, KEY_VWERASE },   /* delete-word     (  ,, ^W) */
    { SYM_LITERAL_NEXT, KEY_VLNEXT },   /* literal-next    (  ,, ^V) */
    { SYM_REDISPLAY, KEY_VREPRINT },    /* redisplay line  (  ,, ^R) */
    { SYM_SIGTSTP, KEY_VSUSP },         /* suspend process (  ,, ^Z) */
    { SYM_SIGINT, KEY_VINTR },          /* interrupt process( ,, ^C) */
    { SYM_SIGQUIT, KEY_VQUIT },         /* quit            (  ,, ^\) */
    { 0, 0 }
};

static struct t_keyseq_to_sym
{
    int       sym;                /* symbol */
    const char *keyseq;           /* key sequence */
}
          keyseq_to_sym[] =
{
    { SYM_LIST_COMPLETE, "\033\033" },  /* list possible completions */
    { SYM_LIST_COMPLETE, "\033?" },     /* list possible completions */
    { SYM_DO_COMPLETE_ALL, "\033*" },   /* insert all possible completions */
    { SYM_VERSION, "\033v" },           /* display vrl version */
    { 0, 0 }
};

static struct t_lifo
{
    int       top;                /* index where next key will be written */
    int       buf[SYM_KEYSEQ_MAX];
} lifo = { 0 };

#define MATCH_NONE    (2)
#define MATCH_PARTIAL (3)
#define MATCH_FULL    (4)

PROTOTYPE(static int sym_getch, (int flg));
PROTOTYPE(static void sym_pushback, (size_t cnt, const unsigned char *s));
PROTOTYPE(static int sym_lookup_sym, (int key));
PROTOTYPE(static int try_match, (int, const char *, const Uchar *, size_t cnt));

static int
sym_getch(flg_wait)
int       flg_wait;
{
    if (lifo.top != 0) /* buf not empty */
    {
        if (lifo.buf[--lifo.top] == EOFCHAR)
            return TTY_EOF;
        else
            return lifo.buf[lifo.top];
    }
    else
        return tty_getch(flg_wait);
}

static void
sym_pushback(cnt, s)
size_t    cnt;
const unsigned char *s;
{
    while (cnt)
    {
        lifo.buf[lifo.top++] = (unsigned char) s[--cnt];

        /* debug test: make sure lifo doesn't overflow */
        ASSERT(lifo.top != SYM_KEYSEQ_MAX);
    }
}

static int
sym_lookup_sym(k)
int       k;
{
    /* look up the corresponding symbol and return it */
    int       i;

    for (i = 0; key_to_sym[i].sym != 0; i++)
        if (key_to_sym[i].key == k)
            return key_to_sym[i].sym;

    /* key is not special, return it as is */

    ASSERT(0 < k && k < 256);
    return k;
}

static int
try_match(mresult, str, buf, cnt)
int mresult;
const char *str;
const Uchar *buf;
size_t cnt;
{
    size_t l;

    /* No match of no string, buf longer than str, or cmp fails.
     * In that case, just return the old code unchanged.
     */
    if (str == NULL || (l = strlen(str)) < cnt
		    || strncmp(str, (const char *) buf, cnt) != 0)
	return mresult;

    /* buf is prefix or match of this keyseq */
    return (l == cnt ? MATCH_FULL : MATCH_PARTIAL);
}

int
sym_get()
{
    /* read chars from tty until sym is complete or timeout */

    Uchar     buf[SYM_KEYSEQ_MAX];
    size_t    cnt = 0;
    int       ch;
    int       sym = 0; /* XXX: =0 to silence lint */

    ch = sym_getch(TTY_WAIT);

    for (;;)
    {
        if (ch == TTY_TIMEOUT)
        {

            /* Can't complete function key, return all chars one by one */
            sym_pushback(cnt, buf);

            /* Next won't wait, we just pushed something back */
            return sym_lookup_sym(sym_getch(TTY_WAIT));
        }
        else if (ch == TTY_EOF)
        {
            /* End of input... */

            if (cnt == 0)
                return SYM_EOF;
            else
            {

                /*
                 * If there are pending chars, return them one by one, but
                 * don't forget we've seen EOF
                 */
                buf[cnt++] = EOFCHAR;
                sym_pushback(cnt, buf);

                /* Next won't wait, we just pushed something back! */
                return sym_lookup_sym(sym_getch(TTY_WAIT));
            }
        }
        else if (ch == TTY_INTR)
        {
            return SYM_INTR;
        }
        else
        {
            /* Got something, try to map it to a unique key */
            int       key = MATCH_NONE;
            int       i;

            buf[cnt++] = (Uchar) ch;

            for (i = 0; keyseq_to_key[i].key != 0; i++)
            {
		key = try_match(key, keyseq_to_key[i].keyseq, buf, cnt);

		if (key == MATCH_FULL)
		{
		    key = keyseq_to_key[i].key;
		    break;
		}
            }

	    /*
	     * If it's not a key, it may be an ansi cursor key
	     */
	    if ((key == MATCH_NONE || key == MATCH_PARTIAL) && opt_ansicur)
	    {
		for (i = 0; ansicur_to_key[i].key != 0; i++)
		{
		    key = try_match(key, ansicur_to_key[i].keyseq, buf, cnt);

		    if (key == MATCH_FULL)
		    {
			key = ansicur_to_key[i].key;
			break;
		    }
		}
            }

	    /*
	     * Maybe it's a meta-cmd.
	     */
            for (i = 0; keyseq_to_sym[i].sym != 0; i++)
            {
		key = try_match(key, keyseq_to_sym[i].keyseq, buf, cnt);

		if (key == MATCH_FULL)
		{
		    sym = keyseq_to_sym[i].sym;
		    break;
		}
            }


            if (key == MATCH_NONE)
            {

                /*
                 * The current input str doesn't match anything. return it
                 * one by one.
                 */
                sym_pushback(cnt, buf);

                sym = sym_lookup_sym(sym_getch(TTY_WAIT));
                break;
            }
            else if (key == MATCH_PARTIAL)
            {

                /*
                 * we have a prefix of some function key. Need to read
                 * some more to make sure.
                 */
                ch = sym_getch(TTY_TIMER);
            }
            else
            {
                /* Complete match. Sym is already set if key == MATCH_FULL */
		if (key != MATCH_FULL)
                    sym = sym_lookup_sym(key);
                break;
            }
        }
    }

    /*
     * special case: if the literal-next command is given, sym lookup must
     * be avoided for next char.
     */
    if (sym == SYM_LITERAL_NEXT)
    {
        sym = sym_getch(TTY_WAIT);
    }

    return sym;
}

/*
 * sym_define_key -- add new keys to the keyseq_to_key table. Used to
 *                   dynamically add the (possibly changing) definitions
 *                   of the tty's special home/end/cursor/... keys.
 *                   This can happen when the user changes $TERM, or plays
 *                   with the 'stty' command.
 *
 * NOTE1: a new key definition is only accepted when the keyseq isn't
 *        one of vrl's own keys, to avoid that built-in functions suddenly
 *        stop working because they become hidden. This might happen for
 *        example when the tty's reprint char is set to ^R, it would break
 *        the history-search.
 * NOTE2: caller should have allocated space for the keyseq, we don't copy
 *        it here!
 */
void
sym_define_key(k, keyseq)
int       k;
const char *keyseq;
{
    int       i;

    ASSERT(keyseq == NULL || strlen(keyseq) < SYM_KEYSEQ_MAX);

    /*
     * If the keyseq has length 1, check key_to_sym[]. Else, check
     * keyseq_to_sym[].
     * Setting the keyseq to NULL has the effect of removing the current
     * mapping for this special key.
     */
    if (keyseq != NULL)
    {
        if(strlen(keyseq) == 1)
        {
            for (i = 0; key_to_sym[i].sym != 0; i++)
                if (key_to_sym[i].key == (Uchar) keyseq[0])
                {
                    keyseq = NULL;
                    break;
                }
        }
        else
        {
            for (i = 0; keyseq_to_sym[i].sym != 0; i++)
                if (strcmp(keyseq_to_sym[i].keyseq, keyseq) == 0)
                {
                    keyseq = NULL;
                    break;
                }
        }
    }

    /*
     * Look up the key and change the keyseq that generates it.
     */
    for (i = 0; keyseq_to_key[i].key != 0; i++)
    {
        if (keyseq_to_key[i].key == k)
            keyseq_to_key[i].keyseq = keyseq;
    }
}
