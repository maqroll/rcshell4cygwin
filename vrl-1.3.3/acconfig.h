/*
 * acconfig.h: non-standard configuration items, see configure.in.
 *
 * $Id: acconfig.h,v 1.4 2000/05/10 21:09:58 vons Exp $
 */

/* Define ssize_t as int if you don't have the ssize_t type */
#undef ssize_t

/* Define HAVE_WORKING_STDARG if your have a _working_ stdarg
 * implementation, not just the stdarg.h include file.
 */
#undef HAVE_WORKING_STDARG

/* Some environments (OSF/1, ...) support prototypes but don't define
 * __STDC__ by default.
 */
#undef HAVE_PROTOTYPES

/* Apparently, some systems don't have the PC, BC, and UP variables
 * in their termcap/termlib libraries.
 */
#undef HAVE_TERMCAP_VARS

/* And if the variables exist, they are not necessarily declared somewhere 
 * in the system includes (not even in termcap.h, QNX4).
 */
#undef HAVE_TERMCAP_DEFNS

/*
 * Defined if your system has the ioctl(TIOCGWINSZ) call
 */
#undef HAVE_TIOCGWINSZ

/* Define this if memcpy() is found in memory.h instead of string.h
 */
#undef NEED_MEMORY_H

/* Define this if malloc() is found in malloc.h instead of stdlib.h
 */
#undef NEED_MALLOC_H

@BOTTOM@

/*-------------------------------------------------------------------------
 * Additional configuration defines, not handled by configure:
 */

/* If your system uses something other than the '/', you should change
 * this.
 */
#define DIRSEP	'/'

/* Define VOLATILE as void if your compiler doesn't know about the volatile
 * keyword.
 */
#define VOLATILE volatile

/*  Define SIG_ATOMIC_T as int if your system doesn't know about sig_atomic_t
 */
#define SIG_ATOMIC_T	sig_atomic_t

