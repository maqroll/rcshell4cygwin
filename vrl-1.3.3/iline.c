/*
 * $Id: iline.c,v 1.5 1996/09/01 19:13:04 vons Exp $
 *
 * iline.c -- manage the edit operations on the input line
 */

#include "config.h"
#include "vrl.h"

#include <ctype.h>

static char *il = NULL;
static char *il_save = NULL;
static size_t il_len = 0;
static size_t il_alloced = 0;
static size_t curpos = 0;

PROTOTYPE(static void iline_checklen, (size_t len));

void
iline_init()
{
    /* init with reasonable size to avoid lot of small realloc's */
    iline_checklen(97);
}

static void
iline_checklen(l)
size_t l;
{
    if (il_alloced < l)
    {
	il = (char *) utl_reallocate(il, l);
	il_save = (char *) utl_reallocate(il_save, l);
	il_alloced = l;
    }
    ASSERT(il != NULL);
    ASSERT(il_save != NULL);
}

const char *
iline_linebuf()
{
    return il;
}

/*
 * iline_del2eol - delete all chars from curpos to the end of the line,
 *		   curpos included.
 */
void
iline_del2eol()
{
    il_len = curpos;
    il[il_len] = '\0';

}

/*
 * iline_peekch - return char at curpos
 */
int
iline_peekch(d)
ssize_t d;
{
    ASSERT((ssize_t)curpos + d >= 0); ASSERT(curpos + d <= il_len);
    return il[curpos + d];

}

void
iline_replace(s)
const char *s;
{
    size_t l;

    /* NULL means empty it */
    if (s == NULL)
	s = "";

    l = strlen(s);
    iline_checklen(l + 1);
    (void) strcpy(il, s);
    il_len = l;

    /* put cursor at end of string */
    curpos = l;
}

void
iline_edit(from, olen, newdata)
ssize_t from;
size_t olen;
const char *newdata;
{
    /* replace iline[from, from+len) with newdata */
    /* last inserted/changed char becomes curpos */
    /* when deleting, newdata may be null */

    size_t nlen;
    ssize_t abs_from;

    if (newdata == NULL)
	nlen = 0;
    else
        nlen = strlen (newdata);

    abs_from = curpos + from;

    /* The old range specified by the caller may include the pos
     * after the last char, in case the cursor is located at that pos.
     * Just adapt olen if needed.
     */
    if (abs_from + olen > il_len)
	olen = il_len - abs_from;

    ASSERT(abs_from >= 0 && abs_from <= il_len);

    /* new string longer than old section ? */
    if (olen < nlen)
    {
	/* yes, verify buffer size */
	iline_checklen(il_len + (nlen-olen) +1);
    }
    /* move tail to new pos. It may be simply the trailing nul */
    (void) memmove(&il[abs_from+nlen], &il[abs_from+olen],
		il_len - (abs_from + olen)  + 1);

    /* put new string in place */
    if (nlen != 0)
        (void) memcpy(&il[abs_from], newdata, nlen);

    il_len = il_len + (nlen - olen);

    /* cursor at rightmost pos */
    curpos = abs_from + nlen;

    /* make sure it is nul-terminated */
    il[il_len] = '\0';
}

/*
 * iline_curword_start -- move to start of word, skipping any whitespace,
 *                        and return the #chars in the word.
 *
 * If the cursor is in a word, the word ends at the curpos, inclusive.
 *
 * The extra_word_chars string can be used to add extra chars to a word, so
 * that a filename is one word, and not a sequence of /-separated words.
 *
 * If skip_space is ILINE_CW_SKIP, any whitespace between the end of the
 * last  word and the cursor is skipped. If set to ILINE_CW_DONT_SKIP,
 * the word starts after the first whitespace left of the cursor.
 */
size_t
iline_curword_start(skip_space, extra_word_chars)
int skip_space;
const char *extra_word_chars;
{
     int c;
     int alnum;
     size_t word_end;
     size_t cpos;

    if (extra_word_chars == NULL)
	extra_word_chars = "";

    /* Ignore curpos if it is a space after a word (or after the last
     * word on the line!), so that
     *
     * "word  otherwrd"   "word otherwrd" and "word  otherwrd" "longword"
     *     ^                   ^                    ^               ^
     *       1                   2                   3              4
     *
     * work as expected, i.e. in case 1 and 2, the current word is "word",
     * it is empty in case 3, and "longword" in the last.
     */
    cpos = curpos;
    if(cpos != 0 && ( isspace((Uchar)il[cpos]) || il[cpos] == '\0'))
	cpos--;

    /* skip any whitespace left of cpos */
    if (skip_space == ILINE_CW_SKIP)
    {
        while(isspace((Uchar)il[cpos]) && cpos != 0)
	    cpos--;
    }

    /* no word before cpos or line is empty ? If so, leave curpos intact */
    if (isspace((Uchar)il[cpos]) || il[cpos] == '\0')
	return 0;

    /* There is something... */
    curpos = cpos;

    /*
     * The first char encountered defines the word class we're looking for.
     * We distinguish three classes:
     *   - alphanumeric, underscore, and extra_word_chars
     *   - whitespace (space, tab, newline, carriage-return)
     *   - everything else (!@#$%^&*()_+-=:;,.<>/\|?"'~`[]{})
     */
    c = (Uchar)il[curpos];


#define ISALNUM(c)	\
	((isalnum(c) || c == '_' || strchr(extra_word_chars, c) != NULL))

    alnum =ISALNUM(c);

    /* word_end will point to last char of word */

    /* move forward as long as next char is in same class */
    word_end = curpos;
    while ( !isspace((c = (Uchar)il[word_end+1]))
			&& !(ISALNUM(c) ^ alnum) && c != '\0')
    {
	word_end++;
    }


    /* move as long as prev char is in same class, and not whitespace */
    while( !isspace((c = (Uchar)il[curpos-1]))
	   		&& !(ISALNUM(c) ^ alnum) && curpos != 0)
    {
	curpos--;
    }

    return (word_end - curpos + 1);
}

/*
 * iline_getpos -- return absolute position
 */
size_t
iline_getpos()
{
    /* return current position */
    return curpos;
}

/*
 * iline_setpos -- set new position, relative to old pos. Note that the
 *		   cursor may be located _after_ last char (!)
 */
void
iline_setpos(p)
ssize_t p;
{
    size_t ncurpos;

    if (p < 0)
	p = -p, ncurpos = (curpos < p ? 0 : curpos - p);
    else
	ncurpos = ((curpos + p) > il_len ? il_len : curpos + p);

    if (ncurpos == curpos)
	tty_beep();		/* no change, so move to invalid pos */
    else
	curpos = ncurpos;
}

/*
 * iline_start - move to given position, relative to start of iline.
 */
void
iline_start(o)
size_t o;
{
   ASSERT(o <= il_len);
   curpos = 0 + o;
}

/*
 * iline_end - move to given position, relative to end of iline.
 */
void
iline_end(o)
size_t o;
{
    ASSERT(o <= il_len);
    curpos = il_len - o;
}

void
iline_save()
{
    (void) strcpy (il_save, il);
}

void
iline_undo()
{
    (void) strcpy (il, il_save);
    curpos = il_len = strlen(il);
}
