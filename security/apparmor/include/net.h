/*
 * AppArmor security module
 *
 * This file contains AppArmor network mediation definitions.
 *
 * Copyright (C) 1998-2008 Novell/SUSE
 * Copyright 2009-2014 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#ifndef __AA_NET_H
#define __AA_NET_H

#include <net/sock.h>

#include "apparmorfs.h"
#include "label.h"
#include "perms.h"
#include "policy.h"

#define AA_MAY_SEND		AA_MAY_WRITE
#define AA_MAY_RECEIVE		AA_MAY_READ

#define AA_MAY_SHUTDOWN		AA_MAY_DELETE

#define AA_MAY_CONNECT		AA_MAY_OPEN
#define AA_MAY_ACCEPT		0x00100000

#define AA_MAY_BIND		0x00200000
#define AA_MAY_LISTEN		0x00400000

#define AA_MAY_SETOPT		0x01000000
#define AA_MAY_GETOPT		0x02000000

#define NET_PERMS_MASK (AA_MAY_SEND | AA_MAY_RECEIVE | AA_MAY_CREATE |    \
			AA_MAY_SHUTDOWN | AA_MAY_BIND | AA_MAY_LISTEN |	  \
			AA_MAY_CONNECT | AA_MAY_ACCEPT | AA_MAY_SETATTR | \
			AA_MAY_GETATTR | AA_MAY_SETOPT | AA_MAY_GETOPT)

#define NET_FS_PERMS (AA_MAY_SEND | AA_MAY_RECEIVE | AA_MAY_CREATE |	\
		      AA_MAY_SHUTDOWN | AA_MAY_CONNECT | AA_MAY_RENAME |\
		      AA_MAY_SETATTR | AA_MAY_GETATTR | AA_MAY_CHMOD |	\
		      AA_MAY_CHOWN | AA_MAY_CHGRP | AA_MAY_LOCK |	\
		      AA_MAY_MPROT)

#define NET_PEER_MASK (AA_MAY_SEND | AA_MAY_RECEIVE | AA_MAY_CONNECT |	\
		       AA_MAY_ACCEPT)
struct aa_sk_cxt {
	struct aa_label *label;
	struct aa_label *peer;
};

#define SK_CXT(X) (X)->sk_security
#define SOCK_CXT(X) SOCK_INODE(X)->i_security
#define DEFINE_AUDIT_NET(NAME, OP, SK, F, T, P)				  \
	struct lsm_network_audit NAME ## _net = { .sk = (SK),		  \
						  .family = (F)};	  \
	DEFINE_AUDIT_DATA(NAME,						  \
			  (SK) ? LSM_AUDIT_DATA_NET : LSM_AUDIT_DATA_NONE,\
			  OP);						  \
	NAME.u.net = &(NAME ## _net);					  \
	aad(&NAME)->net.type = (T);					  \
	aad(&NAME)->net.protocol = (P)

/* struct aa_net - network confinement data
 * @allowed: basic network families permissions
 * @audit_network: which network permissions to force audit
 * @quiet_network: which network permissions to quiet rejects
 */
struct aa_net {
	u16 allow[AF_MAX];
	u16 audit[AF_MAX];
	u16 quiet[AF_MAX];
};


extern struct aa_fs_entry aa_fs_entry_network[];

void audit_net_cb(struct audit_buffer *ab, void *va);
int aa_profile_af_perm(struct aa_profile *profile, int op, u16 family,
		       int type, int protocol, struct sock *sk);
int aa_af_perm(struct aa_label *label, int op, u32 request, u16 family,
	       int type, int protocol, struct sock *sk);
int aa_sock_perm(int op, u32 request, struct socket *sock);
int aa_sock_create_perm(struct aa_label *label, int family, int type,
			int protocol);
int aa_sock_bind_perm(struct socket *sock, struct sockaddr *address,
		      int addrlen);
int aa_sock_connect_perm(struct socket *sock, struct sockaddr *address,
			 int addrlen);
int aa_sock_listen_perm(struct socket *sock, int backlog);
int aa_sock_accept_perm(struct socket *sock, struct socket *newsock);
int aa_sock_msg_perm(int op, u32 request, struct socket *sock,
		     struct msghdr *msg, int size);
int aa_sock_opt_perm(int op, u32 request, struct socket *sock, int level,
		     int optname);
int aa_sock_file_perm(struct aa_label *label, int op, u32 request,
		      struct socket *sock);


static inline void aa_free_net_rules(struct aa_net *new)
{
	/* NOP */
}

#endif /* __AA_NET_H */
