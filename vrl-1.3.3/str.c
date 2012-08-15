/*
 * $Id: str.c,v 1.5 1996/05/22 20:07:49 vons Exp $
 *
 * str.c -- manage string 'objects'
 */

#include "config.h"
#include "vrl.h"

/*----------------------------------------------------------------------
 * str_init -- init given string
 */
void
str_init(s, initial_size)
struct str *s;
size_t    initial_size;
{
    s->len = 0;
    if (initial_size != 0)
        str_grow(s, initial_size);
}

/*----------------------------------------------------------------------
 * str_assign -- reassign new value to string
 */
void
str_assign(s, cs)
struct str *s;
const char *cs;
{
    size_t    l;
    if ((l = strlen(cs) + 1) > s->len)
        str_grow(s, l);

    (void) strcpy(s->str, cs);
}

/*----------------------------------------------------------------------
 * str_add -- add data to end of string
 */
void
str_add(s, cs)
struct str *s;
const char *cs;
{
    size_t    l;
    if ((l = strlen(s->str) + strlen(cs) + 1) > s->len)
        str_grow(s, l);

    (void) strcat(s->str, cs);
}

/*----------------------------------------------------------------------
 * str_grow -- make sure string length is at least l
 */
void
str_grow(s, l)
struct str *s;
size_t    l;
{
    char     *p;

    if (s->len >= l)
        return;

    /* allocate with a certain margin, reduces the number of reallocs */
    l += 32;

    if (s->len == 0)
        p = malloc(l);
    else
        p = realloc(s->str, l);

    if (p == NULL)
    {
        out_printf("not enough memory");
        exit(2);
    }

    s->len = l;
    s->str = p;
}
