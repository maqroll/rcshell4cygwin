/*
 * $Id: tty.c,v 1.15 2000/05/10 21:10:23 vons Exp $
 *
 * tty.c -- handle tty initialisation, capabilities, and keys
 */

#include "config.h"
#include "vrl.h"

#include <errno.h>
#include <stdio.h>

#ifdef HAVE_FCNTL_H
#include <fcntl.h>		/* O_NONBLOCK, F_GETFL, F_SETFL */
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>            /* ioctl() */
#endif

#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
 /* good question... */
#endif


#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#else
extern    PROTOTYPE(char *tgetstr, (const char *, char **));
extern    PROTOTYPE(char *tgoto, (const char *, int, int));
extern    PROTOTYPE(int tgetent, (char *, const char *));
extern    PROTOTYPE(int tgetflag, (const char *));
extern    PROTOTYPE(int tgetnum, (const char *));
extern    PROTOTYPE(int tputs, (const char *, int, int (*) (int)));
#endif

#ifdef HAVE_TERMCAP_VARS
#ifndef HAVE_TERMCAP_DEFNS
extern char PC;                   /* used by tputs () as padd character */
extern char *UP;                  /* used by tgoto */
extern char *BC;                  /* used by tgoto */
extern short ospeed;              /* used by tputs () */
#endif
#else
char      PC;
char     *UP;
char     *BC;
short     ospeed;
#endif

/* Manpages say 1024 bytes is enough, but it ain't for some $TERMCAP's I've
 * seen...
 */
#define TERMCAP_BSIZE	2048

#define TTY_DEFAULT_WIDTH	80


/* Use ANSI-C defines for file-descriptors if available */
#ifdef STDIN_FILENO
#define TTY_READFD	STDIN_FILENO
#define TTY_WRITEFD	STDOUT_FILENO
#else
#define TTY_READFD	0
#define TTY_WRITEFD	1
#endif

/* POSIX has O_NONBLOCK, older systems use FNDELAY (BSD) or O_NDELAY (SYSV) */
#ifndef O_NONBLOCK

#ifdef FNDELAY
#define O_NONBLOCK	FNDELAY
#else
#ifdef O_NDELAY
#define O_NONBLOCK	O_NDELAY
#endif
#endif

#endif /* !O_NONBLOCK */

/* SHRT_MAX can be found in limits.h. Use acceptable default if not */
#ifndef SHRT_MAX
#define SHRT_MAX	0x7fff
#endif

/*
 * Next table contains the termcap options we need
 */
static const char *tc_cr, *tc_bl, *tc_vb, *tc_ks, *tc_ke, *tc_bc;
static const char *tc_ce, *tc_ch, *tc_le, *tc_LE, *tc_RI, *tc_nl;

static struct t_tty_capas
{
    const char *name;
    const char **storage;
}
          tty_capas[] =
{
/* INDENT OFF */
    { "cr", &tc_cr },               /* carriage return */
    { "bl", &tc_bl },               /* (audio) bell */
    { "vb", &tc_vb },               /* visual bell */
    { "ks", &tc_ks },               /* keypad start */
    { "ke", &tc_ke },               /* keypad end */
    { "bc", &tc_bc },               /* backspace */
    { "ce", &tc_ce },               /* clear to end of line */
    { "ch", &tc_ch },               /* move cursor horizontally to col N */
    { "le", &tc_le },               /* move cursor left */
    { "LE", &tc_LE },               /* move cursor left N positions */
    { "RI", &tc_RI },               /* move cursor right N positions */
    { "nl", &tc_nl },               /* move cursor to (start of) next line */

#if 0
    { "nd", NULL },              /* move cursor right (non-destructive space) */
    { "im", NULL },              /* enter insert mode */
    { "ei", NULL },              /* end insert mode */
    { "ic", NULL },              /* insert char */
    { "IC", NULL },              /* insert N chars */
    { "dm", NULL },              /* enter delete mode */
    { "ed", NULL },              /* end delete mode */
    { "dc", NULL },              /* delete char */
    { "DC", NULL },              /* delete N chars */
#endif

    { NULL, NULL }
/* INDENT ON */
};

/*
 * The following table will contain the terminal's special keys,
 * the char-sequence they generate and the equivalent KEY_xxx.
 * The KEY_XXX values are here so we know how to call sym_define_keys().
 */
static struct t_keycodes
{
    const char *name;             /* key code name */
    const char *value;            /* key code value */
    int       sym;                /* key will be mapped to this sym */
}
          keycodes[] =
{
/* INDENT OFF */
    { "kD", NULL, KEY_DELETE },   /* delete key */
    { "kr", NULL, KEY_RIGHT },    /* arrow right key */
    { "kl", NULL, KEY_LEFT },     /* arrow left key */
    { "ku", NULL, KEY_PREV },     /* arrow up key */
    { "kd", NULL, KEY_NEXT },     /* arrow down key */
    { "kh", NULL, KEY_HOME },     /* home key */
    { "@7", NULL, KEY_END },      /* end key */
    { "kb", NULL, KEY_BACKSPACE },/* backspace key */
    { "kI", NULL, KEY_INSERT },   /* insert key */
    { NULL, NULL, 0 }
/* INDENT ON */
};

/*
 * The last table, which will contain the terminal's erase/kill/... chars.
 * 
 * The table provides string space for sym_define_keys(), also making a
 * string from the single char we get from c_cc[].
 *
 * The KEY_XXX values are here so we know how to call sym_define_keys().
 */
static struct t_ttychars
{
    int       keyid;              /* key id as known by termios */
    char      keychar[2];         /* actual char (see stty), 0 if disabled */
    int       keysym;             /* key id as known by vrl */
}
          ttychars[] =
{
/* INDENT OFF */
    { VERASE, "\0", KEY_VERASE },
    { VKILL, "\0", KEY_VKILL },
    { VEOF, "\0", KEY_VEOF },
#ifdef VWERASE
    { VWERASE, "\0", KEY_VWERASE },
#endif
#ifdef VLNEXT
    { VLNEXT, "\0", KEY_VLNEXT },
#endif
#ifdef VREPRINT
    { VREPRINT, "\0", KEY_VREPRINT },
#endif
    { VINTR, "\0", KEY_VINTR },
    { VQUIT, "\0", KEY_VQUIT },
    { VSUSP, "\0", KEY_VSUSP },
    { 0, "\0", 0 }                /* the keysym=0 flags the end */
/* INDENT ON */
};

static char bell_str[] = "\007";  /* bell */

static struct termios tty_settings_appli;
static struct termios new_tty;
static int timeout_mode;

static int term_width;
static int has_am;

/*--------------------------------------------------------------------
 * tty_setup -- put tty in raw mode, re-init special keys if needed.
 *
 * This is derived from an example in Stevens' "Advanced Programming ..."
 * book.
 */
void
tty_setup()
{
    /* update tty capa's/keys if needed */
    tty_get_capas((const char *) NULL);

    (void) tty_update_term_width(0);     /* 0 == normal call, no sigwinch */

    /*
     * Must save and restore appli settings every time, terminal settings
     * may have changed !  (when user ran 'stty' in shell)
     */
    if (0 != tcgetattr(TTY_READFD, &tty_settings_appli))
    {
        out_printf("tcgetattr(appli_mode) returned %s", strerror(errno));
        return;
    }

    (void) memcpy((void *) &new_tty, (void *) &tty_settings_appli,
                  sizeof(struct termios));

    /*
     * Raw input, no echo and no int/quit/susp key/signal handling.
     */
    new_tty.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);

    /*
     * No SIGINT on BREAK, input parity check off, don't strip 8th bit on
     * input, output flow control off
     *
     * XXX: The POSIX spec suggests in the description of CR that ICRNL,
     * IGNCR are not active when ICANON is off. However, the FreeBSD code
     * seems to convert CR even in raw mode, so disable it explicitly
     * after all.
     */
    new_tty.c_iflag &= ~(BRKINT | INLCR | IGNCR | ICRNL | IXON);

    /*
     * if we're using 8 bits, disable parity check and don't strip 8th bit
     */
    if ((new_tty.c_cflag & CSIZE) == CS8)
        new_tty.c_iflag &= ~(INPCK | ISTRIP);

    /* No output processing */
    new_tty.c_oflag &= ~(OPOST);

    /* Get one char at a time, no timeout */
    timeout_mode = TTY_WAIT;
    new_tty.c_cc[VTIME] = 0;
    new_tty.c_cc[VMIN] = 1;

    /* termcap functions want to know the speed for padding calculations. */
    {
        /*
         * ospeed must be (signed) short but cfgetospeed() can return
	 * bigger values (e.g. for speeds >19200 baud) since speed_t
	 * can be typedef-ed as int . So if the ospeed fits in a (signed)
	 * short, use it. If not, use max speed that fits in a short.
	 */
	speed_t cur_ospeed = cfgetospeed(&new_tty);

	if (cur_ospeed <= SHRT_MAX)
	    ospeed = (short) cur_ospeed;
	else
	    ospeed = B19200;
    }

    /* No flush/wait, accept type-ahead */
    if (0 != tcsetattr(TTY_READFD, TCSANOW, &new_tty))
    {
        out_printf("tcgetattr(edit_mode) returned %s", strerror(errno));
        return;
    }

    /*
     * Playing with pthreads under FreeBSD-2.2.6 revealed that when a
     * multi-threaded application crashes, the shell immediately dies
     * afterwards because the keyboard read in tty_getch() below returns
     * EAGAIN.
     *
     * That can only happen in case the crashed process enabled non-blocking
     * I/O on the tty (as is done by the pthreads implementation), so always
     * force tty back to blocking I/O.
     */
    {
        int tty_flags;

        tty_flags = fcntl(TTY_READFD, F_GETFL, NULL);
        if (tty_flags < 0)
            out_printf("warning: fcntl(F_GETFL) returned %s",
                                                strerror(errno));

        /* Turn non-blocking off if needed */
        if ( tty_flags & O_NONBLOCK )
        {
            tty_flags &= ~(O_NONBLOCK);

            if ( (-1) == fcntl(TTY_READFD, F_SETFL, tty_flags))
                out_printf("warning: fcntl(F_SETFL) returned %s",
                                                strerror(errno));
        }
    }

    /*
     * Handle tty's special keys
     */
    {
        int       i, pc_vdisable;

#if defined (_PC_VDISABLE)        /* try POSIX compatible */
        pc_vdisable = (int) fpathconf(TTY_READFD, _PC_VDISABLE);

        if (pc_vdisable == -1)
#endif

#if defined (_POSIX_VDISABLE)     /* fallback, partly POSIX */
            pc_vdisable = _POSIX_VDISABLE;

        if (pc_vdisable == -1)
#endif
            pc_vdisable = 0xFF;   /* anarchy systems */

        /*
         * take tty chars into account. They will be ignored if
         * undefined/unset.
         */
        for (i = 0; ttychars[i].keysym != 0; i++)
        {
            if (new_tty.c_cc[ttychars[i].keyid] == pc_vdisable)
            {
                ttychars[i].keychar[0] = '\0';
                sym_define_key(ttychars[i].keysym, (const char *) NULL);
            }
            else
            {
                ttychars[i].keychar[0] = new_tty.c_cc[ttychars[i].keyid];
                sym_define_key(ttychars[i].keysym, ttychars[i].keychar);
            }

        }
    }

    (void) tputs(tc_ks, 1, out_char);
    out_flush();
}

/*--------------------------------------------------------------------
 * tty_restore -- reset terminal
 */
void
tty_restore()
{
    /* restore tty to original mode */
    (void) tputs(tc_ke, 1, out_char);
    out_flush();

    /* don't flush, allow type-ahead ! */
    if (0 != tcsetattr(TTY_READFD, TCSANOW, &tty_settings_appli))
        out_printf("tcsetattr(appli_mode) returned %s", strerror(errno));
}


/*--------------------------------------------------------------------
 * tty_getch -- read a char from the tty, wait for it if the flag says
 *		so. If not, a timeout is used.
 */
int
tty_getch(flg_wait)
int       flg_wait;
{
    char      c;
    int       n;

    /* get a char from the tty, change mode first if needed */
    if (flg_wait != timeout_mode)
    {
        if (flg_wait == TTY_WAIT)
        {
            new_tty.c_cc[VTIME] = 0;
            new_tty.c_cc[VMIN] = 1;
        }
        else
        {
            new_tty.c_cc[VTIME] = 10;  /* wait 1 second */
            new_tty.c_cc[VMIN] = 0;
        }

        if (0 != tcsetattr(TTY_READFD, TCSANOW, &new_tty))
            out_printf("tcgetattr(wait/timeout) returned %s", strerror(errno));
        timeout_mode = flg_wait;
    }

    /* loop until char is successfully read */
    for (;;)
    {
        n = read(TTY_READFD, &c, 1);

        if (n == 0)
        {
            return (flg_wait == TTY_WAIT ? TTY_EOF : TTY_TIMEOUT);
        }
        else if (n < 0)
        {
            if (errno == EINTR)
                return TTY_INTR;

            /*
             * Other error ? dunno how to recover, so treat it like an EOF.
             * THIS WILL GENERALLY KILL THE APPLICATION! (e.g. your shell!)
             */
            out_printf("read() returned %s", strerror(errno));
            return TTY_EOF;
        }

        /* Throw null-chars away at the source, too difficult to handle */
        if (c == '\0')
            continue;

        /*
         * tty's special chars (erase, kill) are interpreted at KEY_xxx
         * level, nothing to do here. (These chars are dynamic keys, see
         * above).
         */

        return c;
    }
    /* NOTREACHED */
}

/*--------------------------------------------------------------------
 * tty_beep -- signal user by beeping or flashing the screen
 */
void
tty_beep()
{
    /* audio/visual bell */
    if (opt_do_beep)
        (void) tputs(tc_bl, 1, (out_char));

}

/*--------------------------------------------------------------------
 * tty_get_capas -- update notion of tty keys/capas
 *
 * Failures are ignored (well, more or less...)
 */
void
tty_get_capas(term_type)
const char *term_type;
{

    /*
     * buffers for termcap functions and special key char-sequences
     */
    static char key_capa_buf[TERMCAP_BSIZE];

    /*
     * termcap buffer, is static since it may be used by get_term_width()
     */
    static char termcap_buf[TERMCAP_BSIZE];

    /*
     * Next keep track of current $TERM and $TERMCAP so that we only
     * update when things change.
     */
    static char *cur_termtype = NULL;
    static char *cur_termcap = NULL;

    const char *new_term;
    char     *new_termcap;
    char     *kcb_ptr = key_capa_buf;
    int       i;

    new_term = term_type != NULL ? term_type : getenv("TERM");
    new_termcap = getenv("TERMCAP");

    /* default to a dumb terminal */
    if (new_term == NULL)
        new_term = "dumb";

    /* return if no change in $TERM and $TERMCAP (latter may be NULL) */
    if (cur_termtype != NULL && strcmp(new_term, cur_termtype) == 0)
    {
        /* both null ? */
        if (cur_termcap == NULL && new_termcap == NULL)
            return;

        /* both not null and equal ? */
        if (cur_termcap != NULL && new_termcap != NULL &&
            strcmp(new_termcap, cur_termcap) == 0)
            return;
    }

    /* If the new tty is not valid, keep the old one and do nothing */
    if (tgetent(termcap_buf, new_term) <= 0)
    {
        out_printf("Warning: terminal type '%s' unknown", new_term);

        /*
         * If there is no terminal type defined yet, use some reasonable
         * defaults.
         */
        if (cur_termtype == NULL)
        {
            tc_nl = "\n";
            tc_cr = "\r";
            has_am = 1;
            tc_bl = bell_str;     /* ANSI has \a, but K&R doesn't */
        }
        return;
    }

    /* update the info we have */
    cur_termtype = utl_realloc_str(cur_termtype, new_term);

    if (new_termcap == NULL)
        (void) free(cur_termcap), cur_termcap = NULL;
    else
        cur_termcap = utl_realloc_str(cur_termcap, new_termcap);

    /*
     * Start with special termcap variables UP, PC, BC.
     */

    /* tgoto() pad character */
    {
        char     *padchar = tgetstr("pc", &kcb_ptr);

        PC = (char) (padchar == NULL ? 0 : *padchar);
    }

    /*
     * is backspace something else then ^H ?
     */
    {
        static char backsp[] = "\b";

        if (tgetflag("bs"))
            BC = backsp;
        else
        {
            BC = tgetstr("bc", &kcb_ptr);

            if (BC == NULL)
                BC = backsp;
        }
    }

    UP = tgetstr("up", &kcb_ptr);

    for (i = 0; tty_capas[i].name != NULL; i++)
        *(tty_capas[i].storage) = tgetstr(tty_capas[i].name, &kcb_ptr);

    tc_bc = BC;

    /*
     * termcap assumes that carriage return is ^M (015), unless stated
     * otherwise
     */
    if (tc_cr == (char *) NULL)
        tc_cr = "\015";

    /* Same thing for nl */
    if (tc_nl == (char *) NULL)
        tc_nl = "\n";

    /*
     * get automatic margin flags
     */
    has_am = (tgetflag("am") == 1);

    /*
     * if we don't have keypad mode on/off
     */
    if (tc_ks == NULL)
        tc_ks = "";
    if (tc_ke == NULL)
        tc_ke = "";

    /*
     * find out about bell: 1st audio, then visual, and lastly \007
     */

    if (tc_bl == (char *) NULL)
        tc_bl = tc_vb;

    if (tc_bl == (char *) NULL)
        tc_bl = bell_str;

    /*
     * find out about the terminal keys
     */
    for (i = 0; keycodes[i].name != NULL; i++)
    {
        keycodes[i].value = tgetstr(keycodes[i].name, &kcb_ptr);
        sym_define_key(keycodes[i].sym, keycodes[i].value);
    }

    /*
     * XXX: 'Hack' to work around broken termcap entries.
     *
     * Some systems (OpenBSD 2.2, fixed since)  have (vt100/...) termcap
     * entries without a definition of the DEL key. If there's no "kD"
     * entry, and none of the keys vrl uses generates \177 (backspace
     * sometimes does), assume the DEL key does exist and generates \177.
     *
     * Eric S. Raymond's termcap file lists 1012 ttys, of which 545 have kb,
     * 155 have kD, and 155 have both. The BSD termcap has 592 ttys, of
     * which 295 have kb, 105 kD, and 105 have both.
     *
     * This hack seems to be worth it, for the time being...
     */
    {
        const char *bkkey = "\177\0";
        int       delkey_index = -1;
        int       seq_used = 0;

        /*
         * scan list to find KEY_DELETE entry. At the same time, verify
         * that the \177 sequence isn't used yet.
         */
        for (i = 0; keycodes[i].name != NULL; i++)
        {
            if (keycodes[i].sym == KEY_DELETE)
                delkey_index = i;
            else if (keycodes[i].value != NULL &&
                     strcmp(keycodes[i].value, bkkey) == 0)
                seq_used = 1;
        }

        /*
         * If there's a KEY_DELETE (always), the sequence isn't used, and
         * the delete key is still undefined, force it.
         */
        if (delkey_index != -1 && !seq_used &&
            keycodes[delkey_index].value == NULL)
        {
            keycodes[delkey_index].value = kcb_ptr;
            *(kcb_ptr++) = '\177';
            *(kcb_ptr++) = '\0';
            sym_define_key(keycodes[delkey_index].sym,
                           keycodes[delkey_index].value);
        }
    }
}

/*--------------------------------------------------------------------
 * tty_write -- send a number of bytes to the tty. This handles EINTR
 *              on write() as well.
 */
void
tty_write(str, len)
const char *str;
size_t    len;
{
    size_t    to_write = len;

    while (to_write != 0)
    {
        ssize_t   written;

        written = write(TTY_WRITEFD, str, to_write);

        if (written >= 0)
        {
            to_write -= (size_t) written;
            str += written;
        }
        else
        {
            if (errno == EINTR)
                continue;

            /*
             * XXX: what to do ? can't write an error msg, and calling
             * exit would abort the shell. Go on for now, user types
             * blind...
	     *
	     * To avoid an infinite loop, do as if the write succeeded.
             */
	     break;
        }
    }
}

/*----------------------------------------------------------------------
 * clrtoeol - clear to end of line. If we can't clear, return false.
 */
int
tty_clrtoeol()
{
    if (tc_ce == NULL)
        return 0;

    (void) tputs(tc_ce, 1, out_char);
    return 1;
}

/*----------------------------------------------------------------------
 * tty_newline - move cursor to start of next line
 *
 * Termcap also has 'nw', which is moves the cursor to the start of
 * the next line. However, nw == cr + do, and the behavior of do is
 * undefined when the cursor is on the bottom line.
 *
 * Since vrl never knows on what line the cursor is located, 'nw'
 * cannot be used. Note that neither nvi, jv, nor less use 'nw'...
 */
void
tty_newline()
{
    /* Use 'cr' + 'nl' */
    (void) tputs(tc_cr, 1, out_char);
    (void) tputs(tc_nl, 1, out_char);
}


/*----------------------------------------------------------------------
 * tty_cr - move cursor to start of current line
 */
void
tty_cr()
{
    (void) tputs(tc_cr, 1, out_char);
}


/*----------------------------------------------------------------------
 * tty_setcurpos -- move screen cursor to given position on current line
 *
 * There are a lot of terminals that don't have the absolute 'ch'
 * (cursor horizontal) capability, but a relative move left/right
 * (LE/RI) relative to the current position is an acceptable fallback.
 *
 * If that doesn't work either, try multiple 'nd' (non-destructive space)
 * or 'le' (cursor left) steps relative to the current position.
 *
 * If there is no 'le', go to the beginning of the line and move
 * right from there on.
 *
 * Note that for most terminals, 'nd' is a multi-char ESC-sequence,
 * so it is cheaper to print the chars we are moving over.
 *
 * Also, RI/LE/ch normally consist of at least two chars, so when the
 * move is small, i.e. 1 step forward/backward, print the char
 * we're moving over, or use le directly, forget about RI/LE/ch.
 */
void
tty_setcurpos(newpos, curpos, curline)
size_t    newpos;
size_t    curpos;
const char *curline;
{
    int       i;
    ssize_t   delta = newpos - curpos;

    if (delta == 0)
        return;

    else if (delta == 1)
        (void) out_char(curline[curpos]);/* Shortcut */

    else if (delta > 0)
    {

        /*
         * Try 'ch' (abs move), 'RI' (rel move N), or send the chars to
         * move over.
         */
        if (tc_ch != NULL)
            (void) tputs(tgoto(tc_ch, 0, (int) newpos), 1, out_char);

        else if (tc_RI != NULL)
            (void) tputs(tgoto(tc_RI, 0, (int) delta), 1, out_char);

        else
            out_str(curline + curpos, (size_t) delta);

    }
    else
    {                             /* delta < 0 */
        delta = -delta;

        if (delta > 1 || tc_le == NULL)
        {

            /*
             * Try 'ch' (abs move) or 'LE' (rel move N)
             */
            if (tc_ch != NULL)
            {
                (void) tputs(tgoto(tc_ch, 0, (int) newpos), 1, out_char);
                return;
            }
            else if (tc_LE != NULL)
            {
                (void) tputs(tgoto(tc_LE, 0, (int) delta), 1, out_char);
                return;
            }
        }

        /*
         * If we get here, the only thing left is 'le'.
         *
         * In some cases, it may be cheaper to move rightward from the left
         * margin, instead of leftward from the current position.
         *
         * Heuristic: if the delta is bigger than 50% of the abspos, move
         * forward from left margin.
         */
        if (tc_le != NULL && curpos > delta + delta)
        {
            for (i = delta; i != 0; i--)
                (void) tputs(tc_le, 1, out_char);
        }
        else
        {

            /*
             * Can't move leftward or is considered expensive. Just go to
             * the beginning of the line print the char's we are moving
             * over
             */
            (void) tputs(tc_cr, 1, out_char);
            out_str(curline, newpos);
        }
    }
}

/*--------------------------------------------------------------------------
 * tty_width -- return the current tty width
 */
size_t
tty_width()
{
    /* exclude the rightmost position if tty has auto-margins */
    return (term_width - (has_am ? 1 : 0));
}

/*--------------------------------------------------------------------------
 * tty_update_term_width -- handle window size change, return 1 if really
 *			    changed, 0 if not.
 *
 * We have three ways to determine the output screen width:
 * -1- The reliable one is to use the ioctl(TIOCGWINSZ)
 * -2- If not supported, use what termcap gives us
 * -3- $COLUMNS overrules everything (as per POSIX 1003.2)
 *
 * If we get here do to a SIGWINCH, only the ioctl is accepted, since the
 * others won't change and thus won't inform vrl about the new size.
 * If the ioctl doesn't work, vrl keeps using the old size, i.e. from
 * termcap/env so nothing is lost anyway.
 */
int
tty_update_term_width(sigwinch)
int       sigwinch;
{
    char     *s;
    int       ncols = 0;

#ifdef HAVE_TIOCGWINSZ
    {
        struct winsize size;

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, (char *) &size) >= 0)
            ncols = size.ws_col;
	else if (errno != EINVAL)
	{
            out_printf("ioctl(TIOCGWINSZ) error [%s]", strerror(errno));

	    /* XXX: When running a Solaris 1 executable under Solaris 2.5,
	     *      the ioctl above may fail with EINVAL if the tty's size
	     *      is invalid/unknown.
	     *      A work-around is to use /usr/xpg4/bin/stty (something
	     *      like that, _not_ /bin/stty) to set the correct #cols
	     *      The best way to fix that is to compile a native
	     *      Solaris 2 binary though....
	     * XXX: the ioctl() will fail under UnixWare 2.1.2 when the
	     *      driver doesn't support the ioctl(), like on a console.
	     */
	}
    }
#endif

    if (!sigwinch)
    {

        /*
         * termcap & $LINES/$COLUMNS are only checked first time: if we
         * got a SIGWINCH, the ioctl() above is the only one who can tell
         * us about the new size so $LINES/COLUMNS should not be allowed
         * to overrule in that case.
         */

        if (ncols <= 0)
            ncols = tgetnum("co");

        /*
         * allow environment to overrule
         */
        if ((s = getenv("COLUMNS")) != (char *) NULL)
            ncols = atoi(s);

        /*
         * If none of the above resulted in something useful, ask user to
         * set the size in the environment.
         */
        if (ncols <= 0)
        {
            ncols = TTY_DEFAULT_WIDTH;
            out_printf("#columns on tty unknown, defaulting to %i", ncols);
        }
    }

    /*
     * applications running in a solaris 1.x/2.x shelltool/cmdtool will
     * also get a SIGWINCH when the window is (partially) uncovered. Only
     * update something when the display size _really_ changed.
     */
    if (ncols > 0 && ncols != term_width)
    {
        term_width = ncols;
	return 1;
    }

    return 0;
}
