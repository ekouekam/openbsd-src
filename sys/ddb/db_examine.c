/*	$OpenBSD: db_examine.c,v 1.15 2007/09/13 17:14:17 miod Exp $	*/
/*	$NetBSD: db_examine.c,v 1.11 1996/03/30 22:30:07 christos Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>		/* type definitions */

#include <ddb/db_lex.h>
#include <ddb/db_output.h>
#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_access.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>

char	db_examine_format[TOK_STRING_SIZE] = "x";

/*
 * Examine (print) data.  Syntax is:
 *		x/[bhlq][cdiorsuxz]*
 * For example, the command:
 *  	x/bxxxx
 * should print:
 *  	address:  01  23  45  67
 */
/*ARGSUSED*/
void
db_examine_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	if (modif[0] != '\0')
		db_strlcpy(db_examine_format, modif, sizeof(db_examine_format));

	if (count == -1)
		count = 1;

	db_examine((db_addr_t)addr, db_examine_format, count);
}

void
db_examine(db_addr_t addr, char *fmt, int count)
{
	int		c;
	db_expr_t	value;
	int		size;
	int		width;
	char *		fp;
	char		tmpfmt[24];

	while (--count >= 0) {
		fp = fmt;
		size = 4;
		width = 12;
		while ((c = *fp++) != 0) {
			if (db_print_position() == 0) {
				/* Always print the address. */
				db_printsym(addr, DB_STGY_ANY, db_printf);
				db_printf(":\t");
				db_prev = addr;
			}
			switch (c) {
			case 'b':	/* byte */
				size = 1;
				width = 4;
				break;
			case 'h':	/* half-word */
				size = 2;
				width = 8;
				break;
			case 'l':	/* long-word */
				size = 4;
				width = 12;
				break;
#ifdef __LP64__
			case 'q':	/* quad-word */
				size = 8;
				width = 20;
				break;
#endif
			case 'a':	/* address */
				db_printf("= 0x%lx\n", (long)addr);
				break;
			case 'r':	/* signed, current radix */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_format(tmpfmt, sizeof tmpfmt,
				    (long)value, DB_FORMAT_R, 0, width);
				db_printf("%-*s", width, tmpfmt);
				break;
			case 'x':	/* unsigned hex */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				db_printf("%-*lx", width, (long)value);
				break;
			case 'z':	/* signed hex */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_format(tmpfmt, sizeof tmpfmt,
				    (long)value, DB_FORMAT_Z, 0, width);
				db_printf("%-*s", width, tmpfmt);
				break;
			case 'd':	/* signed decimal */
				value = db_get_value(addr, size, TRUE);
				addr += size;
				db_printf("%-*ld", width, (long)value);
				break;
			case 'u':	/* unsigned decimal */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				db_printf("%-*lu", width, (long)value);
				break;
			case 'o':	/* unsigned octal */
				value = db_get_value(addr, size, FALSE);
				addr += size;
				db_printf("%-*lo", width, value);
				break;
			case 'c':	/* character */
				value = db_get_value(addr, 1, FALSE);
				addr += 1;
				if (value >= ' ' && value <= '~')
					db_printf("%c", value);
				else
					db_printf("\\%03o", value);
				break;
			case 's':	/* null-terminated string */
				for (;;) {
					value = db_get_value(addr, 1, FALSE);
					addr += 1;
					if (value == 0)
						break;
					if (value >= ' ' && value <= '~')
						db_printf("%c", value);
					else
						db_printf("\\%03o", value);
				}
				break;
			case 'i':	/* instruction */
				addr = db_disasm(addr, FALSE);
				break;
			case 'I':	/* instruction, alternate form */
				addr = db_disasm(addr, TRUE);
				break;
			default:
				break;
			}
			if (db_print_position() != 0)
				db_end_line(width);
		}
	}
	db_next = addr;
}

/*
 * Print value.
 */
char	db_print_format = 'x';

/*ARGSUSED*/
void
db_print_cmd(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	db_expr_t	value;
	char		tmpfmt[24];

	if (modif[0] != '\0')
		db_print_format = modif[0];

	switch (db_print_format) {
	case 'a':
		db_printsym((db_addr_t)addr, DB_STGY_ANY, db_printf);
		break;
	case 'r':
		db_printf("%s", db_format(tmpfmt, sizeof tmpfmt, addr,
		    DB_FORMAT_R, 0, sizeof(db_expr_t) * 2 * 6 / 5));
		break;
	case 'x':
		db_printf("%*x", sizeof(db_expr_t) * 2, addr);
		break;
	case 'z':
		db_printf("%s", db_format(tmpfmt, sizeof tmpfmt, addr,
		    DB_FORMAT_Z, 0, sizeof(db_expr_t) * 2));
		break;
	case 'd':
		db_printf("%*d", sizeof(db_expr_t) * 2 * 6 / 5, addr);
		break;
	case 'u':
		db_printf("%*u", sizeof(db_expr_t) * 2 * 6 / 5, addr);
		break;
	case 'o':
		db_printf("%*o", sizeof(db_expr_t) * 2 * 4 / 3, addr);
		break;
	case 'c':
		value = addr & 0xFF;
		if (value >= ' ' && value <= '~')
			db_printf("%c", value);
		else
			db_printf("\\%03o", value);
		break;
	}
	db_printf("\n");
}

void
db_print_loc_and_inst(db_addr_t loc)
{
	db_printsym(loc, DB_STGY_PROC, db_printf);
	db_printf(":\t");
	(void) db_disasm(loc, FALSE);
}

/* local copy is needed here so that we can trace strlcpy() in libkern */
size_t
db_strlcpy(char *dst, const char *src, size_t siz)
{
	char *d = dst;
	const char *s = src;
	size_t n = siz;

	/* Copy as many bytes as will fit */
	if (n != 0 && --n != 0) {
		do {
			if ((*d++ = *s++) == 0)
				break;
		} while (--n != 0);
	}

	/* Not enough room in dst, add NUL and traverse rest of src */
	if (n == 0) {
		if (siz != 0)
			*d = '\0';		/* NUL-terminate dst */
		while (*s++)
			;
	}

	return(s - src - 1);	/* count does not include NUL */
}

/*
 * Search for a value in memory.
 * Syntax: search [/bhl] addr value [mask] [,count]
 */
/*ARGSUSED*/
void
db_search_cmd(db_expr_t daddr, int have_addr, db_expr_t dcount, char *modif)
{
	int		t;
	db_addr_t	addr;
	int		size;
	db_expr_t	value;
	db_expr_t	mask;
	db_expr_t	count;

	t = db_read_token();
	if (t == tSLASH) {
		t = db_read_token();
		if (t != tIDENT) {
			bad_modifier:
			db_printf("Bad modifier\n");
			db_flush_lex();
			return;
		}

		if (!strcmp(db_tok_string, "b"))
			size = 1;
		else if (!strcmp(db_tok_string, "h"))
			size = 2;
		else if (!strcmp(db_tok_string, "l"))
			size = 4;
		else
			goto bad_modifier;
	} else {
		db_unread_token(t);
		size = 4;
	}

	if (!db_expression(&value)) {
		db_printf("Address missing\n");
		db_flush_lex();
		return;
	}
	addr = (db_addr_t) value;

	if (!db_expression(&value)) {
		db_printf("Value missing\n");
		db_flush_lex();
		return;
	}

	if (!db_expression(&mask))
		mask = (int) ~0;

	t = db_read_token();
	if (t == tCOMMA) {
		if (!db_expression(&count)) {
			db_printf("Count missing\n");
			db_flush_lex();
			return;
		}
	} else {
		db_unread_token(t);
		count = -1;		/* forever */
	}
	db_skip_to_eol();

	db_search(addr, size, value, mask, count);
}

void
db_search(db_addr_t addr, int size, db_expr_t value, db_expr_t mask,
    db_expr_t count)
{
	/* Negative counts means forever.  */
	while (count < 0 || count-- != 0) {
		db_prev = addr;
		if ((db_get_value(addr, size, FALSE) & mask) == value)
			break;
		addr += size;
	}
	db_next = addr;
}
