/*
 * $Id: opt.c,v 1.7 1998/10/29 11:16:10 vons Exp $
 *
 * opt.c -- manage the vrl options as defined in $HOME/.vrlrc
 */

#include "config.h"
#include "vrl.h"

#include <stdio.h>
#include <ctype.h>

/* user wants beep ? */
int       opt_do_beep = 1;

/* default insert mode ? */
int       opt_insmode = 1;

/* max hist cnt */
int       opt_histmax = 100;

/* add cmd to hist and remove any older copies */
int       opt_histmode = OPT_HIST_NOREPEAT;

/* autom. list all files on ambiguous completion */
int       opt_autolist = COMP_LIST_NOCHANGE;

/* recognise the 'ansi cursor sequences' */
int	  opt_ansicur = 1;

/* max keyword size (used for buffer allocation) must be one larger than kw */
#define KW_SIZE 20

/* buffer used for keyword/value storage */
static char kwbuf[KW_SIZE];

struct opt_descr
{
    const char *name;
    int      *var;
    struct
    {
        const char *str;
        const int num;
    }         optval[3];          /* max # possibilities for option is
                                   * currently 3 */
};

static const struct opt_descr opts[] =
{
    {"beep", &opt_do_beep,
        {{"on", 1},
        {"off", 0},
        {NULL, 0}
        }
    },
    {"insertmode", &opt_insmode,
        {{"on", 1},
        {"off", 0},
        {NULL, 0}
        }
    },
    {"histmax", &opt_histmax,     /* numeric option, no strings */
        {{NULL, 0}, {NULL, 0}, {NULL, 0}}
    },
    {"histmode", &opt_histmode,
        {{"all", OPT_HIST_ALL},
        {"norepeat", OPT_HIST_NOREPEAT},
        {"nodups", OPT_HIST_NODUPS},
        }
    },
    {"completion", &opt_autolist,
        {{"never", COMP_NOLIST},
        {"nochange", COMP_LIST_NOCHANGE},
        {"always", COMP_LIST_IF_AMBIGUOUS},
        }
    },
    {"ansicursorkeys", &opt_ansicur,
        {{"on", 1},
        {"off", 0},
        {NULL, 0}
        }
    },
};

static const char *inifile = ".vrlrc";
static const char dirsep[2] = {DIRSEP, '\0'};

#define GETWORD_OK	0
#define GETWORD_TOOLONG 1

PROTOTYPE(static int get_word, (FILE *, char *, size_t));
PROTOTYPE(static const char *get_optval, (FILE *f));
PROTOTYPE(static int locate_entry, (FILE *f));

void
opt_read()
{
    const char *h = getenv("HOME");
    char     *s;
    FILE     *f;

    /* Seems $HOME is not always there (got bug-report), so test it */
    if (h == NULL)
	return;

    /* construct $home/.vrlrc in buffer and try to open it */
    s = malloc(strlen(h) + strlen(inifile) + 2);
    (void) strcpy(s, h);
    (void) strcat(s, dirsep);
    (void) strcat(s, inifile);

    f = fopen(s, "r");

    (void) free(s);

    if (f == NULL)
        return;

    for (;;)
    {
        int       id = locate_entry(f);
        const char *kw;

        /* we're done if no new option keywd has been found */
        if (id < 0)
            break;

        /* get option value */
        kw = get_optval(f);

        /* is there a keyword ? */
        if (kw == NULL)
        {
            out_printf("%s: option '%s' misses value", inifile, opts[id].name);
        }
        else if (opts[id].optval[0].str == NULL)
        {
            /* numeric option */
            const Uchar *p;

            for (p = (const Uchar *) kw; *p != '\0'; p++)
                if (!isdigit(*p))
                {
                    out_printf("%s: option '%s' requires numeric value",
                               inifile, opts[id].name);
                    break;
                }

            if (*p == '\0')
            {
		/* 
		 * Accept 5 digits max. (need to use some limit, and
		 * atoi() won't complain if there are too many digits.
		 */
		if (strlen(kw) > 5)
		{
                    out_printf("%s: value of option '%s' is too big",
                               inifile, opts[id].name);
		}
		else
		    *(opts[id].var) = atoi(kw);
            }
        }
        else
        {                         /* string option */
            const int cnt = sizeof(opts[0].optval) / sizeof(opts[0].optval[0]);
            int       i;

            for (i = 0; i < cnt; i++)
            {
                if (opts[id].optval[i].str != NULL &&
                    strcmp(opts[id].optval[i].str, kw) == 0)
                {
                    *(opts[id].var) = opts[id].optval[i].num;
                    break;
                }
            }

            if (cnt == i)
            {
                out_printf("%s: option '%s' has unknown value '%s'",
                           inifile, opts[id].name, kw);
            }
        }
    }

    (void) fclose(f);
}

static int
locate_entry(f)
FILE     *f;
{
    for (;;)
    {
        int       c;

        /* find start of first word */
        while ((c = getc(f)) != EOF && isspace(c))
            /* loop */;

        if (c == EOF)
        {
            break;
        }
        else if (c != '#')
        {
            /* c contains first char of option name */
            kwbuf[0] = (char) c;

            /* get remainder of word */
            if (get_word(f, &(kwbuf[1]), KW_SIZE - 1) == GETWORD_OK)
            {
                const int cnt = sizeof(opts) / sizeof(opts[0]);
                int       i;

                for (i = 0; i < cnt; i++)
                {
                    if (opts[i].name != NULL &&
                        strcmp(opts[i].name, kwbuf) == 0)
                    {
                        /* valid option, return its id */
                        return i;
                    }
                }

                if (cnt == i)
                    out_printf("%s: unknown option '%s'", inifile, kwbuf);
            }
            else
                out_printf("%s: option name '%s...' too long", inifile, kwbuf);
        }
        /* else, c == '#' so nothing to do */

        /* move to start of next line */
        while ((c = getc(f)) != EOF && c != '\n')
           /* loop */;
    }

    return -1;
}

/*----------------------------------------------------------------------
 * get_optval - return ptr to option value.
 *
 * Returns -1 if it could not find an option name
 */
static const char *
get_optval(f)
FILE     *f;
{
    int       c;
    int       spaceonly;

    while ((c = getc(f)) != EOF && isspace(c) && c != '\n')
       /* loop */;

    if (c == EOF || c == '\n')
        return NULL;

    kwbuf[0] = (char) c;

    /* get remainder of word (truncated if too long) */
    (void) get_word(f, &(kwbuf[1]), KW_SIZE - 2);

    /* skip remainder of line */
    spaceonly = 1;
    while ((c = getc(f)) != EOF && c != '\n')
    {
	if (!isspace(c) && spaceonly)
	{
	    spaceonly = 0;
	    out_printf("%s: junk found after option value '%s'",
			inifile, kwbuf);
	}
    }

    return &(kwbuf[0]);
}

/*----------------------------------------------------------------------
 * get_word - read remainder of word, until whitesp, EOF, or \n.
 *
 * Return GETWORD_OK if everything is ok, return GETWORD_TOOLONG if
 * keyword is truncated.
 */
static int
get_word(f, buf, bufsiz)
FILE     *f;
char     *buf;
size_t    bufsiz;
{
    int       c = 0; /* 0 == something other than white space */
    int       i = 0;

    /* note that isspace() includes \n */
    while (i < bufsiz-1 && (c = getc(f)) != EOF && !isspace(c))
        buf[i++] = (char) c;

    buf[i] = '\0';

    /* push back the char after the word */
    if (isspace(c))
	(void) ungetc(c, f);

    return (i == bufsiz ? GETWORD_TOOLONG : GETWORD_OK);
}
