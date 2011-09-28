/* $OpenBSD: cmd-refresh-client.c,v 1.6 2011/09/28 20:11:21 nicm Exp $ */

/*
 * Copyright (c) 2007 Nicholas Marriott <nicm@users.sourceforge.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include "tmux.h"

/*
 * Refresh client.
 */

int	cmd_refresh_client_exec(struct cmd *, struct cmd_ctx *);

const struct cmd_entry cmd_refresh_client_entry = {
	"refresh-client", "refresh",
	"St:", 0, 0,
	"[-S] " CMD_TARGET_CLIENT_USAGE,
	0,
	NULL,
	NULL,
	cmd_refresh_client_exec
};

int
cmd_refresh_client_exec(struct cmd *self, struct cmd_ctx *ctx)
{
	struct args	*args = self->args;
	struct client	*c;

	if ((c = cmd_find_client(ctx, args_get(args, 't'))) == NULL)
		return (-1);

	if (args_has(args, 'S')) {
		status_update_jobs(c);
		server_status_client(c);
	} else
		server_redraw_client(c);

	return (0);
}
