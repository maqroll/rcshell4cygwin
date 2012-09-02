/*
 * $Id: sig.c,v 1.2 1996/05/17 21:15:03 vons Exp $
 */

#include "config.h"
#include "vrl.h"

#include <signal.h>

#ifndef SIGWINCH

void sigwinch_setup() { }
void sigwinch_restore() { }
int sigwinch_seen() { return 0; }

#else

PROTOTYPE(static RETSIGTYPE sigwinch_handler, (int));

static VOLATILE SIG_ATOMIC_T caught_sigwinch = 0;

int
sigwinch_seen()
{
    int i;

    i = caught_sigwinch;
    caught_sigwinch = 0;
    return i;
}

#ifdef HAVE_SIGACTION

static struct sigaction old_act;

void
sigwinch_setup()
{
    struct sigaction act;

    (void) sigaction(SIGWINCH, (struct sigaction *) NULL, &old_act);

    act.sa_handler = sigwinch_handler;
    (void) sigemptyset(&act.sa_mask);

    act.sa_flags = 0;

#ifdef SA_RESTART
    /* sa_flags=0, no SA_RESTART */
#else
#ifdef SA_INTERRUPT
    act.sa_flags = SA_INTERRUPT;
#else
    /* leave sa_flags=0, and hope for the best ... */
#endif
#endif

    (void) sigaction(SIGWINCH, &act, &old_act);

    caught_sigwinch = 0;
}

void
sigwinch_restore()
{
    (void) sigaction(SIGWINCH, &old_act, (struct sigaction *) NULL);
}

#else /* -------------- use signal() -------------- */

PROTOTYPE(RETSIGTYPE(*sigwinch_old_handler), (int));

void
sigwinch_setup()
{
    caught_sigwinch = 0;
    sigwinch_old_handler = signal(SIGWINCH, sigwinch_handler);
}

void
sigwinch_restore()
{
    (void) signal(SIGWINCH, sigwinch_old_handler);
}
#endif                            /* !HAVE_SIGACTION */

/*ARGSUSED*/
static    RETSIGTYPE
sigwinch_handler(signo)
int       signo;
{
#ifndef HAVE_SIGACTION
    /* re-install signal handler just in case */
    (void) signal(SIGWINCH, sigwinch_handler);
#endif
    caught_sigwinch = ~0;
}

#endif	/* SIGWINCH */
