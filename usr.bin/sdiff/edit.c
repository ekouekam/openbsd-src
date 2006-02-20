/*	$OpenBSD: edit.c,v 1.12 2006/02/20 08:38:18 otto Exp $ */

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "extern.h"

static void edit(const char *);

/*
 * Takes the name of a file and opens it with an editor.
 */
static void
edit(const char *filename)
{
	int status;
	pid_t pid;
	const char *editor;

	editor = getenv("VISUAL");
	if (editor == NULL)
		editor = getenv("EDITOR");
	if (editor == NULL)
		editor = "vi";

	/* Start editor on temporary file. */
	switch (pid = fork()) {
	case 0:
		/* child */
		execlp(editor, editor, filename, (void *)NULL);
		warn("could not execute editor: %s", editor);
		cleanup(filename);
		/* NOTREACHED */
	case -1:
		warn("could not fork");
		cleanup(filename);
		/* NOTREACHED */
	}

	/* parent */
	/* Wait for editor to exit. */
	if (waitpid(pid, &status, 0) == -1) {
		warn("waitpid");
		cleanup(filename);
		/* NOTREACHED */
	}

	/* Check that editor terminated normally. */
	if (!WIFEXITED(status)) {
		warn("%s terminated abnormally", editor);
		cleanup(filename);
		/* NOTREACHED */
	}
}

/*
 * Parse edit command.  Returns 0 on success, -1 on error.
 */
int
eparse(const char *cmd, const char *left, const char *right)
{
	FILE *file;
	size_t nread, nwritten;
	char *filename;
	char buf[BUFSIZ], *text;

	/* Skip whitespace. */
	while (isspace(*cmd))
		++cmd;

	text = NULL;
	switch (*cmd) {
	case '\0':
		/* Edit empty file. */
		break;

	case 'b':
		/* Both strings. */
		if (left == NULL)
			goto RIGHT;
		if (right == NULL)
			goto LEFT;

		/* Neither column is blank, so print both. */
		if (asprintf(&text, "%s\n%s\n", left, right) == -1)
			err(2, "could not allocate memory");
		break;

	case 'l':
LEFT:
		/* Skip if there is no left column. */
		if (left == NULL)
			break;

		if (asprintf(&text, "%s\n", left) == -1)
			err(2, "could not allocate memory");

		break;

	case 'r':
RIGHT:
		/* Skip if there is no right column. */
		if (right == NULL)
			break;

		if (asprintf(&text, "%s\n", right) == -1)
			err(2, "could not allocate memory");

		break;

	default:
		return (-1);
	}

	/* Create temp file. */
	filename = xmktemp(text);

	/* text is no longer used. */
	free(text);

	/* Edit temp file. */
	edit(filename);

	/* Open temporary file. */
	if (!(file = fopen(filename, "r"))) {
		warn("could not open edited file: %s", filename);
		cleanup(filename);
		/* NOTREACHED */
	}

	/* Copy temporary file contents to output file. */
	for (nread = sizeof(buf); nread == sizeof(buf);) {
		nread = fread(buf, sizeof(*buf), sizeof(buf), file);
		/* Test for error or end of file. */
		if (nread != sizeof(buf) &&
		    (ferror(file) || !feof(file))) {
			warnx("error reading edited file: %s", filename);
			cleanup(filename);
			/* NOTREACHED */
		}

		/*
		 * If we have nothing to read, break out of loop
		 * instead of writing nothing.
		 */
		if (!nread)
			break;

		/* Write data we just read. */
		nwritten = fwrite(buf, sizeof(*buf), nread, outfile);
		if (nwritten != nread) {
			warnx("error writing to output file");
			cleanup(filename);
			/* NOTREACHED */
		}
	}

	/* We've reached the end of the temporary file, so remove it. */
	if (unlink(filename))
		warn("could not delete: %s", filename);
	fclose(file);

	/* filename was malloc()ed in xmktemp(). */
	free(filename);

	return (0);
}
