#	$NetBSD: Makefile,v 1.2 2012/04/04 10:42:03 joerg Exp $

LIB=	LLVMVectorize

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/lib/Transforms/Vectorize

SRCS+=	BBVectorize.cpp \
	Vectorize.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
