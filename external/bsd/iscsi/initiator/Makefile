# $NetBSD: Makefile,v 1.5 2011/08/17 09:24:23 christos Exp $

.sinclude "${.CURDIR}/../Makefile.inc"

.include <bsd.own.mk>

USE_FORT?=yes	# network client/local server

ISCSIDIST=	${.CURDIR}/../dist
.PATH:	${ISCSIDIST}/src/initiator

PROG=iscsi-initiator
SRCS=iscsi-initiator.c virtdir.c libkmod.c
LDADD+= -lrefuse -liscsi -lpuffs
DPADD+= ${LIBREFUSE} ${LIBISCSI} ${LIBPUFFS}
CPPFLAGS+= -I${ISCSIDIST}/include
CPPFLAGS+= -pthread
LDFLAGS+= -pthread
MAN=iscsi-initiator.8
WARNS=4

.ifdef MODULAR_KERNEL
CPPFLAGS+= -DUSE_LIBKMOD
LDADD+= -lprop
DPADD+= ${LIBPROP}
.endif

.include <bsd.prog.mk>
