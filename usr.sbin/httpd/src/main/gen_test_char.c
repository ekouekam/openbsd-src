/* we need some of the portability definitions... for strchr */
#include "httpd.h"

/* A bunch of functions in util.c scan strings looking for certain characters.
 * To make that more efficient we encode a lookup table.
 */
#define T_ESCAPE_SHELL_CMD	(0x01)
#define T_ESCAPE_PATH_SEGMENT	(0x02)
#define T_OS_ESCAPE_PATH	(0x04)
#define T_HTTP_TOKEN_STOP	(0x08)
#define T_ESCAPE_LOGITEM	(0x10)

int main(int argc, char *argv[])
{
    unsigned c;
    unsigned char flags;

    printf(
"/* this file is automatically generated by gen_test_char, do not edit */\n"
"#define T_ESCAPE_SHELL_CMD	0x%02x /* chars with special meaning in the shell */\n"
"#define T_ESCAPE_PATH_SEGMENT	0x%02x /* find path segment, as defined in RFC1808 */\n"
"#define T_OS_ESCAPE_PATH	0x%02x /* escape characters in a path or uri */\n"
"#define T_HTTP_TOKEN_STOP	0x%02x /* find http tokens, as defined in RFC2616 */\n"
"#define T_ESCAPE_LOGITEM	0x%02x /* filter what should go in the log file */\n"
"\n",
	T_ESCAPE_SHELL_CMD,
	T_ESCAPE_PATH_SEGMENT,
	T_OS_ESCAPE_PATH,
	T_HTTP_TOKEN_STOP,
	T_ESCAPE_LOGITEM
	);

    /* we explicitly dealt with NUL above
     * in case some strchr() do bogosity with it */

    printf("static const unsigned char test_char_table[256] = {\n"
	   "    0x00, ");    /* print initial item */

    for (c = 1; c < 256; ++c) {
	flags = 0;

	/* escape_shell_cmd */
#if defined(WIN32) || defined(OS2)
        /* Win32/OS2 have many of the same vulnerable characters
         * as Unix sh, plus the carriage return and percent char.
         * The proper escaping of these characters varies from unix
         * since Win32/OS2 use carets or doubled-double quotes, 
         * and neither lf nor cr can be escaped.  We escape unix 
         * specific as well, to assure that cross-compiled unix 
         * applications behave similiarly when invoked on win32/os2.
         */
        if (strchr("&;`'\"|*?~<>^()[]{}$\\\n\r%", c)) {
	    flags |= T_ESCAPE_SHELL_CMD;
	}
#else
        if (strchr("&;`'\"|*?~<>^()[]{}$\\\n", c)) {
	    flags |= T_ESCAPE_SHELL_CMD;
	}
#endif

	if (!ap_isalnum(c) && !strchr("$-_.+!*'(),:@&=~", c)) {
	    flags |= T_ESCAPE_PATH_SEGMENT;
	}

	if (!ap_isalnum(c) && !strchr("$-_.+!*'(),:@&=/~", c)) {
	    flags |= T_OS_ESCAPE_PATH;
	}

	/* these are the "tspecials" from RFC2068 */
	if (ap_iscntrl(c) || strchr(" \t()<>@,;:\\/[]?={}", c)) {
	    flags |= T_HTTP_TOKEN_STOP;
	}

	/* For logging, escape all control characters,
	 * double quotes (because they delimit the request in the log file)
	 * backslashes (because we use backslash for escaping)
	 * and 8-bit chars with the high bit set
	 */
	if (!ap_isprint(c) || c == '"' || c == '\\' || ap_iscntrl(c)) {
	    flags |= T_ESCAPE_LOGITEM;
	}
	printf("0x%02x%s", flags, (c < 255) ? ", " : "  ");

	if ((c % 8) == 7)
	    printf(" /*0x%02x...0x%02x*/\n    ", c-7, c);
    }
    printf("\n};\n");

    return 0;
}
