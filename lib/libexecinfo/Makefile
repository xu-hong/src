# $NetBSD: Makefile,v 1.1 2012/05/26 22:02:29 christos Exp $

.include <bsd.own.mk>

USE_UNWIND=yes
WARNS?=4
INCSDIR=/usr/include
INCS=execinfo.h

#CPPFLAGS+='-D__RCSID(a)=' -D_GNU_SOURCE '-D__printflike(a,b)='
#CPPFLAGS+=-I/usr/include/libelf
#COPTS+=-std=gnu99

LIBDPLIBS+= elf	${NETBSDSRCDIR}/external/bsd/libelf/lib

LIB=execinfo
SRCS=symtab.c backtrace.c
MAN= backtrace.3

.if ${USE_UNWIND} == "yes"
SRCS+=unwind.c
.else
SRCS+=builtin.c
.endif

MLINKS+= backtrace.3 backtrace_symbols.3
MLINKS+= backtrace.3 backtrace_symbols_fmt.3
MLINKS+= backtrace.3 backtrace_symbols_fd.3
MLINKS+= backtrace.3 backtrace_symbols_fd_fmt.3

.include <bsd.lib.mk>
