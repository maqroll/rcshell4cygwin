/*
 * $Id: vrl.h,v 1.9 1998/02/15 13:43:18 vons Exp $
 *
 * vrl.h -- global vrl include, contains all defines and prototypes
 */

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>               /* getpid(), getopt() */
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>            /* ssize_t */
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>             /* stat() */
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>              /* some old compilers still use this
                                   * (strcpy, strcat) */
#endif

#ifdef NEED_MEMORY_H
#include <memory.h>               /* or this (memcpy/memset) */
#endif

#ifdef NEED_MALLOC_H
#include <malloc.h>               /* or this (malloc()) */
#endif


#ifdef HAVE_PROTOTYPES
#define PROTOTYPE(func, args)	func args
#else
#define PROTOTYPE(func, args)	func ()
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
/* #define NAMLEN(dirent) strlen ((dirent)->d_name) */
#else

#ifdef HAVE_DIRECT_H
#include <direct.h>
/* #define NAMLEN(dirent) strlen ((dirent)->d_name) */
#else

#define dirent direct
/* #define NAMLEN(dirent) (dirent)->d_nam */

#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif

#ifdef HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif

#ifdef HAVE_NDIR_H
#include <ndir.h>
#endif

#endif  /* HAVE_DIRECT_H */

#endif	/* HAVE_DIRENT_H */

/* don't use assert.h. Only need the test+msg, not the abort() */
#ifdef DEBUG
#define ASSERT(cond)							   \
    ( (cond) ?								   \
        (void) 0							   \
      : (out_printf("Assertion \"%s\" failed: file \"%s\", line \"%i\"\n", \
				#cond, __FILE__, __LINE__), 		   \
	(void) 0))
#define LOG(msg)	out_printf msg
#else
#define ASSERT(cond)	((void)0)
#define LOG(msg)	((void)0)
#endif

#define CNTRL(c)	((char)((int)(c) - (int)'A' + 1))
#define TAB		'\t'
#define ESC		(char)27
#define BACKSPACE	'\b'

#define TOHEX(d)	(char)((d) <= 9 ? ('0' + (d)) : ('A' + ((d) - 10)))

typedef unsigned char Uchar;

 /* -- opt.c -- */

extern int opt_histmode;
extern int opt_histmax;
extern int opt_autolist;
extern int opt_do_beep;
extern int opt_insmode;
extern int opt_ansicur;

PROTOTYPE(extern void opt_read, (void));


 /* -- tty.c -- */


#define TTY_TIMEOUT	(-1)          /* special results from getch */
#define TTY_EOF		(-2)
#define TTY_INTR	(-3)

#define TTY_WAIT	(1)             /* flags for getch */
#define TTY_TIMER	(2)

/* switch tty to raw mode, initialise special keys, ... */
PROTOTYPE(void tty_setup, (void));

/* restore tty to initial mode */
PROTOTYPE(void tty_restore, (void));

/* update tty.c's idea of the terminal type */
PROTOTYPE(void tty_get_capas, (const char *term_type));

/* get next input char */
PROTOTYPE(int tty_getch, (int waitflag));

/* make tty beep (sound, flash, ...) */
PROTOTYPE(void tty_beep, (void));

/* get width of tty */
PROTOTYPE(size_t tty_width, (void));

/* Tell tty module to update the tty width, returns value != 0 if changed.
 * sigwinch should be set if the call is due to a SIGWINCH.
 */
PROTOTYPE(int tty_update_term_width, (int sigwinch));

/* move cursor to given pos on line (leftmost position is 0) */
PROTOTYPE(void tty_setcurpos, (size_t, size_t, const char *));

/* write data to tty */
PROTOTYPE(void tty_write, (const char *, size_t));

/* clear to eol return 1 if ok, 0 if couldn't clear */
PROTOTYPE(int tty_clrtoeol, (void));

/* move cursor to start of next line */
PROTOTYPE(void tty_newline, (void));

/* move cursor to start of current line */
PROTOTYPE(void tty_cr, (void));


 /* -- sym.c -- */

/* Codes for special keys.
 *
 * normal chars map to 0..255, keyboard function keys start at 256.
 */

#define KEY_LEFT		256 /* left & right arrow */
#define KEY_RIGHT		257

#define KEY_PREV		258 /* up&down arrow */
#define KEY_NEXT		259

#define KEY_DELETE		260 /* del & backsp key */
#define KEY_BACKSPACE		261

#define KEY_HOME		262 /* home & end key */
#define KEY_END			263

#define KEY_INSERT		264 /* insert key */

#define KEY_VERASE		265 /* tty's erase key */
#define KEY_VKILL		266 /* tty's line-kill key */
#define KEY_VEOF		267 /* tty's eof key */
#define KEY_VWERASE		268 /* tty's word-erase key */
#define KEY_VLNEXT		269 /* tty's literal-next key */
#define KEY_VREPRINT		270 /* tty's reprint key */
#define KEY_VSUSP		271 /* tty's suspend key */

#define KEY_VINTR		3 /* tty's interrupt key */
#define KEY_VQUIT		273 /* tty's quit key */

/* Actual symbols vrl can handle, i.e. syms that sym_get() may return */
#define SYM_LEFT		1024    /* cursor left */
#define SYM_RIGHT		1025    /* cursor right */
#define SYM_DELETE		1026    /* del char under cursor */
#define SYM_BACKSPACE		1027    /* del char left of cursor */
#define SYM_PREVHIST		1028    /* prev hist cmd */
#define SYM_NEXTHIST		1029    /* next hist cmd */
#define SYM_LIST_COMPLETE	1030    /* list possible completions */
#define SYM_DO_COMPLETE		1031    /* complete current name */
#define SYM_REDISPLAY		1032    /* redraw whole line */
#define SYM_ENTER		1033    /* enter key */
#define SYM_EOF			1034    /* end of input  */
#define SYM_TOGGLE_INSMODE	1035    /* toggle insert/overwrite */
#define SYM_LINE_START		1036    /* goto start of line */
#define SYM_LINE_END		1037    /* goto end of line */
#define SYM_LITERAL_NEXT	1038    /* take next char literal */
#define SYM_KILL_LINE		1039    /* erase whole line (tty's kill char) */
#define SYM_HIST_SEARCH		1040    /* history search */
#define SYM_DELETE_WORD		1041    /* delete word */
#define SYM_DEL_TO_EOL 		1042    /* delete to end of line */
#define SYM_SIGTSTP 		1043    /* send sigtstp to process */
#define SYM_SIGINT 		1044    /* send sigint to process */
#define SYM_SIGQUIT 		1045    /* send sigquit to process */
#define SYM_INTR 		1046    /* interrupt seen (sigwinch) */
#define SYM_TRANSPOSE 		1047    /* transpose chars */
#define SYM_REREAD_OPTIONS 	1048    /* re-read options file */
#define SYM_VERSION 	        1049    /* display version */
#define SYM_DO_COMPLETE_ALL	1050    /* insert all possible completions */

/* process next input symbol, return 0:ok 1:eof */
PROTOTYPE(int sym_get, (void));

/* define key sequence for given symbol */
PROTOTYPE(void sym_define_key, (int keycode, const char *keyseq));


 /* -- iline.c -- */

#define ILINE_CW_SKIP		0         /* see iline_curword_start() */
#define ILINE_CW_DONT_SKIP	1

/* init iline module */
PROTOTYPE(void iline_init, (void));

/* get addr of buffer that contains the input line under construction */
PROTOTYPE(const char *iline_linebuf, (void));

/* replace the whole line with new data */
PROTOTYPE(void iline_replace, (const char *newdata));

/* delete all chars from curpos (included) to the end of the line */
PROTOTYPE(void iline_del2eol, (void));

/* replace part of input line with new data. (from is rel to curpos!) */
PROTOTYPE(void iline_edit, (ssize_t from, size_t len, const char *newdata));

/* get current position (relative to iline) */
PROTOTYPE(size_t iline_getpos, (void));

/* set current position (relative to curpos on iline) */
PROTOTYPE(void iline_setpos, (ssize_t));

/* peek at char at given pos (relative to curpos) */
PROTOTYPE(int iline_peekch, (ssize_t));

/* set current position to start of word */
PROTOTYPE(size_t iline_curword_start, (int skip_space,
                                       const char *extra_word_chars));

/* set current position (relative to start of iline) */
PROTOTYPE(void iline_start, (size_t));

/* set current position (relative to end of iline) */
PROTOTYPE(void iline_end, (size_t));

/* copy current line to save buffer */
PROTOTYPE(void iline_save, (void));

/* copy save buffer back into line buffer */
PROTOTYPE(void iline_undo, (void));


 /* -- complete.c -- */

/*
 * All options expand the current fname/expression as far as possible,
 * the behavior for ambiguous completions can be customised:
 *
 * COMP_NOLIST            - don't list, just beep
 * COMP_LIST_NOCHANGE     - only list if fname/expr didn't change
 * COMP_LIST_IF_AMBIGUOUS - always list all possibilities
 *
 * If the possible completions should only be listed, use
 *
 * COMP_LIST_ONLY	- list all possibilities, don't update cmdline
 *
 * If all possible completions must be inserted, use
 *
 * COMP_INSERT_ALL	- replace current fname/expression with all possible
 *			  extensions
 */

#define COMP_NOLIST	        0
#define COMP_LIST_NOCHANGE      1
#define COMP_LIST_IF_AMBIGUOUS  2
#define COMP_LIST_ONLY 	        3
#define COMP_INSERT_ALL         4

#define COMP_OK	      		0       /* return values */
#define COMP_DID_LIST 		1

/* complete current word, list alternatives if ambiguous */
PROTOTYPE(int comp_complete, (int flag));


 /* -- history.c -- */

#define OPT_HIST_ALL       0
#define OPT_HIST_NOREPEAT  1
#define OPT_HIST_NODUPS	   2

#define HIST_SFIRST	   3
#define HIST_SNEXT	   4

/* move ptr to most recent cmd in hist */
PROTOTYPE(const char *hist_newest, (void));

/* move ptr to next older cmd in hist */
PROTOTYPE(const char *hist_next_older, (void));

/* move ptr to the next (more recent) cmd in hist */
PROTOTYPE(const char *hist_next_newer, (void));

/* add copy of given cmd to hist */
PROTOTYPE(void hist_add, (const char *));

/* search backwards for occurrence of string in history */
PROTOTYPE(const char *hist_search, (const char *, int mode));


 /* -- out.c -- */

PROTOTYPE(void out_init, (void));

/* output a single char. */
PROTOTYPE(int out_char, (int c));

/* output a string */
PROTOTYPE(void out_str, (const char *s, size_t l));

/* flush output buffer */
PROTOTYPE(void out_flush, (void));

/* add newline to output buffer */
PROTOTYPE(void out_nl, (void));

/* write error msg to tty */
#ifdef HAVE_WORKING_STDARG
void      out_printf(const char *fmt,...);
#else
void      out_printf();
#endif


 /* -- screen.c -- */

/* one-time setup of screen module */
PROTOTYPE(void scr_init, (void));

/* set up screen module for new cycle */
PROTOTYPE(void scr_setup, (const char *prompt));

/* make screen look like iline */
PROTOTYPE(void scr_update, (const char *li, size_t curpos));

/* vrl's idea of the screenline is no longer valid */
PROTOTYPE(void scr_invalidate, (void));



 /* -- str.c -- */

struct str
{
    char     *str;
    size_t    len;
};                                /* see str.c */

/* init a str */
PROTOTYPE(void str_init, (struct str *, size_t));

/* make sure given str has a minimum size, realloc if needed */
PROTOTYPE(void str_grow, (struct str *, size_t));

/* assign char sequence to str */
PROTOTYPE(void str_assign, (struct str *, const char *));

/* add char sequence at end of str */
PROTOTYPE(void str_add, (struct str *, const char *));



 /* -- util.c -- */

/* a realloc that accepts a NULL ptr */
PROTOTYPE(void *utl_reallocate, (void *, size_t));

/* max path len on system */
PROTOTYPE(size_t utl_maxpathlen, (void));

/* reallocate & refill a string */
PROTOTYPE(char *utl_realloc_str, (char *old, const char *str));

#ifndef HAVE_STRERROR
PROTOTYPE(char *strerror, (int errnum));
#endif

#ifndef HAVE_STRSTR
PROTOTYPE(char *strstr, (const char *, const char *));
#endif


 /* -- sig.c -- */

/* setup sigwinch handler */
PROTOTYPE(void sigwinch_setup, (void));

/* restore old sigwinch handler */
PROTOTYPE(void sigwinch_restore, (void));

/* returns true if sigwinch has been seen */
PROTOTYPE(int sigwinch_seen, (void));
