/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcpip.h>
#include <net/if.h>
#include <netinet/ip_fil.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ipf.h"
#include "ipt.h"

#ifndef	lint
static	char	sccsid[] = "@(#)ipft_tx.c	1.2 10/17/95 (C) 1993 Darren Reed";
#endif

extern	int	opts;

static	int	text_open(), text_close(), text_readip(), parseline();

struct	ipread	iptext = { text_open, text_close, text_readip };
static	FILE	*tfp = NULL;
static	int	tfd = -1;

static	int	text_open(fname)
char	*fname;
{
	if (tfp && tfd != -1) {
		rewind(tfp);
		return tfd;
	}

	if (!strcmp(fname, "-")) {
		tfd = 0;
		tfp = stdin;
	} else {
		tfd = open(fname, O_RDONLY);
		if (tfd != -1)
			tfp = fdopen(tfd, "r");
	}
	return tfd;
}


static	int	text_close()
{
	int	cfd = tfd;

	tfd = -1;
	return close(cfd);
}


static	int	text_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	register char *s;
	struct	ip *ip;
	char	line[513];

 	ip = (struct ip *)buf;
	*ifn = NULL;
	while (fgets(line, sizeof(line)-1, tfp)) {
		if ((s = index(line, '\n')))
			*s = '\0';
		if ((s = index(line, '\r')))
			*s = '\0';
		if ((s = index(line, '#')))
			*s = '\0';
		if (!*line)
			continue;
		if (!(opts & OPT_BRIEF))
			printf("input: %s\n", line);
		*ifn = NULL;
		*dir = 0;
		if (!parseline(line, buf, ifn, dir))
#if 0
			return sizeof(struct tcpiphdr);
#else
			return sizeof(struct ip);
#endif
	}
	return -1;
}

static	int	parseline(line, ip, ifn, out)
char	*line;
struct	ip	*ip;
char	**ifn;
int	*out;
{
	extern	char	*proto;
	tcphdr_t	th, *tcp = &th;
	struct	icmp	icmp, *ic = &icmp;
	char	*cps[20], **cpp, c, opts[68];
	int	i;

	bzero((char *)ip, MAX(sizeof(*tcp), sizeof(*ic)) + sizeof(*ip));
	bzero((char *)tcp, sizeof(*tcp));
	bzero((char *)ic, sizeof(*ic));
	bzero(opts, sizeof(opts));
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	for (i = 0, cps[0] = strtok(line, " \b\t\r\n"); cps[i] && i < 19; )
		cps[++i] = strtok(NULL, " \b\t\r\n");
	if (i < 2)
		return 1;

	cpp = cps;

	c = **cpp;
	if (!isalpha(c) || (tolower(c) != 'o' && tolower(c) != 'i')) {
		fprintf(stderr, "bad direction \"%s\"\n", *cpp);
		return 1;
	}
	*out = (tolower(c) == 'o') ? 1 : 0;
	cpp++;

	if (!strcasecmp(*cpp, "on")) {
		cpp++;
		if (!*cpp)
			return 1;
		*ifn = *cpp++;
	}

	c = **cpp;
	ip->ip_len = sizeof(struct ip);
	if (!strcasecmp(*cpp, "tcp") || !strcasecmp(*cpp, "udp") ||
	    !strcasecmp(*cpp, "icmp")) {
		if (c == 't') {
			ip->ip_p = IPPROTO_TCP;
			ip->ip_len += sizeof(struct tcphdr);
			proto = "tcp";
		} else if (c == 'u') {
			ip->ip_p = IPPROTO_UDP;
			ip->ip_len += sizeof(struct udphdr);
			proto = "udp";
		} else {
			ip->ip_p = IPPROTO_ICMP;
			ip->ip_len += sizeof(struct icmp);
			proto = "icmp";
		}
		cpp++;
	} else
		ip->ip_p = IPPROTO_IP;

	if (!*cpp)
		return 1;
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		char	*last;

		last = index(*cpp, ',');
		if (!last) {
			fprintf(stderr, "tcp/udp with no source port\n");
			return 1;
		}
		*last++ = '\0';
		tcp->th_sport = portnum(last);
	}
	ip->ip_src.s_addr = hostnum(*cpp);
	cpp++;
	if (!*cpp)
		return 1;

	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP) {
		char	*last;

		last = index(*cpp, ',');
		if (!last) {
			fprintf(stderr, "tcp/udp with no destination port\n");
			return 1;
		}
		*last++ = '\0';
		tcp->th_dport = portnum(last);
	}
	ip->ip_dst.s_addr = hostnum(*cpp);
	cpp++;
	if (*cpp && ip->ip_p == IPPROTO_TCP) {
		extern	char	flagset[];
		extern	u_char	flags[];
		char	*s, *t;

		for (s = *cpp; *s; s++)
			if ((t  = index(flagset, *s)))
				tcp->th_flags |= flags[t - flagset];
		if (tcp->th_flags)
			cpp++;
		assert(tcp->th_flags != 0);
	} else if (*cpp && ip->ip_p == IPPROTO_ICMP) {
		extern	char	*icmptypes[];
		char	**s, *t;
		int	i;

		for (s = icmptypes, i = 0; !*s || strcmp(*s, "END"); s++, i++)
			if (*s && !strncasecmp(*cpp, *s, strlen(*s))) {
				ic->icmp_type = i;
				if ((t = index(*cpp, ',')))
					ic->icmp_code = atoi(t+1);
				cpp++;
				break;
			}
	}

	if (*cpp && !strcasecmp(*cpp, "opt")) {
		u_long	olen;

		cpp++;
		olen = buildopts(*cpp, opts);
		if (olen) {
			bcopy(opts, (char *)(ip + 1), olen);
			ip->ip_hl += olen >> 2;
		}
	}
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		bcopy((char *)tcp, ((char *)ip) + (ip->ip_hl << 2),
			sizeof(*tcp));
	else if (ip->ip_p == IPPROTO_ICMP)
		bcopy((char *)ic, ((char *)ip) + (ip->ip_hl << 2),
			sizeof(*ic));
	return 0;
}
