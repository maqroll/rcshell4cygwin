/*
 * $Id: out.c,v 1.5 1996/08/07 20:58:36 vons Exp $
 *
 * out.c -- manages an output buffer that is written to the screen
 *          on request.
 */

#include "config.h"
#include "vrl.h"
#include <stdio.h>		/* vsprintf(), vsnprintf() */

#ifdef HAVE_WORKING_STDARG
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static struct str out_buf;
static size_t out_len;

void
out_init()
{
    str_init(&out_buf, 100);      /* 100 is as good as any other */
    out_len = 0;
}

/*----------------------------------------------------------------------
 * out_char -- add char to output buffer
 *
 * NOTE: this function is also used by tgoto() and thus must return  an
 *       int instead of being void.
 */
int
out_char(c)
int       c;
{
    str_grow(&out_buf, out_len + 1);
    out_buf.str[out_len] = (char) c;
    out_len++;

    return 0;
}

void
out_str(s, l)
const char *s;
size_t    l;
{
    str_grow(&out_buf, out_len + l);

    (void) memcpy(&(out_buf.str[out_len]), s, l);

    out_len += l;
}

void
out_flush()
{
    if (out_len != 0)
        tty_write(out_buf.str, out_len);
    out_len = 0;
}

void
out_nl()
{
    tty_newline();
    out_flush();
}

/*----------------------------------------------------------------------
 * out_printf -- printf like function that writes to tty. Msg will be
 *               displayed at start of current line, followed by a 
 *		 newline. The cmdline on screen is invalidated to
 *               force a refresh.
 */
#ifdef HAVE_WORKING_STDARG
void
out_printf(const char *fmtstr,...)
{
    va_list   ap;
    va_start(ap, fmtstr);
#else
void
out_printf(va_alist)
va_dcl
{
    va_list   ap;
    const char *fmtstr;

    va_start(ap);
    fmtstr = va_arg(ap, const char *);
#endif
    {
        char      buf[256];

#ifdef HAVE_VSNPRINTF
        /* vsnprintf() is a BSD invention */
        (void) vsnprintf(buf,sizeof(buf), fmtstr, ap);
#else
        (void) vsprintf(buf, fmtstr, ap);
#endif

	tty_cr();
        out_str(buf, strlen(buf));

	/* clear the remainder of the line if possible */
	(void) tty_clrtoeol();

        out_nl();
	scr_invalidate(); /* force cmdline rewrite */
    }
    va_end(ap);
}
