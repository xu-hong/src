#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93
#	$NetBSD: Makefile,v 1.15 2011/01/02 05:30:11 tsutsui Exp $

CLEANFILES+=stboot bootst bootst.bug

PROG=bootst.bug
SRCS=   boot.c conf.c dev_tape.c rawfs.c
LIBS=	${LIBSA} ${LIBKERN} ${LIBBUG}
SRTOBJ=
DPADD=	${LIBS} ${WRTVID}

bootst stboot: ${PROG} ${WRTVID}
	@${SIZE} bootst.bug
	@${STRIP} bootst.bug
	@${OBJCOPY} --output-target=binary bootst.bug bootst
	@${WRTVID} bootst	# creates stboot

proginstall::
	${INSTALL_FILE} ${STRIPFLAG} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		bootst ${DESTDIR}${MDEC_DIR}
	${INSTALL_FILE} ${STRIPFLAG} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		stboot ${DESTDIR}${MDEC_DIR}

.include "../Makefile.booters"

all realall: bootst
.include <bsd.prog.mk>
