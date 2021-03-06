/*	$NetBSD: mq_prio_test.c,v 1.2 2009/07/19 02:31:19 rmind Exp $	*/

/*
 * Test for POSIX message queue priority handling.
 *
 * This file is in the Public Domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include <mqueue.h>
#include <assert.h>

#define	MQUEUE			"/mq_prio_test"
#define	MQ_PRIO_BOUNDARY	31

static void
send_msgs(mqd_t mqfd)
{
	char msg[2];

	msg[1] = '\0';

	msg[0] = 'a';
	if (mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BOUNDARY) == -1)
		err(EXIT_FAILURE, "mq_send 1");

	msg[0] = 'b';
	if (mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BOUNDARY + 1) == -1)
		err(EXIT_FAILURE, "mq_send 2");

	msg[0] = 'c';
	if (mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BOUNDARY) == -1)
		err(EXIT_FAILURE, "mq_send 3");

	msg[0] = 'd';
	if (mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BOUNDARY - 1) == -1)
		err(EXIT_FAILURE, "mq_send 4");

	msg[0] = 'e';
	if (mq_send(mqfd, msg, sizeof(msg), 0) == -1)
		err(EXIT_FAILURE, "mq_send 5");

	msg[0] = 'f';
	if (mq_send(mqfd, msg, sizeof(msg), MQ_PRIO_BOUNDARY + 1) == -1)
		err(EXIT_FAILURE, "mq_send 6");
}

static void
receive_msgs(mqd_t mqfd)
{
	struct mq_attr mqa;
	char *m;
	unsigned p;
	int len;

	if (mq_getattr(mqfd, &mqa) == -1) {
		err(EXIT_FAILURE, "mq_getattr");
	}
	len = mqa.mq_msgsize;
	m = calloc(1, len);
	if (m == NULL) {
		err(EXIT_FAILURE, "calloc");
	}

	if (mq_receive(mqfd, m, len, &p) == -1) {
		err(EXIT_FAILURE, "mq_receive 1");
	}
	assert(p == (MQ_PRIO_BOUNDARY + 1) && m[0] == 'b');

	if (mq_receive(mqfd, m, len, &p) == -1) {
		err(EXIT_FAILURE, "mq_receive 2");
	}
	assert(p == (MQ_PRIO_BOUNDARY + 1) && m[0] == 'f');

	if (mq_receive(mqfd, m, len, &p) == -1) {
		err(EXIT_FAILURE, "mq_receive 3");
	}
	assert(p == MQ_PRIO_BOUNDARY && m[0] == 'a');

	if (mq_receive(mqfd, m, len, &p) == -1) {
		err(EXIT_FAILURE, "mq_receive 4");
	}
	assert(p == MQ_PRIO_BOUNDARY && m[0] == 'c');

	if (mq_receive(mqfd, m, len, &p) == -1) {
		err(EXIT_FAILURE, "mq_receive 5");
	}
	assert(p == (MQ_PRIO_BOUNDARY - 1) && m[0] == 'd');

	if (mq_receive(mqfd, m, len, &p) == -1) {
		err(EXIT_FAILURE, "mq_receive 6");
	}
	assert(p == 0 && m[0] == 'e');

	free(m);
}

int
main(int argc, char **argv)
{
	mqd_t mqfd;

	(void)mq_unlink(MQUEUE);

	mqfd = mq_open(MQUEUE, O_RDWR | O_CREAT,
	    S_IRUSR | S_IRWXG | S_IROTH, NULL);
	if (mqfd == -1)
		err(EXIT_FAILURE, "mq_open");

	send_msgs(mqfd);
	receive_msgs(mqfd);

	mq_close(mqfd);

	if (mq_unlink(MQUEUE) == -1)
		err(EXIT_FAILURE, "mq_unlink");

	return 0;
}
