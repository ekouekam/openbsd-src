/*	$OpenBSD: relay.c,v 1.162 2013/02/05 21:36:33 bluhm Exp $	*/

/*
 * Copyright (c) 2006 - 2012 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/tree.h>
#include <sys/hash.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <pwd.h>
#include <event.h>
#include <fnmatch.h>

#include <openssl/ssl.h>

#include "relayd.h"

void		 relay_statistics(int, short, void *);
int		 relay_dispatch_parent(int, struct privsep_proc *,
		    struct imsg *);
int		 relay_dispatch_pfe(int, struct privsep_proc *,
		    struct imsg *);
void		 relay_shutdown(void);

void		 relay_nodedebug(const char *, struct protonode *);
void		 relay_protodebug(struct relay *);
void		 relay_init(struct privsep *, struct privsep_proc *p, void *);
void		 relay_launch(void);
int		 relay_socket(struct sockaddr_storage *, in_port_t,
		    struct protocol *, int, int);
int		 relay_socket_listen(struct sockaddr_storage *, in_port_t,
		    struct protocol *);
int		 relay_socket_connect(struct sockaddr_storage *, in_port_t,
		    struct protocol *, int);

void		 relay_accept(int, short, void *);
void		 relay_input(struct rsession *);

u_int32_t	 relay_hash_addr(struct sockaddr_storage *, u_int32_t);

int		 relay_splice(struct ctl_relay_event *);
int		 relay_splicelen(struct ctl_relay_event *);

SSL_CTX		*relay_ssl_ctx_create(struct relay *);
void		 relay_ssl_transaction(struct rsession *,
		    struct ctl_relay_event *);
void		 relay_ssl_accept(int, short, void *);
void		 relay_connect_retry(int, short, void *);
void		 relay_ssl_connect(int, short, void *);
void		 relay_ssl_connected(struct ctl_relay_event *);
void		 relay_ssl_readcb(int, short, void *);
void		 relay_ssl_writecb(int, short, void *);

char		*relay_load_file(const char *, off_t *);
static __inline int
		 relay_proto_cmp(struct protonode *, struct protonode *);
extern void	 bufferevent_read_pressure_cb(struct evbuffer *, size_t,
		    size_t, void *);

volatile int relay_sessions;
volatile int relay_inflight = 0;
objid_t relay_conid;

static struct relayd		*env = NULL;
int				 proc_id;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	relay_dispatch_parent },
	{ "pfe",	PROC_PFE,	relay_dispatch_pfe },
};

pid_t
relay(struct privsep *ps, struct privsep_proc *p)
{
	env = ps->ps_env;
	return (proc_run(ps, p, procs, nitems(procs), relay_init, NULL));
}

void
relay_shutdown(void)
{
	config_purge(env, CONFIG_ALL);
	usleep(200);	/* XXX relay needs to shutdown last */
}

void
relay_nodedebug(const char *name, struct protonode *pn)
{
	const char	*s;
	int		 digest;

	if (pn->action == NODE_ACTION_NONE)
		return;

	fprintf(stderr, "\t\t");
	fprintf(stderr, "%s ", name);

	switch (pn->type) {
	case NODE_TYPE_HEADER:
		break;
	case NODE_TYPE_QUERY:
		fprintf(stderr, "query ");
		break;
	case NODE_TYPE_COOKIE:
		fprintf(stderr, "cookie ");
		break;
	case NODE_TYPE_PATH:
		fprintf(stderr, "path ");
		break;
	case NODE_TYPE_URL:
		fprintf(stderr, "url ");
		break;
	}

	switch (pn->action) {
	case NODE_ACTION_APPEND:
		fprintf(stderr, "append \"%s\" to \"%s\"",
		    pn->value, pn->key);
		break;
	case NODE_ACTION_CHANGE:
		fprintf(stderr, "change \"%s\" to \"%s\"",
		    pn->key, pn->value);
		break;
	case NODE_ACTION_REMOVE:
		fprintf(stderr, "remove \"%s\"",
		    pn->key);
		break;
	case NODE_ACTION_EXPECT:
	case NODE_ACTION_FILTER:
		s = pn->action == NODE_ACTION_EXPECT ? "expect" : "filter";
		digest = pn->flags & PNFLAG_LOOKUP_URL_DIGEST;
		if (strcmp(pn->value, "*") == 0)
			fprintf(stderr, "%s %s\"%s\"", s,
			    digest ? "digest " : "", pn->key);
		else
			fprintf(stderr, "%s \"%s\" from \"%s\"", s,
			    pn->value, pn->key);
		break;
	case NODE_ACTION_HASH:
		fprintf(stderr, "hash \"%s\"", pn->key);
		break;
	case NODE_ACTION_LOG:
		fprintf(stderr, "log \"%s\"", pn->key);
		break;
	case NODE_ACTION_MARK:
		if (strcmp(pn->value, "*") == 0)
			fprintf(stderr, "mark \"%s\"", pn->key);
		else
			fprintf(stderr, "mark \"%s\" from \"%s\"",
			    pn->value, pn->key);
		break;
	case NODE_ACTION_NONE:
		break;
	}
	fprintf(stderr, "\n");
}

void
relay_protodebug(struct relay *rlay)
{
	struct protocol		*proto = rlay->rl_proto;
	struct protonode	*proot, *pn;
	struct proto_tree	*tree;
	const char		*name;
	int			 i;

	fprintf(stderr, "protocol %d: name %s\n",
	    proto->id, proto->name);
	fprintf(stderr, "\tflags: %s, relay flags: %s\n",
	    printb_flags(proto->flags, F_BITS),
	    printb_flags(rlay->rl_conf.flags, F_BITS));
	if (proto->tcpflags)
		fprintf(stderr, "\ttcp flags: %s\n",
		    printb_flags(proto->tcpflags, TCPFLAG_BITS));
	if ((rlay->rl_conf.flags & (F_SSL|F_SSLCLIENT)) && proto->sslflags)
		fprintf(stderr, "\tssl flags: %s\n",
		    printb_flags(proto->sslflags, SSLFLAG_BITS));
	if (proto->cache != -1)
		fprintf(stderr, "\tssl session cache: %d\n", proto->cache);
	fprintf(stderr, "\ttype: ");
	switch (proto->type) {
	case RELAY_PROTO_TCP:
		fprintf(stderr, "tcp\n");
		break;
	case RELAY_PROTO_HTTP:
		fprintf(stderr, "http\n");
		break;
	case RELAY_PROTO_DNS:
		fprintf(stderr, "dns\n");
		break;
	}

	name = "request";
	tree = &proto->request_tree;
 show:
	i = 0;
	RB_FOREACH(proot, proto_tree, tree) {
#if DEBUG > 1
		i = 0;
#endif
		PROTONODE_FOREACH(pn, proot, entry) {
#if DEBUG > 1
			i = 0;
#endif
			if (++i > 100)
				break;
			relay_nodedebug(name, pn);
		}
		/* Limit the number of displayed lines */
		if (++i > 100) {
			fprintf(stderr, "\t\t...\n");
			break;
		}
	}
	if (tree == &proto->request_tree) {
		name = "response";
		tree = &proto->response_tree;
		goto show;
	}
}

int
relay_privinit(struct relay *rlay)
{
	extern int	 debug;

	log_debug("%s: adding relay %s", __func__, rlay->rl_conf.name);

	if (debug)
		relay_protodebug(rlay);

	switch (rlay->rl_proto->type) {
	case RELAY_PROTO_DNS:
		relay_udp_privinit(env, rlay);
		break;
	case RELAY_PROTO_TCP:
	case RELAY_PROTO_HTTP:
		/* Use defaults */
		break;
	}

	if (rlay->rl_conf.flags & F_UDP)
		rlay->rl_s = relay_udp_bind(&rlay->rl_conf.ss,
		    rlay->rl_conf.port, rlay->rl_proto);
	else
		rlay->rl_s = relay_socket_listen(&rlay->rl_conf.ss,
		    rlay->rl_conf.port, rlay->rl_proto);
	if (rlay->rl_s == -1)
		return (-1);

	return (0);
}

void
relay_init(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct timeval	 tv;

	if (config_init(ps->ps_env) == -1)
		fatal("failed to initialize configuration");

	/* Set to current prefork id */
	proc_id = p->p_instance;

	/* We use a custom shutdown callback */
	p->p_shutdown = relay_shutdown;

	/* Unlimited file descriptors (use system limits) */
	socket_rlimit(-1);

	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, relay_statistics, NULL);
	bcopy(&env->sc_statinterval, &tv, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
}

void
relay_statistics(int fd, short events, void *arg)
{
	struct relay		*rlay;
	struct ctl_stats	 crs, *cur;
	struct timeval		 tv, tv_now;
	int			 resethour = 0, resetday = 0;
	struct rsession		*con, *next_con;

	/*
	 * This is a hack to calculate some average statistics.
	 * It doesn't try to be very accurate, but could be improved...
	 */

	timerclear(&tv);
	if (gettimeofday(&tv_now, NULL) == -1)
		fatal("relay_init: gettimeofday");

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		bzero(&crs, sizeof(crs));
		resethour = resetday = 0;

		cur = &rlay->rl_stats[proc_id];
		cur->cnt += cur->last;
		cur->tick++;
		cur->avg = (cur->last + cur->avg) / 2;
		cur->last_hour += cur->last;
		if ((cur->tick % (3600 / env->sc_statinterval.tv_sec)) == 0) {
			cur->avg_hour = (cur->last_hour + cur->avg_hour) / 2;
			resethour++;
		}
		cur->last_day += cur->last;
		if ((cur->tick % (86400 / env->sc_statinterval.tv_sec)) == 0) {
			cur->avg_day = (cur->last_day + cur->avg_day) / 2;
			resethour++;
		}
		bcopy(cur, &crs, sizeof(crs));

		cur->last = 0;
		if (resethour)
			cur->last_hour = 0;
		if (resetday)
			cur->last_day = 0;

		crs.id = rlay->rl_conf.id;
		crs.proc = proc_id;
		proc_compose_imsg(env->sc_ps, PROC_PFE, -1, IMSG_STATISTICS, -1,
		    &crs, sizeof(crs));

		for (con = SPLAY_ROOT(&rlay->rl_sessions);
		    con != NULL; con = next_con) {
			next_con = SPLAY_NEXT(session_tree,
			    &rlay->rl_sessions, con);
			timersub(&tv_now, &con->se_tv_last, &tv);
			if (timercmp(&tv, &rlay->rl_conf.timeout, >=))
				relay_close(con, "hard timeout");
		}
	}

	/* Schedule statistics timer */
	evtimer_set(&env->sc_statev, relay_statistics, NULL);
	bcopy(&env->sc_statinterval, &tv, sizeof(tv));
	evtimer_add(&env->sc_statev, &tv);
}

void
relay_launch(void)
{
	void			(*callback)(int, short, void *);
	struct relay		*rlay;
	struct host		*host;
	struct relay_table	*rlt;

	TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
		if ((rlay->rl_conf.flags & (F_SSL|F_SSLCLIENT)) &&
		    (rlay->rl_ssl_ctx = relay_ssl_ctx_create(rlay)) == NULL)
			fatal("relay_init: failed to create SSL context");

		TAILQ_FOREACH(rlt, &rlay->rl_tables, rlt_entry) {
			switch (rlt->rlt_mode) {
			case RELAY_DSTMODE_ROUNDROBIN:
			case RELAY_DSTMODE_RANDOM:
				rlt->rlt_key = 0;
				break;
			case RELAY_DSTMODE_LOADBALANCE:
			case RELAY_DSTMODE_HASH:
			case RELAY_DSTMODE_SRCHASH:
				rlt->rlt_key =
				    hash32_str(rlay->rl_conf.name, HASHINIT);
				rlt->rlt_key =
				    hash32_str(rlt->rlt_table->conf.name,
				    rlt->rlt_key);
				break;
			}
			rlt->rlt_nhosts = 0;
			TAILQ_FOREACH(host, &rlt->rlt_table->hosts, entry) {
				if (rlt->rlt_nhosts >= RELAY_MAXHOSTS)
					fatal("relay_init: "
					    "too many hosts in table");
				host->idx = rlt->rlt_nhosts;
				rlt->rlt_host[rlt->rlt_nhosts++] = host;
			}
			log_info("adding %d hosts from table %s%s",
			    rlt->rlt_nhosts, rlt->rlt_table->conf.name,
			    rlt->rlt_table->conf.check ? "" : " (no check)");
		}

		switch (rlay->rl_proto->type) {
		case RELAY_PROTO_DNS:
			relay_udp_init(rlay);
			break;
		case RELAY_PROTO_TCP:
		case RELAY_PROTO_HTTP:
			/* Use defaults */
			break;
		}

		log_debug("%s: running relay %s", __func__,
		    rlay->rl_conf.name);

		rlay->rl_up = HOST_UP;

		if (rlay->rl_conf.flags & F_UDP)
			callback = relay_udp_server;
		else
			callback = relay_accept;

		event_set(&rlay->rl_ev, rlay->rl_s, EV_READ,
		    callback, rlay);
		event_add(&rlay->rl_ev, NULL);
		evtimer_set(&rlay->rl_evt, callback, rlay);
	}
}

int
relay_socket_af(struct sockaddr_storage *ss, in_port_t port)
{
	switch (ss->ss_family) {
	case AF_INET:
		((struct sockaddr_in *)ss)->sin_port = port;
		((struct sockaddr_in *)ss)->sin_len =
		    sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)ss)->sin6_port = port;
		((struct sockaddr_in6 *)ss)->sin6_len =
		    sizeof(struct sockaddr_in6);
		break;
	default:
		return (-1);
	}

	return (0);
}

in_port_t
relay_socket_getport(struct sockaddr_storage *ss)
{
	switch (ss->ss_family) {
	case AF_INET:
		return (((struct sockaddr_in *)ss)->sin_port);
	case AF_INET6:
		return (((struct sockaddr_in6 *)ss)->sin6_port);
	default:
		return (0);
	}

	/* NOTREACHED */
	return (0);
}

int
relay_socket(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto, int fd, int reuseport)
{
	int s = -1, val;
	struct linger lng;

	if (relay_socket_af(ss, port) == -1)
		goto bad;

	s = fd == -1 ? socket(ss->ss_family, SOCK_STREAM, IPPROTO_TCP) : fd;
	if (s == -1)
		goto bad;

	/*
	 * Socket options
	 */
	bzero(&lng, sizeof(lng));
	if (setsockopt(s, SOL_SOCKET, SO_LINGER, &lng, sizeof(lng)) == -1)
		goto bad;
	if (reuseport) {
		val = 1;
		if (setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &val,
			sizeof(int)) == -1)
			goto bad;
	}
	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto bad;
	if (proto->tcpflags & TCPFLAG_BUFSIZ) {
		val = proto->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
		val = proto->tcpbufsiz;
		if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * IP options
	 */
	if (proto->tcpflags & TCPFLAG_IPTTL) {
		val = (int)proto->tcpipttl;
		if (setsockopt(s, IPPROTO_IP, IP_TTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (proto->tcpflags & TCPFLAG_IPMINTTL) {
		val = (int)proto->tcpipminttl;
		if (setsockopt(s, IPPROTO_IP, IP_MINTTL,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	/*
	 * TCP options
	 */
	if (proto->tcpflags & (TCPFLAG_NODELAY|TCPFLAG_NNODELAY)) {
		if (proto->tcpflags & TCPFLAG_NNODELAY)
			val = 0;
		else
			val = 1;
		if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
		    &val, sizeof(val)) == -1)
			goto bad;
	}
	if (proto->tcpflags & (TCPFLAG_SACK|TCPFLAG_NSACK)) {
		if (proto->tcpflags & TCPFLAG_NSACK)
			val = 0;
		else
			val = 1;
		if (setsockopt(s, IPPROTO_TCP, TCP_SACK_ENABLE,
		    &val, sizeof(val)) == -1)
			goto bad;
	}

	return (s);

 bad:
	if (s != -1)
		close(s);
	return (-1);
}

int
relay_socket_connect(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto, int fd)
{
	int	s;

	if ((s = relay_socket(ss, port, proto, fd, 0)) == -1)
		return (-1);

	if (connect(s, (struct sockaddr *)ss, ss->ss_len) == -1) {
		if (errno != EINPROGRESS)
			goto bad;
	}

	return (s);

 bad:
	close(s);
	return (-1);
}

int
relay_socket_listen(struct sockaddr_storage *ss, in_port_t port,
    struct protocol *proto)
{
	int s;

	if ((s = relay_socket(ss, port, proto, -1, 1)) == -1)
		return (-1);

	if (bind(s, (struct sockaddr *)ss, ss->ss_len) == -1)
		goto bad;
	if (listen(s, proto->tcpbacklog) == -1)
		goto bad;

	return (s);

 bad:
	close(s);
	return (-1);
}

void
relay_connected(int fd, short sig, void *arg)
{
	struct rsession		*con = arg;
	struct relay		*rlay = con->se_relay;
	struct protocol		*proto = rlay->rl_proto;
	evbuffercb		 outrd = relay_read;
	evbuffercb		 outwr = relay_write;
	struct bufferevent	*bev;
	struct ctl_relay_event	*out = &con->se_out;
	socklen_t		 len;
	int			 error;

	if (sig == EV_TIMEOUT) {
		relay_abort_http(con, 504, "connect timeout", 0);
		return;
	}

	len = sizeof(error);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error,
	    &len) == -1 || error) {
		if (error)
			errno = error;
		relay_abort_http(con, 500, "socket error", 0);
		return;
	}

	if ((rlay->rl_conf.flags & F_SSLCLIENT) && (out->ssl == NULL)) {
		relay_ssl_transaction(con, out);
		return;
	}

	DPRINTF("%s: session %d: %ssuccessful", __func__,
	    con->se_id, rlay->rl_proto->lateconnect ? "late connect " : "");

	switch (rlay->rl_proto->type) {
	case RELAY_PROTO_HTTP:
		/* Check the servers's HTTP response */
		if (!RB_EMPTY(&rlay->rl_proto->response_tree)) {
			con->se_out.toread = TOREAD_HTTP_HEADER;
			outrd = relay_read_http;
			if ((con->se_out.nodes = calloc(proto->response_nodes,
			    sizeof(u_int8_t))) == NULL) {
				relay_abort_http(con, 500,
				    "failed to allocate nodes", 0);
				return;
			}
		}
		break;
	case RELAY_PROTO_TCP:
		/* Use defaults */
		break;
	default:
		fatalx("relay_connected: unknown protocol");
	}

	/*
	 * Relay <-> Server
	 */
	bev = bufferevent_new(fd, outrd, outwr, relay_error, &con->se_out);
	if (bev == NULL) {
		relay_abort_http(con, 500,
		    "failed to allocate output buffer event", 0);
		return;
	}
	evbuffer_free(bev->output);
	bev->output = con->se_out.output;
	if (bev->output == NULL)
		fatal("relay_connected: invalid output buffer");
	con->se_out.bev = bev;

	/* Initialize the SSL wrapper */
	if ((rlay->rl_conf.flags & F_SSLCLIENT) && (out->ssl != NULL))
		relay_ssl_connected(out);

	bufferevent_settimeout(bev,
	    rlay->rl_conf.timeout.tv_sec, rlay->rl_conf.timeout.tv_sec);
	bufferevent_enable(bev, EV_READ|EV_WRITE);

	if (relay_splice(&con->se_out) == -1)
		relay_close(con, strerror(errno));
}

void
relay_input(struct rsession *con)
{
	struct relay	*rlay = con->se_relay;
	struct protocol *proto = rlay->rl_proto;
	evbuffercb	 inrd = relay_read;
	evbuffercb	 inwr = relay_write;

	switch (rlay->rl_proto->type) {
	case RELAY_PROTO_HTTP:
		/* Check the client's HTTP request */
		if (!RB_EMPTY(&rlay->rl_proto->request_tree) ||
		    proto->lateconnect) {
			con->se_in.toread = TOREAD_HTTP_HEADER;
			inrd = relay_read_http;
			if ((con->se_in.nodes = calloc(proto->request_nodes,
			    sizeof(u_int8_t))) == NULL) {
				relay_close(con, "failed to allocate nodes");
				return;
			}
		}
		break;
	case RELAY_PROTO_TCP:
		/* Use defaults */
		break;
	default:
		fatalx("relay_input: unknown protocol");
	}

	/*
	 * Client <-> Relay
	 */
	con->se_in.bev = bufferevent_new(con->se_in.s, inrd, inwr,
	    relay_error, &con->se_in);
	if (con->se_in.bev == NULL) {
		relay_close(con, "failed to allocate input buffer event");
		return;
	}

	/* Initialize the SSL wrapper */
	if ((rlay->rl_conf.flags & F_SSL) && con->se_in.ssl != NULL)
		relay_ssl_connected(&con->se_in);

	bufferevent_settimeout(con->se_in.bev,
	    rlay->rl_conf.timeout.tv_sec, rlay->rl_conf.timeout.tv_sec);
	bufferevent_enable(con->se_in.bev, EV_READ|EV_WRITE);

	if (relay_splice(&con->se_in) == -1)
		relay_close(con, strerror(errno));
}

void
relay_write(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = arg;
	struct rsession		*con = cre->con;
	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		con->se_done = 1;
	if (con->se_done)
		relay_close(con, "last write (done)");
}

void
relay_dump(struct ctl_relay_event *cre, const void *buf, size_t len)
{
	if (!len)
		return;

	/*
	 * This function will dump the specified message directly
	 * to the underlying session, without waiting for success
	 * of non-blocking events etc. This is useful to print an
	 * error message before gracefully closing the session.
	 */
	if (cre->ssl != NULL)
		(void)SSL_write(cre->ssl, buf, len);
	else
		(void)write(cre->s, buf, len);
}

void
relay_read(struct bufferevent *bev, void *arg)
{
	struct ctl_relay_event	*cre = arg;
	struct rsession		*con = cre->con;
	struct evbuffer		*src = EVBUFFER_INPUT(bev);

	if (gettimeofday(&con->se_tv_last, NULL) == -1)
		goto fail;
	if (!EVBUFFER_LENGTH(src))
		return;
	if (relay_bufferevent_write_buffer(cre->dst, src) == -1)
		goto fail;
	if (con->se_done)
		goto done;
	if (cre->dst->bev)
		bufferevent_enable(cre->dst->bev, EV_READ);
	return;
 done:
	relay_close(con, "last read (done)");
	return;
 fail:
	relay_close(con, strerror(errno));
}

int
relay_lognode(struct rsession *con, struct protonode *pn, struct protonode *pk,
    char *buf, size_t len)
{
	const char		*label = NULL;

	if ((pn->flags & PNFLAG_LOG) == 0)
		return (0);
	bzero(buf, len);
	if (pn->label != 0)
		label = pn_id2name(pn->label);
	if (snprintf(buf, len, " [%s%s%s: %s]",
	    label == NULL ? "" : label,
	    label == NULL ? "" : ", ",
	    pk->key, pk->value) == -1 ||
	    evbuffer_add(con->se_log, buf, strlen(buf)) == -1)
		return (-1);
	return (0);
}

int
relay_splice(struct ctl_relay_event *cre)
{
	struct rsession		*con = cre->con;
	struct relay		*rlay = con->se_relay;
	struct protocol		*proto = rlay->rl_proto;
	struct splice		 sp;

	if ((rlay->rl_conf.flags & (F_SSL|F_SSLCLIENT)) ||
	    (proto->tcpflags & TCPFLAG_NSPLICE))
		return (0);

	if (cre->bev->readcb != relay_read)
		return (0);

	bzero(&sp, sizeof(sp));
	sp.sp_fd = cre->dst->s;
	sp.sp_idle = rlay->rl_conf.timeout;
	if (setsockopt(cre->s, SOL_SOCKET, SO_SPLICE, &sp, sizeof(sp)) == -1) {
		log_debug("%s: session %d: splice dir %d failed: %s",
		    __func__, con->se_id, cre->dir, strerror(errno));
		return (-1);
	}
	cre->splicelen = 0;
	DPRINTF("%s: session %d: splice dir %d successful",
	    __func__, con->se_id, cre->dir);
	return (1);
}

int
relay_splicelen(struct ctl_relay_event *cre)
{
	struct rsession		*con = cre->con;
	off_t			 len;
	socklen_t		 optlen;

	optlen = sizeof(len);
	if (getsockopt(cre->s, SOL_SOCKET, SO_SPLICE, &len, &optlen) == -1) {
		log_debug("%s: session %d: splice dir %d get length failed: %s",
		    __func__, con->se_id, cre->dir, strerror(errno));
		return (-1);
	}
	if (len > cre->splicelen) {
		cre->splicelen = len;
		return (1);
	}
	return (0);
}

void
relay_error(struct bufferevent *bev, short error, void *arg)
{
	struct ctl_relay_event *cre = arg;
	struct rsession *con = cre->con;
	struct evbuffer *dst;

	if (error & EVBUFFER_TIMEOUT) {
		if (cre->splicelen >= 0) {
			bufferevent_enable(bev, EV_READ);
		} else if (cre->dst->splicelen >= 0) {
			switch (relay_splicelen(cre->dst)) {
			case -1:
				goto fail;
			case 0:
				relay_close(con, "buffer event timeout");
				break;
			case 1:
				bufferevent_enable(bev, EV_READ);
				break;
			}
		} else {
			relay_close(con, "buffer event timeout");
		}
		return;
	}
	if (error & EVBUFFER_ERROR && errno == ETIMEDOUT) {
		if (cre->dst->splicelen >= 0) {
			switch (relay_splicelen(cre->dst)) {
			case -1:
				goto fail;
			case 0:
				relay_close(con, "splice timeout");
				return;
			case 1:
				bufferevent_enable(bev, EV_READ);
				break;
			}
		}
		if (relay_splice(cre) == -1)
			goto fail;
		return;
	}
	if (error & (EVBUFFER_READ|EVBUFFER_WRITE|EVBUFFER_EOF)) {
		bufferevent_disable(bev, EV_READ|EV_WRITE);

		con->se_done = 1;
		if (cre->dst->bev != NULL) {
			dst = EVBUFFER_OUTPUT(cre->dst->bev);
			if (EVBUFFER_LENGTH(dst))
				return;
		} else
			return;

		relay_close(con, "done");
		return;
	}
	relay_close(con, "buffer event error");
	return;
 fail:
	relay_close(con, strerror(errno));
}

void
relay_accept(int fd, short event, void *arg)
{
	struct relay *rlay = arg;
	struct protocol *proto = rlay->rl_proto;
	struct rsession *con = NULL;
	struct ctl_natlook *cnl = NULL;
	socklen_t slen;
	struct timeval tv;
	struct sockaddr_storage ss;
	int s = -1;

	event_add(&rlay->rl_ev, NULL);
	if ((event & EV_TIMEOUT))
		return;

	slen = sizeof(ss);
	if ((s = accept_reserve(fd, (struct sockaddr *)&ss,
	    &slen, FD_RESERVE, &relay_inflight)) == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			struct timeval evtpause = { 1, 0 };

			event_del(&rlay->rl_ev);
			evtimer_add(&rlay->rl_evt, &evtpause);
			log_debug("%s: deferring connections", __func__);
		}
		return;
	}
	if (relay_sessions >= RELAY_MAX_SESSIONS ||
	    rlay->rl_conf.flags & F_DISABLE)
		goto err;

	if (fcntl(s, F_SETFL, O_NONBLOCK) == -1)
		goto err;

	if ((con = calloc(1, sizeof(*con))) == NULL)
		goto err;

	con->se_in.s = s;
	con->se_in.ssl = NULL;
	con->se_out.s = -1;
	con->se_out.ssl = NULL;
	con->se_in.dst = &con->se_out;
	con->se_out.dst = &con->se_in;
	con->se_in.con = con;
	con->se_out.con = con;
	con->se_in.splicelen = -1;
	con->se_out.splicelen = -1;
	con->se_in.toread = TOREAD_UNLIMITED;
	con->se_out.toread = TOREAD_UNLIMITED;
	con->se_relay = rlay;
	con->se_id = ++relay_conid;
	con->se_relayid = rlay->rl_conf.id;
	con->se_pid = getpid();
	con->se_in.tree = &proto->request_tree;
	con->se_out.tree = &proto->response_tree;
	con->se_in.dir = RELAY_DIR_REQUEST;
	con->se_out.dir = RELAY_DIR_RESPONSE;
	con->se_retry = rlay->rl_conf.dstretry;
	con->se_bnds = -1;
	if (gettimeofday(&con->se_tv_start, NULL) == -1)
		goto err;
	bcopy(&con->se_tv_start, &con->se_tv_last, sizeof(con->se_tv_last));
	bcopy(&ss, &con->se_in.ss, sizeof(con->se_in.ss));
	con->se_out.port = rlay->rl_conf.dstport;
	switch (ss.ss_family) {
	case AF_INET:
		con->se_in.port = ((struct sockaddr_in *)&ss)->sin_port;
		break;
	case AF_INET6:
		con->se_in.port = ((struct sockaddr_in6 *)&ss)->sin6_port;
		break;
	}

	relay_sessions++;
	SPLAY_INSERT(session_tree, &rlay->rl_sessions, con);

	/* Increment the per-relay session counter */
	rlay->rl_stats[proc_id].last++;

	/* Pre-allocate output buffer */
	con->se_out.output = evbuffer_new();
	if (con->se_out.output == NULL) {
		relay_close(con, "failed to allocate output buffer");
		return;
	}

	/* Pre-allocate log buffer */
	con->se_log = evbuffer_new();
	if (con->se_log == NULL) {
		relay_close(con, "failed to allocate log buffer");
		return;
	}

	if (rlay->rl_conf.flags & F_DIVERT) {
		slen = sizeof(con->se_out.ss);
		if (getsockname(s, (struct sockaddr *)&con->se_out.ss,
		    &slen) == -1) {
			relay_close(con, "peer lookup failed");
			return;
		}
		con->se_out.port = relay_socket_getport(&con->se_out.ss);

		/* Detect loop and fall back to the alternate forward target */
		if (bcmp(&rlay->rl_conf.ss, &con->se_out.ss,
		    sizeof(con->se_out.ss)) == 0 &&
		    con->se_out.port == rlay->rl_conf.port)
			con->se_out.ss.ss_family = AF_UNSPEC;
	} else if (rlay->rl_conf.flags & F_NATLOOK) {
		if ((cnl = calloc(1, sizeof(*cnl))) == NULL) {
			relay_close(con, "failed to allocate nat lookup");
			return;
		}

		con->se_cnl = cnl;
		bzero(cnl, sizeof(*cnl));
		cnl->in = -1;
		cnl->id = con->se_id;
		cnl->proc = proc_id;
		cnl->proto = IPPROTO_TCP;

		bcopy(&con->se_in.ss, &cnl->src, sizeof(cnl->src));
		slen = sizeof(cnl->dst);
		if (getsockname(s,
		    (struct sockaddr *)&cnl->dst, &slen) == -1) {
			relay_close(con, "failed to get local address");
			return;
		}

		proc_compose_imsg(env->sc_ps, PROC_PFE, -1, IMSG_NATLOOK, -1,
		    cnl, sizeof(*cnl));

		/* Schedule timeout */
		evtimer_set(&con->se_ev, relay_natlook, con);
		bcopy(&rlay->rl_conf.timeout, &tv, sizeof(tv));
		evtimer_add(&con->se_ev, &tv);
		return;
	}

	relay_session(con);
	return;
 err:
	if (s != -1) {
		close(s);
		if (con != NULL)
			free(con);
		/*
		 * the session struct was not completly set up, but still
		 * counted as an inflight session. account for this.
		 */
		relay_inflight--;
		log_debug("%s: inflight decremented, now %d",
		    __func__, relay_inflight);
	}
}

u_int32_t
relay_hash_addr(struct sockaddr_storage *ss, u_int32_t p)
{
	struct sockaddr_in	*sin4;
	struct sockaddr_in6	*sin6;

	if (ss->ss_family == AF_INET) {
		sin4 = (struct sockaddr_in *)ss;
		p = hash32_buf(&sin4->sin_addr,
		    sizeof(struct in_addr), p);
	} else {
		sin6 = (struct sockaddr_in6 *)ss;
		p = hash32_buf(&sin6->sin6_addr,
		    sizeof(struct in6_addr), p);
	}

	return (p);
}

int
relay_from_table(struct rsession *con)
{
	struct relay		*rlay = con->se_relay;
	struct host		*host;
	struct relay_table	*rlt = NULL;
	struct table		*table = NULL;
	u_int32_t		 p = con->se_hashkey;
	int			 idx = -1;

	/* the table is already selected */
	if (con->se_table != NULL) {
		rlt = con->se_table;
		table = rlt->rlt_table;
		if (table->conf.check && !table->up)
			table = NULL;
		goto gottable;
	}

	/* otherwise grep the first active table */
	TAILQ_FOREACH(rlt, &rlay->rl_tables, rlt_entry) {
		table = rlt->rlt_table;
		if ((rlt->rlt_flags & F_USED) == 0 ||
		    (table->conf.check && !table->up))
			table = NULL;
		else
			break;
	}

 gottable:
	if (table == NULL) {
		log_debug("%s: session %d: no active hosts",
		    __func__, con->se_id);
		return (-1);
	}
	if (!con->se_hashkeyset) {
		p = con->se_hashkey = rlt->rlt_key;
		con->se_hashkeyset = 1;
	}

	switch (rlt->rlt_mode) {
	case RELAY_DSTMODE_ROUNDROBIN:
		if ((int)rlt->rlt_key >= rlt->rlt_nhosts)
			rlt->rlt_key = 0;
		idx = (int)rlt->rlt_key;
		break;
	case RELAY_DSTMODE_RANDOM:
		idx = (int)arc4random_uniform(rlt->rlt_nhosts);
		break;
	case RELAY_DSTMODE_SRCHASH:
	case RELAY_DSTMODE_LOADBALANCE:
		/* Source IP address without port */
		p = relay_hash_addr(&con->se_in.ss, p);
		if (rlt->rlt_mode == RELAY_DSTMODE_SRCHASH)
			break;
		/* FALLTHROUGH */
	case RELAY_DSTMODE_HASH:
		/* Local "destination" IP address and port */
		p = relay_hash_addr(&rlay->rl_conf.ss, p);
		p = hash32_buf(&rlay->rl_conf.port,
		    sizeof(rlay->rl_conf.port), p);
		break;
	default:
		fatalx("relay_from_table: unsupported mode");
		/* NOTREACHED */
	}
	if (idx == -1 && (idx = p % rlt->rlt_nhosts) >= RELAY_MAXHOSTS)
		return (-1);
	host = rlt->rlt_host[idx];
	DPRINTF("%s: session %d: table %s host %s, p 0x%08x, idx %d",
	    __func__, con->se_id, table->conf.name, host->conf.name, p, idx);
	while (host != NULL) {
		DPRINTF("%s: session %d: host %s", __func__,
		    con->se_id, host->conf.name);
		if (!table->conf.check || host->up == HOST_UP)
			goto found;
		host = TAILQ_NEXT(host, entry);
	}
	TAILQ_FOREACH(host, &table->hosts, entry) {
		DPRINTF("%s: next host %s", __func__, host->conf.name);
		if (!table->conf.check || host->up == HOST_UP)
			goto found;
	}

	/* Should not happen */
	fatalx("relay_from_table: no active hosts, desynchronized");

 found:
	if (rlt->rlt_mode == RELAY_DSTMODE_ROUNDROBIN)
		rlt->rlt_key = host->idx + 1;
	con->se_retry = host->conf.retry;
	con->se_out.port = table->conf.port;
	bcopy(&host->conf.ss, &con->se_out.ss, sizeof(con->se_out.ss));

	return (0);
}

void
relay_natlook(int fd, short event, void *arg)
{
	struct rsession		*con = arg;
	struct relay		*rlay = con->se_relay;
	struct ctl_natlook	*cnl = con->se_cnl;

	if (cnl == NULL)
		fatalx("invalid NAT lookup");

	if (con->se_out.ss.ss_family == AF_UNSPEC && cnl->in == -1 &&
	    rlay->rl_conf.dstss.ss_family == AF_UNSPEC &&
	    TAILQ_EMPTY(&rlay->rl_tables)) {
		relay_close(con, "session NAT lookup failed");
		return;
	}
	if (cnl->in != -1) {
		bcopy(&cnl->rdst, &con->se_out.ss, sizeof(con->se_out.ss));
		con->se_out.port = cnl->rdport;
	}
	free(con->se_cnl);
	con->se_cnl = NULL;

	relay_session(con);
}

void
relay_session(struct rsession *con)
{
	struct relay		*rlay = con->se_relay;
	struct ctl_relay_event	*in = &con->se_in, *out = &con->se_out;

	if (bcmp(&rlay->rl_conf.ss, &out->ss, sizeof(out->ss)) == 0 &&
	    out->port == rlay->rl_conf.port) {
		log_debug("%s: session %d: looping", __func__, con->se_id);
		relay_close(con, "session aborted");
		return;
	}

	if (rlay->rl_conf.flags & F_UDP) {
		/*
		 * Call the UDP protocol-specific handler
		 */
		if (rlay->rl_proto->request == NULL)
			fatalx("invalide UDP session");
		if ((*rlay->rl_proto->request)(con) == -1)
			relay_close(con, "session failed");
		return;
	}

	if ((rlay->rl_conf.flags & F_SSL) && (in->ssl == NULL)) {
		relay_ssl_transaction(con, in);
		return;
	}

	if (!rlay->rl_proto->lateconnect) {
		if (rlay->rl_conf.fwdmode == FWD_TRANS)
			relay_bindanyreq(con, 0, IPPROTO_TCP);
		else if (relay_connect(con) == -1) {
			relay_close(con, "session failed");
			return;
		}
	}

	relay_input(con);
}

void
relay_bindanyreq(struct rsession *con, in_port_t port, int proto)
{
	struct relay		*rlay = con->se_relay;
	struct ctl_bindany	 bnd;
	struct timeval		 tv;

	bzero(&bnd, sizeof(bnd));
	bnd.bnd_id = con->se_id;
	bnd.bnd_proc = proc_id;
	bnd.bnd_port = port;
	bnd.bnd_proto = proto;
	bcopy(&con->se_in.ss, &bnd.bnd_ss, sizeof(bnd.bnd_ss));
	proc_compose_imsg(env->sc_ps, PROC_PARENT, -1, IMSG_BINDANY,
	    -1, &bnd, sizeof(bnd));

	/* Schedule timeout */
	evtimer_set(&con->se_ev, relay_bindany, con);
	bcopy(&rlay->rl_conf.timeout, &tv, sizeof(tv));
	evtimer_add(&con->se_ev, &tv);
}

void
relay_bindany(int fd, short event, void *arg)
{
	struct rsession	*con = arg;

	if (con->se_bnds == -1) {
		relay_close(con, "bindany failed, invalid socket");
		return;
	}
	if (relay_connect(con) == -1)
		relay_close(con, "session failed");
}

void
relay_connect_retry(int fd, short sig, void *arg)
{
	struct timeval	 evtpause = { 1, 0 };
	struct rsession	*con = arg;
	struct relay	*rlay = con->se_relay;
	int		 bnds = -1;

	if (relay_inflight < 1)
		fatalx("relay_connect_retry: no connection in flight");

	DPRINTF("%s: retry %d of %d, inflight: %d",__func__,
	    con->se_retrycount, con->se_retry, relay_inflight);

	if (sig != EV_TIMEOUT)
		fatalx("relay_connect_retry: called without timeout");

	evtimer_del(&con->se_inflightevt);

	/*
	 * XXX we might want to check if the inbound socket is still
	 * available: client could have closed it while we were waiting?
	 */

	DPRINTF("%s: got EV_TIMEOUT", __func__);

	if (getdtablecount() + FD_RESERVE +
	    relay_inflight > getdtablesize()) {
		if (con->se_retrycount < RELAY_OUTOF_FD_RETRIES) {
			evtimer_add(&con->se_inflightevt, &evtpause);
			return;
		}
		/* we waited for RELAY_OUTOF_FD_RETRIES seconds, give up */
		event_add(&rlay->rl_ev, NULL);
		relay_abort_http(con, 504, "connection timed out", 0);
		return;
	}

	if (rlay->rl_conf.fwdmode == FWD_TRANS) {
		/* con->se_bnds cannot be unset */
		bnds = con->se_bnds;
	}

 retry:
	if ((con->se_out.s = relay_socket_connect(&con->se_out.ss,
	    con->se_out.port, rlay->rl_proto, bnds)) == -1) {
		log_debug("%s: session %d: "
		    "forward failed: %s, %s", __func__,
		    con->se_id, strerror(errno),
		    con->se_retry ? "next retry" : "last retry");

		con->se_retrycount++;

		if ((errno == ENFILE || errno == EMFILE) &&
		    (con->se_retrycount < con->se_retry)) {
			event_del(&rlay->rl_ev);
			evtimer_add(&con->se_inflightevt, &evtpause);
			evtimer_add(&rlay->rl_evt, &evtpause);
			return;
		} else if (con->se_retrycount < con->se_retry)
			goto retry;
		event_add(&rlay->rl_ev, NULL);
		relay_abort_http(con, 504, "connect failed", 0);
		return;
	}

	relay_inflight--;
	DPRINTF("%s: inflight decremented, now %d",__func__, relay_inflight);

	event_add(&rlay->rl_ev, NULL);

	if (errno == EINPROGRESS)
		event_again(&con->se_ev, con->se_out.s, EV_WRITE|EV_TIMEOUT,
		    relay_connected, &con->se_tv_start, &rlay->rl_conf.timeout,
		    con);
	else
		relay_connected(con->se_out.s, EV_WRITE, con);

	return;
}

int
relay_connect(struct rsession *con)
{
	struct relay	*rlay = con->se_relay;
	struct timeval	 evtpause = { 1, 0 };
	int		 bnds = -1, ret;

	if (relay_inflight < 1)
		fatalx("relay_connect: no connection in flight");

	if (gettimeofday(&con->se_tv_start, NULL) == -1)
		return (-1);

	if (!TAILQ_EMPTY(&rlay->rl_tables)) {
		if (relay_from_table(con) != 0)
			return (-1);
	} else if (con->se_out.ss.ss_family == AF_UNSPEC) {
		bcopy(&rlay->rl_conf.dstss, &con->se_out.ss,
		    sizeof(con->se_out.ss));
		con->se_out.port = rlay->rl_conf.dstport;
	}

	if (rlay->rl_conf.fwdmode == FWD_TRANS) {
		if (con->se_bnds == -1) {
			log_debug("%s: could not bind any sock", __func__);
			return (-1);
		}
		bnds = con->se_bnds;
	}

	/* Do the IPv4-to-IPv6 or IPv6-to-IPv4 translation if requested */
	if (rlay->rl_conf.dstaf.ss_family != AF_UNSPEC) {
		if (con->se_out.ss.ss_family == AF_INET &&
		    rlay->rl_conf.dstaf.ss_family == AF_INET6)
			ret = map4to6(&con->se_out.ss, &rlay->rl_conf.dstaf);
		else if (con->se_out.ss.ss_family == AF_INET6 &&
		    rlay->rl_conf.dstaf.ss_family == AF_INET)
			ret = map6to4(&con->se_out.ss);
		else
			ret = 0;
		if (ret != 0) {
			log_debug("%s: mapped to invalid address", __func__);
			return (-1);
		}
	}

 retry:
	if ((con->se_out.s = relay_socket_connect(&con->se_out.ss,
	    con->se_out.port, rlay->rl_proto, bnds)) == -1) {
		if (errno == ENFILE || errno == EMFILE) {
			log_debug("%s: session %d: forward failed: %s",
			    __func__, con->se_id, strerror(errno));
			evtimer_set(&con->se_inflightevt, relay_connect_retry,
			    con);
			event_del(&rlay->rl_ev);
			evtimer_add(&con->se_inflightevt, &evtpause);
			evtimer_add(&rlay->rl_evt, &evtpause);
			return (0);
		} else {
			if (con->se_retry) {
				con->se_retry--;
				log_debug("%s: session %d: "
				    "forward failed: %s, %s", __func__,
				    con->se_id, strerror(errno),
				    con->se_retry ?
				    "next retry" : "last retry");
				goto retry;
			}
			log_debug("%s: session %d: forward failed: %s",
			    __func__, con->se_id, strerror(errno));
			return (-1);
		}
	}

	relay_inflight--;
	DPRINTF("%s: inflight decremented, now %d",__func__,
	    relay_inflight);

	if (errno == EINPROGRESS)
		event_again(&con->se_ev, con->se_out.s, EV_WRITE|EV_TIMEOUT,
		    relay_connected, &con->se_tv_start, &rlay->rl_conf.timeout,
		    con);
	else
		relay_connected(con->se_out.s, EV_WRITE, con);

	return (0);
}

void
relay_close(struct rsession *con, const char *msg)
{
	char		 ibuf[128], obuf[128], *ptr = NULL;
	struct relay	*rlay = con->se_relay;

	SPLAY_REMOVE(session_tree, &rlay->rl_sessions, con);

	event_del(&con->se_ev);
	if (con->se_in.bev != NULL)
		bufferevent_disable(con->se_in.bev, EV_READ|EV_WRITE);
	if (con->se_out.bev != NULL)
		bufferevent_disable(con->se_out.bev, EV_READ|EV_WRITE);

	if ((env->sc_opts & RELAYD_OPT_LOGUPDATE) && msg != NULL) {
		bzero(&ibuf, sizeof(ibuf));
		bzero(&obuf, sizeof(obuf));
		(void)print_host(&con->se_in.ss, ibuf, sizeof(ibuf));
		(void)print_host(&con->se_out.ss, obuf, sizeof(obuf));
		if (EVBUFFER_LENGTH(con->se_log) &&
		    evbuffer_add_printf(con->se_log, "\r\n") != -1)
			ptr = evbuffer_readline(con->se_log);
		log_info("relay %s, "
		    "session %d (%d active), %d, %s -> %s:%d, "
		    "%s%s%s", rlay->rl_conf.name, con->se_id, relay_sessions,
		    con->se_mark, ibuf, obuf, ntohs(con->se_out.port), msg,
		    ptr == NULL ? "" : ",", ptr == NULL ? "" : ptr);
		if (ptr != NULL)
			free(ptr);
	}

	if (con->se_priv != NULL)
		free(con->se_priv);
	if (con->se_in.bev != NULL)
		bufferevent_free(con->se_in.bev);
	else if (con->se_in.output != NULL)
		evbuffer_free(con->se_in.output);
	if (con->se_in.ssl != NULL) {
		/* XXX handle non-blocking shutdown */
		if (SSL_shutdown(con->se_in.ssl) == 0)
			SSL_shutdown(con->se_in.ssl);
		SSL_free(con->se_in.ssl);
	}
	if (con->se_in.s != -1) {
		close(con->se_in.s);
		if (con->se_out.s == -1) {
			/*
			 * the output was never connected,
			 * thus this was an inflight session.
			 */
			relay_inflight--;
			log_debug("%s: sessions inflight decremented, now %d",
			    __func__, relay_inflight);
		}
	}
	if (con->se_in.path != NULL)
		free(con->se_in.path);
	if (con->se_in.buf != NULL)
		free(con->se_in.buf);
	if (con->se_in.nodes != NULL)
		free(con->se_in.nodes);

	if (con->se_out.bev != NULL)
		bufferevent_free(con->se_out.bev);
	else if (con->se_out.output != NULL)
		evbuffer_free(con->se_out.output);
	if (con->se_out.ssl != NULL) {
		/* XXX handle non-blocking shutdown */
		if (SSL_shutdown(con->se_out.ssl) == 0)
			SSL_shutdown(con->se_out.ssl);
		SSL_free(con->se_out.ssl);
	}
	if (con->se_out.s != -1) {
		close(con->se_out.s);

		/* Some file descriptors are available again. */
		if (evtimer_pending(&rlay->rl_evt, NULL)) {
			evtimer_del(&rlay->rl_evt);
			event_add(&rlay->rl_ev, NULL);
		}
	}

	if (con->se_out.path != NULL)
		free(con->se_out.path);
	if (con->se_out.buf != NULL)
		free(con->se_out.buf);
	if (con->se_out.nodes != NULL)
		free(con->se_out.nodes);

	if (con->se_log != NULL)
		evbuffer_free(con->se_log);

	if (con->se_cnl != NULL) {
#if 0
		proc_compose_imsg(env->sc_ps, PROC_PFE, -1, IMSG_KILLSTATES, -1,
		    cnl, sizeof(*cnl));
#endif
		free(con->se_cnl);
	}

	free(con);
	relay_sessions--;
}

int
relay_dispatch_pfe(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct relay		*rlay;
	struct rsession		*con, se;
	struct ctl_natlook	 cnl;
	struct timeval		 tv;
	struct host		*host;
	struct table		*table;
	struct ctl_status	 st;
	objid_t			 id;
	int			 cid;

	switch (imsg->hdr.type) {
	case IMSG_HOST_DISABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((host = host_find(env, id)) == NULL)
			fatalx("relay_dispatch_pfe: desynchronized");
		if ((table = table_find(env, host->conf.tableid)) ==
		    NULL)
			fatalx("relay_dispatch_pfe: invalid table id");
		if (host->up == HOST_UP)
			table->up--;
		host->flags |= F_DISABLE;
		host->up = HOST_UNKNOWN;
		break;
	case IMSG_HOST_ENABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((host = host_find(env, id)) == NULL)
			fatalx("relay_dispatch_pfe: desynchronized");
		host->flags &= ~(F_DISABLE);
		host->up = HOST_UNKNOWN;
		break;
	case IMSG_TABLE_DISABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((table = table_find(env, id)) == NULL)
			fatalx("relay_dispatch_pfe: desynchronized");
		table->conf.flags |= F_DISABLE;
		table->up = 0;
		TAILQ_FOREACH(host, &table->hosts, entry)
			host->up = HOST_UNKNOWN;
		break;
	case IMSG_TABLE_ENABLE:
		memcpy(&id, imsg->data, sizeof(id));
		if ((table = table_find(env, id)) == NULL)
			fatalx("relay_dispatch_pfe: desynchronized");
		table->conf.flags &= ~(F_DISABLE);
		table->up = 0;
		TAILQ_FOREACH(host, &table->hosts, entry)
			host->up = HOST_UNKNOWN;
		break;
	case IMSG_HOST_STATUS:
		IMSG_SIZE_CHECK(imsg, &st);
		memcpy(&st, imsg->data, sizeof(st));
		if ((host = host_find(env, st.id)) == NULL)
			fatalx("relay_dispatch_pfe: invalid host id");
		if (host->flags & F_DISABLE)
			break;
		if (host->up == st.up) {
			log_debug("%s: host %d => %d", __func__,
			    host->conf.id, host->up);
			fatalx("relay_dispatch_pfe: desynchronized");
		}

		if ((table = table_find(env, host->conf.tableid))
		    == NULL)
			fatalx("relay_dispatch_pfe: invalid table id");

		DPRINTF("%s: [%d] state %d for "
		    "host %u %s", __func__, proc_id, st.up,
		    host->conf.id, host->conf.name);

		if ((st.up == HOST_UNKNOWN && host->up == HOST_DOWN) ||
		    (st.up == HOST_DOWN && host->up == HOST_UNKNOWN)) {
			host->up = st.up;
			break;
		}
		if (st.up == HOST_UP)
			table->up++;
		else
			table->up--;
		host->up = st.up;
		break;
	case IMSG_NATLOOK:
		bcopy(imsg->data, &cnl, sizeof(cnl));
		if ((con = session_find(env, cnl.id)) == NULL ||
		    con->se_cnl == NULL) {
			log_debug("%s: session %d: expired",
			    __func__, cnl.id);
			break;
		}
		bcopy(&cnl, con->se_cnl, sizeof(*con->se_cnl));
		evtimer_del(&con->se_ev);
		evtimer_set(&con->se_ev, relay_natlook, con);
		bzero(&tv, sizeof(tv));
		evtimer_add(&con->se_ev, &tv);
		break;
	case IMSG_CTL_SESSION:
		IMSG_SIZE_CHECK(imsg, &cid);
		memcpy(&cid, imsg->data, sizeof(cid));
		TAILQ_FOREACH(rlay, env->sc_relays, rl_entry) {
			SPLAY_FOREACH(con, session_tree,
			    &rlay->rl_sessions) {
				memcpy(&se, con, sizeof(se));
				se.se_cid = cid;
				proc_compose_imsg(env->sc_ps, p->p_id, -1,
				    IMSG_CTL_SESSION,
				    -1, &se, sizeof(se));
			}
		}
		proc_compose_imsg(env->sc_ps, p->p_id, -1, IMSG_CTL_END,
		    -1, &cid, sizeof(cid));
		break;
	default:
		return (-1);
	}

	return (0);
}

int
relay_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct rsession		*con;
	struct timeval		 tv;
	objid_t			 id;

	switch (imsg->hdr.type) {
	case IMSG_BINDANY:
		bcopy(imsg->data, &id, sizeof(id));
		if ((con = session_find(env, id)) == NULL) {
			log_debug("%s: session %d: expired",
			    __func__, id);
			break;
		}

		/* Will validate the result later */
		con->se_bnds = imsg->fd;

		evtimer_del(&con->se_ev);
		evtimer_set(&con->se_ev, relay_bindany, con);
		bzero(&tv, sizeof(tv));
		evtimer_add(&con->se_ev, &tv);
		break;
	case IMSG_CFG_TABLE:
		config_gettable(env, imsg);
		break;
	case IMSG_CFG_HOST:
		config_gethost(env, imsg);
		break;
	case IMSG_CFG_PROTO:
		config_getproto(env, imsg);
		break;
	case IMSG_CFG_PROTONODE:
		return (config_getprotonode(env, imsg));
	case IMSG_CFG_RELAY:
		config_getrelay(env, imsg);
		break;
	case IMSG_CFG_RELAY_TABLE:
		config_getrelaytable(env, imsg);
		break;
	case IMSG_CFG_DONE:
		config_getcfg(env, imsg);
		break;
	case IMSG_CTL_START:
		relay_launch();
		break;
	case IMSG_CTL_RESET:
		config_getreset(env, imsg);
		break;
	default:
		return (-1);
	}

	return (0);
}

SSL_CTX *
relay_ssl_ctx_create(struct relay *rlay)
{
	struct protocol *proto = rlay->rl_proto;
	SSL_CTX *ctx;

	ctx = SSL_CTX_new(SSLv23_method());
	if (ctx == NULL)
		goto err;

	/* Modify session timeout and cache size*/
	SSL_CTX_set_timeout(ctx, rlay->rl_conf.timeout.tv_sec);
	if (proto->cache < -1) {
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);
	} else if (proto->cache >= -1) {
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
		if (proto->cache >= 0)
			SSL_CTX_sess_set_cache_size(ctx, proto->cache);
	}

	/* Enable all workarounds and set SSL options */
	SSL_CTX_set_options(ctx, SSL_OP_ALL);
	SSL_CTX_set_options(ctx,
	    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);

	/* Set the allowed SSL protocols */
	if ((proto->sslflags & SSLFLAG_SSLV2) == 0)
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	if ((proto->sslflags & SSLFLAG_SSLV3) == 0)
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	if ((proto->sslflags & SSLFLAG_TLSV1) == 0)
		SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);

	if (!SSL_CTX_set_cipher_list(ctx, proto->sslciphers))
		goto err;

	/* Verify the server certificate if we have a CA chain */
	if ((rlay->rl_conf.flags & F_SSLCLIENT) &&
	    (rlay->rl_ssl_ca != NULL)) {
		if (!ssl_ctx_load_verify_memory(ctx,
		    rlay->rl_ssl_ca, rlay->rl_conf.ssl_ca_len))
			goto err;
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	}

	if ((rlay->rl_conf.flags & F_SSL) == 0)
		return (ctx);

	log_debug("%s: loading certificate", __func__);
	if (!ssl_ctx_use_certificate_chain(ctx,
	    rlay->rl_ssl_cert, rlay->rl_conf.ssl_cert_len))
		goto err;

	log_debug("%s: loading private key", __func__);
	if (!ssl_ctx_use_private_key(ctx, rlay->rl_ssl_key,
	    rlay->rl_conf.ssl_key_len))
		goto err;
	if (!SSL_CTX_check_private_key(ctx))
		goto err;

	/* Set session context to the local relay name */
	if (!SSL_CTX_set_session_id_context(ctx, rlay->rl_conf.name,
	    strlen(rlay->rl_conf.name)))
		goto err;

	return (ctx);

 err:
	if (ctx != NULL)
		SSL_CTX_free(ctx);
	ssl_error(rlay->rl_conf.name, "relay_ssl_ctx_create");
	return (NULL);
}

void
relay_ssl_transaction(struct rsession *con, struct ctl_relay_event *cre)
{
	struct relay		*rlay = con->se_relay;
	SSL			*ssl;
	const SSL_METHOD	*method;
	void			(*cb)(int, short, void *);
	u_int			 flag;

	ssl = SSL_new(rlay->rl_ssl_ctx);
	if (ssl == NULL)
		goto err;

	if (cre->dir == RELAY_DIR_REQUEST) {
		cb = relay_ssl_accept;
		method = SSLv23_server_method();
		flag = EV_READ;
	} else {
		cb = relay_ssl_connect;
		method = SSLv23_client_method();
		flag = EV_WRITE;
	}

	if (!SSL_set_ssl_method(ssl, method))
		goto err;
	if (!SSL_set_fd(ssl, cre->s))
		goto err;

	if (cre->dir == RELAY_DIR_REQUEST)
		SSL_set_accept_state(ssl);
	else
		SSL_set_connect_state(ssl);

	cre->ssl = ssl;

	DPRINTF("%s: session %d: scheduling on %s", __func__, con->se_id,
	    (flag == EV_READ) ? "EV_READ" : "EV_WRITE");
	event_again(&con->se_ev, cre->s, EV_TIMEOUT|flag, cb,
	    &con->se_tv_start, &rlay->rl_conf.timeout, con);
	return;

 err:
	if (ssl != NULL)
		SSL_free(ssl);
	ssl_error(rlay->rl_conf.name, "relay_ssl_transaction");
	relay_close(con, "session ssl failed");
}

void
relay_ssl_accept(int fd, short event, void *arg)
{
	struct rsession	*con = arg;
	struct relay	*rlay = con->se_relay;
	int		 retry_flag = 0;
	int		 ssl_err = 0;
	int		 ret;

	if (event == EV_TIMEOUT) {
		relay_close(con, "SSL accept timeout");
		return;
	}

	ret = SSL_accept(con->se_in.ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(con->se_in.ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			retry_flag = EV_READ;
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			retry_flag = EV_WRITE;
			goto retry;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
			if (ret == 0) {
				relay_close(con, "closed");
				return;
			}
			/* FALLTHROUGH */
		default:
			ssl_error(rlay->rl_conf.name, "relay_ssl_accept");
			relay_close(con, "SSL accept error");
			return;
		}
	}


#ifdef DEBUG
	log_info(
#else
	log_debug(
#endif
	    "relay %s, session %d established (%d active)",
	    rlay->rl_conf.name, con->se_id, relay_sessions);

	relay_session(con);
	return;

retry:
	DPRINTF("%s: session %d: scheduling on %s", __func__, con->se_id,
	    (retry_flag == EV_READ) ? "EV_READ" : "EV_WRITE");
	event_again(&con->se_ev, fd, EV_TIMEOUT|retry_flag, relay_ssl_accept,
	    &con->se_tv_start, &rlay->rl_conf.timeout, con);
}

void
relay_ssl_connect(int fd, short event, void *arg)
{
	struct rsession	*con = arg;
	struct relay	*rlay = con->se_relay;
	int		 retry_flag = 0;
	int		 ssl_err = 0;
	int		 ret;

	if (event == EV_TIMEOUT) {
		relay_close(con, "SSL connect timeout");
		return;
	}

	ret = SSL_connect(con->se_out.ssl);
	if (ret <= 0) {
		ssl_err = SSL_get_error(con->se_out.ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			retry_flag = EV_READ;
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			retry_flag = EV_WRITE;
			goto retry;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
			if (ret == 0) {
				relay_close(con, "closed");
				return;
			}
			/* FALLTHROUGH */
		default:
			ssl_error(rlay->rl_conf.name, "relay_ssl_connect");
			relay_close(con, "SSL connect error");
			return;
		}
	}

#ifdef DEBUG
	log_info(
#else
	log_debug(
#endif
	    "relay %s, session %d connected (%d active)",
	    rlay->rl_conf.name, con->se_id, relay_sessions);

	relay_connected(fd, EV_WRITE, con);
	return;

retry:
	DPRINTF("%s: session %d: scheduling on %s", __func__, con->se_id,
	    (retry_flag == EV_READ) ? "EV_READ" : "EV_WRITE");
	event_again(&con->se_ev, fd, EV_TIMEOUT|retry_flag, relay_ssl_connect,
	    &con->se_tv_start, &rlay->rl_conf.timeout, con);
}

void
relay_ssl_connected(struct ctl_relay_event *cre)
{
	/*
	 * Hack libevent - we overwrite the internal bufferevent I/O
	 * functions to handle the SSL abstraction.
	 */
	event_set(&cre->bev->ev_read, cre->s, EV_READ,
	    relay_ssl_readcb, cre->bev);
	event_set(&cre->bev->ev_write, cre->s, EV_WRITE,
	    relay_ssl_writecb, cre->bev);
}

void
relay_ssl_readcb(int fd, short event, void *arg)
{
	char rbuf[IBUF_READ_SIZE];
	struct bufferevent *bufev = arg;
	struct ctl_relay_event *cre = bufev->cbarg;
	struct rsession *con = cre->con;
	struct relay *rlay = con->se_relay;
	int ret = 0, ssl_err = 0;
	short what = EVBUFFER_READ;
	int howmuch = IBUF_READ_SIZE;
	size_t len;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (bufev->wm_read.high != 0)
		howmuch = MIN(sizeof(rbuf), bufev->wm_read.high);

	ret = SSL_read(cre->ssl, rbuf, howmuch);
	if (ret <= 0) {
		ssl_err = SSL_get_error(cre->ssl, ret);

		switch (ssl_err) {
		case SSL_ERROR_WANT_READ:
			DPRINTF("%s: session %d: want read",
			    __func__, con->se_id);
			goto retry;
		case SSL_ERROR_WANT_WRITE:
			DPRINTF("%s: session %d: want write",
			    __func__, con->se_id);
			goto retry;
		default:
			if (ret == 0)
				what |= EVBUFFER_EOF;
			else {
				ssl_error(rlay->rl_conf.name,
				    "relay_ssl_readcb");
				what |= EVBUFFER_ERROR;
			}
			goto err;
		}
	}

	if (evbuffer_add(bufev->input, rbuf, ret) == -1) {
		what |= EVBUFFER_ERROR;
		goto err;
	}

	relay_bufferevent_add(&bufev->ev_read, bufev->timeout_read);

	len = EVBUFFER_LENGTH(bufev->input);
	if (bufev->wm_read.low != 0 && len < bufev->wm_read.low)
		return;
	if (bufev->wm_read.high != 0 && len > bufev->wm_read.high) {
		struct evbuffer *buf = bufev->input;
		event_del(&bufev->ev_read);
		evbuffer_setcb(buf, bufferevent_read_pressure_cb, bufev);
		return;
	}

	if (bufev->readcb != NULL)
		(*bufev->readcb)(bufev, bufev->cbarg);
	return;

 retry:
	relay_bufferevent_add(&bufev->ev_read, bufev->timeout_read);
	return;

 err:
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

void
relay_ssl_writecb(int fd, short event, void *arg)
{
	struct bufferevent *bufev = arg;
	struct ctl_relay_event *cre = bufev->cbarg;
	struct rsession *con = cre->con;
	struct relay *rlay = con->se_relay;
	int ret = 0, ssl_err;
	short what = EVBUFFER_WRITE;

	if (event == EV_TIMEOUT) {
		what |= EVBUFFER_TIMEOUT;
		goto err;
	}

	if (EVBUFFER_LENGTH(bufev->output)) {
		if (cre->buf == NULL) {
			cre->buflen = EVBUFFER_LENGTH(bufev->output);
			if ((cre->buf = malloc(cre->buflen)) == NULL) {
				what |= EVBUFFER_ERROR;
				goto err;
			}
			bcopy(EVBUFFER_DATA(bufev->output),
			    cre->buf, cre->buflen);
		}

		ret = SSL_write(cre->ssl, cre->buf, cre->buflen);
		if (ret <= 0) {
			ssl_err = SSL_get_error(cre->ssl, ret);

			switch (ssl_err) {
			case SSL_ERROR_WANT_READ:
				DPRINTF("%s: session %d: want read",
				    __func__, con->se_id);
				goto retry;
			case SSL_ERROR_WANT_WRITE:
				DPRINTF("%s: session %d: want write",
				    __func__, con->se_id);
				goto retry;
			default:
				if (ret == 0)
					what |= EVBUFFER_EOF;
				else {
					ssl_error(rlay->rl_conf.name,
					    "relay_ssl_writecb");
					what |= EVBUFFER_ERROR;
				}
				goto err;
			}
		}
		evbuffer_drain(bufev->output, ret);
	}
	if (cre->buf != NULL) {
		free(cre->buf);
		cre->buf = NULL;
		cre->buflen = 0;
	}

	if (EVBUFFER_LENGTH(bufev->output) != 0)
		relay_bufferevent_add(&bufev->ev_write, bufev->timeout_write);

	if (bufev->writecb != NULL &&
	    EVBUFFER_LENGTH(bufev->output) <= bufev->wm_write.low)
		(*bufev->writecb)(bufev, bufev->cbarg);
	return;

 retry:
	if (cre->buflen != 0)
		relay_bufferevent_add(&bufev->ev_write, bufev->timeout_write);
	return;

 err:
	if (cre->buf != NULL) {
		free(cre->buf);
		cre->buf = NULL;
		cre->buflen = 0;
	}
	(*bufev->errorcb)(bufev, what, bufev->cbarg);
}

int
relay_bufferevent_add(struct event *ev, int timeout)
{
	struct timeval tv, *ptv = NULL;

	if (timeout) {
		timerclear(&tv);
		tv.tv_sec = timeout;
		ptv = &tv;
	}

	return (event_add(ev, ptv));
}

#ifdef notyet
int
relay_bufferevent_printf(struct ctl_relay_event *cre, const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = evbuffer_add_vprintf(cre->output, fmt, ap);
	va_end(ap);

	if (cre->bev != NULL &&
	    ret != -1 && EVBUFFER_LENGTH(cre->output) > 0 &&
	    (cre->bev->enabled & EV_WRITE))
		bufferevent_enable(cre->bev, EV_WRITE);

	return (ret);
}
#endif

int
relay_bufferevent_print(struct ctl_relay_event *cre, char *str)
{
	if (cre->bev == NULL)
		return (evbuffer_add(cre->output, str, strlen(str)));
	return (bufferevent_write(cre->bev, str, strlen(str)));
}

int
relay_bufferevent_write_buffer(struct ctl_relay_event *cre,
    struct evbuffer *buf)
{
	if (cre->bev == NULL)
		return (evbuffer_add_buffer(cre->output, buf));
	return (bufferevent_write_buffer(cre->bev, buf));
}

int
relay_bufferevent_write_chunk(struct ctl_relay_event *cre,
    struct evbuffer *buf, size_t size)
{
	int ret;
	ret = relay_bufferevent_write(cre, buf->buffer, size);
	if (ret != -1)
		evbuffer_drain(buf, size);
	return (ret);
}

int
relay_bufferevent_write(struct ctl_relay_event *cre, void *data, size_t size)
{
	if (cre->bev == NULL)
		return (evbuffer_add(cre->output, data, size));
	return (bufferevent_write(cre->bev, data, size));
}

int
relay_cmp_af(struct sockaddr_storage *a, struct sockaddr_storage *b)
{
	int ret = -1;
	struct sockaddr_in ia, ib;
	struct sockaddr_in6 ia6, ib6;

	switch (a->ss_family) {
	case AF_INET:
		bcopy(a, &ia, sizeof(struct sockaddr_in));
		bcopy(b, &ib, sizeof(struct sockaddr_in));

		ret = memcmp(&ia.sin_addr, &ib.sin_addr,
		    sizeof(ia.sin_addr));
		if (ret == 0)
			ret = memcmp(&ia.sin_port, &ib.sin_port,
			    sizeof(ia.sin_port));
		break;
	case AF_INET6:
		bcopy(a, &ia6, sizeof(struct sockaddr_in6));
		bcopy(b, &ib6, sizeof(struct sockaddr_in6));

		ret = memcmp(&ia6.sin6_addr, &ib6.sin6_addr,
		    sizeof(ia6.sin6_addr));
		if (ret == 0)
			ret = memcmp(&ia6.sin6_port, &ib6.sin6_port,
			    sizeof(ia6.sin6_port));
		break;
	default:
		break;
	}

	return (ret);
}

char *
relay_load_file(const char *name, off_t *len)
{
	struct stat	 st;
	off_t		 size;
	u_int8_t	*buf = NULL;
	int		 fd;

	if ((fd = open(name, O_RDONLY)) == -1)
		return (NULL);
	if (fstat(fd, &st) != 0)
		goto fail;
	size = st.st_size;
	if ((buf = calloc(1, size + 1)) == NULL)
		goto fail;
	if (read(fd, buf, size) != size)
		goto fail;

	close(fd);

	*len = size;
	return (buf);

 fail:
	if (buf != NULL)
		free(buf);
	close(fd);
	return (NULL);
}

int
relay_load_certfiles(struct relay *rlay)
{
	char	 certfile[PATH_MAX];
	char	 hbuf[sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")];
	struct protocol *proto = rlay->rl_proto;
	int	 useport = htons(rlay->rl_conf.port);

	if ((rlay->rl_conf.flags & F_SSLCLIENT) && strlen(proto->sslca)) {
		if ((rlay->rl_ssl_ca = relay_load_file(proto->sslca,
		    &rlay->rl_conf.ssl_ca_len)) == NULL)
			return (-1);
		log_debug("%s: using ca %s", __func__, proto->sslca);
	}

	if ((rlay->rl_conf.flags & F_SSL) == 0)
		return (0);

	if (print_host(&rlay->rl_conf.ss, hbuf, sizeof(hbuf)) == NULL)
		return (-1);

	if (snprintf(certfile, sizeof(certfile),
	    "/etc/ssl/%s:%u.crt", hbuf, useport) == -1)
		return (-1);
	if ((rlay->rl_ssl_cert = relay_load_file(certfile,
	    &rlay->rl_conf.ssl_cert_len)) == NULL) {
		if (snprintf(certfile, sizeof(certfile),
		    "/etc/ssl/%s.crt", hbuf) == -1)
			return (-1);
		if ((rlay->rl_ssl_cert = relay_load_file(certfile,
		    &rlay->rl_conf.ssl_cert_len)) == NULL)
			return (-1);
		useport = 0;
	}
	log_debug("%s: using certificate %s", __func__, certfile);

	if (useport) {
		if (snprintf(certfile, sizeof(certfile),
		    "/etc/ssl/private/%s:%u.key", hbuf, useport) == -1)
			return -1;
	} else {
		if (snprintf(certfile, sizeof(certfile),
		    "/etc/ssl/private/%s.key", hbuf) == -1)
			return -1;
	}
	if ((rlay->rl_ssl_key = relay_load_file(certfile,
	    &rlay->rl_conf.ssl_key_len)) == NULL)
		return (-1);
	log_debug("%s: using private key %s", __func__, certfile);

	return (0);
}

static __inline int
relay_proto_cmp(struct protonode *a, struct protonode *b)
{
	int ret;
	ret = strcasecmp(a->key, b->key);
	if (ret == 0)
		ret = (int)a->type - b->type;
	return (ret);
}

int
relay_session_cmp(struct rsession *a, struct rsession *b)
{
	struct relay	*rlay = b->se_relay;
	struct protocol	*proto = rlay->rl_proto;

	if (proto != NULL && proto->cmp != NULL)
		return ((*proto->cmp)(a, b));

	return ((int)a->se_id - b->se_id);
}

RB_GENERATE(proto_tree, protonode, nodes, relay_proto_cmp);
SPLAY_GENERATE(session_tree, rsession, se_nodes, relay_session_cmp);
