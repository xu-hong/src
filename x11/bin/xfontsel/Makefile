#	$NetBSD: Makefile,v 1.2 2003/09/13 20:18:20 lukem Exp $

.include <bsd.own.mk>

PROG=	xfontsel
SRCS=	xfontsel.c ULabel.c

APPDEFS=XFontSel.ad

LDADD+=	-lXaw -lXmu -lXt -lSM -lICE -lXpm -lXext -lX11
DPADD+=	${LIBXAW} ${LIBXMU} ${LIBXT} ${LIBSM} ${LIBICE} \
	${LIBXPM} ${LIBXEXT} ${LIBX11}

.PATH:	${X11SRCDIR.xc}/programs/${PROG}

.include <bsd.x11.mk>
.include <bsd.prog.mk>
