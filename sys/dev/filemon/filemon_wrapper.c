/*
 * Copyright (c) 2010, Juniper Networks, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: filemon_wrapper.c,v 1.3 2011/09/24 18:08:15 sjg Exp $");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lwp.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include "filemon.h"

static int
filemon_wrapper_chdir(struct lwp * l, const struct sys_chdir_args * uap,
    register_t * retval)
{
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;
	
	if ((ret = sys_chdir(l, uap, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {

			error = copyinstr(SCARG(uap, path), filemon->fm_fname1,
			    sizeof(filemon->fm_fname1), &done);
			if (error == 0) {
				len = snprintf(filemon->fm_msgbufr,
				    sizeof(filemon->fm_msgbufr),
				    "C %d %s\n",
				    curproc->p_pid, filemon->fm_fname1);

				filemon_output(filemon, filemon->fm_msgbufr,
				    len);
			}
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}

static int
filemon_wrapper_execve(struct lwp * l, struct sys_execve_args * uap,
    register_t * retval)
{
	char fname[MAXPATHLEN];
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;
	
	error = copyinstr(SCARG(uap, path), fname, sizeof(fname), &done);

	if ((ret = sys_execve(l, uap, retval)) == EJUSTRETURN && error == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {

			len = snprintf(filemon->fm_msgbufr, sizeof(filemon->fm_msgbufr),
			    "E %d %s\n",
			    curproc->p_pid, fname);

			filemon_output(filemon, filemon->fm_msgbufr, len);
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}


static int
filemon_wrapper_fork(struct lwp * l, const void *v, register_t * retval)
{
	int ret;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_fork(l, v, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			len = snprintf(filemon->fm_msgbufr,
			    sizeof(filemon->fm_msgbufr),
			    "F %d %ld\n",
			    curproc->p_pid, (long) retval[0]);

			filemon_output(filemon, filemon->fm_msgbufr, len);

			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}

static int
filemon_wrapper_vfork(struct lwp * l, const void *v, register_t * retval)
{
	int ret;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_vfork(l, v, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			len = snprintf(filemon->fm_msgbufr,
			    sizeof(filemon->fm_msgbufr),
			    "F %d %ld\n",
			    curproc->p_pid, (long) retval[0]);

			filemon_output(filemon, filemon->fm_msgbufr, len);

			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}

static int
filemon_wrapper_open(struct lwp * l, struct sys_open_args * uap,
    register_t * retval)
{
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_open(l, uap, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			error = copyinstr(SCARG(uap, path), filemon->fm_fname1,
			    sizeof(filemon->fm_fname1), &done);
			if (error == 0) {
				if (SCARG(uap, flags) & O_RDWR) {
					/* we want a separate R record */
					len = snprintf(filemon->fm_msgbufr,
						sizeof(filemon->fm_msgbufr),
						"R %d %s\n",
						curproc->p_pid,
						filemon->fm_fname1);

					filemon_output(filemon,
						filemon->fm_msgbufr, len);
				}			
				len = snprintf(filemon->fm_msgbufr,
				    sizeof(filemon->fm_msgbufr),
				    "%c %d %s\n",
				    (SCARG(uap, flags) & O_ACCMODE) ? 'W' : 'R',
				    curproc->p_pid, filemon->fm_fname1);

				filemon_output(filemon, filemon->fm_msgbufr,
				    len);
			}
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}

static int
filemon_wrapper_rename(struct lwp * l, struct sys_rename_args * uap,
    register_t * retval)
{
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_rename(l, uap, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			error = copyinstr(SCARG(uap, from), filemon->fm_fname1,
			    sizeof(filemon->fm_fname1), &done);
			if (error == 0) 
				error = copyinstr(SCARG(uap, to),
				    filemon->fm_fname2,
				    sizeof(filemon->fm_fname2), &done);
			if (error == 0) {
				len = snprintf(filemon->fm_msgbufr,
				    sizeof(filemon->fm_msgbufr),
				    "M %d '%s' '%s'\n",
				    curproc->p_pid, filemon->fm_fname1,
				    filemon->fm_fname2);

				filemon_output(filemon, filemon->fm_msgbufr,
				    len);
			}
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}

static int
filemon_wrapper_link(struct lwp * l, struct sys_link_args * uap,
    register_t * retval)
{
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_link(l, uap, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			error = copyinstr(SCARG(uap, path),
			    filemon->fm_fname1, sizeof(filemon->fm_fname1),
			    &done);
			if (error == 0)
				error = copyinstr(SCARG(uap, link),
				    filemon->fm_fname2,
				    sizeof(filemon->fm_fname2), &done);
			if (error == 0) {
				len = snprintf(filemon->fm_msgbufr,
				    sizeof(filemon->fm_msgbufr), "L %d '%s' '%s'\n",
				    curproc->p_pid, filemon->fm_fname1,
				    filemon->fm_fname2);

				filemon_output(filemon, filemon->fm_msgbufr,
				    len);
			}
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}

static int
filemon_wrapper_symlink(struct lwp * l, struct sys_symlink_args * uap,
    register_t * retval)
{
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_symlink(l, uap, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			error = copyinstr(SCARG(uap, path),
			    filemon->fm_fname1,
			    sizeof(filemon->fm_fname1), &done);
			if (error == 0)
				error = copyinstr(SCARG(uap, link),
				    filemon->fm_fname2,
				    sizeof(filemon->fm_fname2), &done);
			if (error == 0) {
				len = snprintf(filemon->fm_msgbufr,
				    sizeof(filemon->fm_msgbufr),
				    "L %d '%s' '%s'\n",
				    curproc->p_pid, filemon->fm_fname1,
				    filemon->fm_fname2);

				filemon_output(filemon, filemon->fm_msgbufr,
				    len);
			}
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}


static void
filemon_wrapper_sys_exit(struct lwp * l, struct sys_exit_args * uap,
    register_t * retval)
{
	size_t len;
	struct filemon *filemon;

	filemon = filemon_lookup(curproc);

	if (filemon) {
		len = snprintf(filemon->fm_msgbufr,
		    sizeof(filemon->fm_msgbufr), "X %d %d\n",
		    curproc->p_pid, SCARG(uap, rval));

		filemon_output(filemon, filemon->fm_msgbufr, len);

		/* Check if the monitored process is about to exit. */
		if (filemon->fm_pid == curproc->p_pid) {
			len = snprintf(filemon->fm_msgbufr,
			    sizeof(filemon->fm_msgbufr), "# Bye bye\n");

			filemon_output(filemon, filemon->fm_msgbufr, len);
		}
		rw_exit(&filemon->fm_mtx);
	}
	sys_exit(l, uap, retval);
}

static int
filemon_wrapper_unlink(struct lwp * l, struct sys_unlink_args * uap,
    register_t * retval)
{
	int ret;
	int error;
	size_t done;
	size_t len;
	struct filemon *filemon;

	if ((ret = sys_unlink(l, uap, retval)) == 0) {
		filemon = filemon_lookup(curproc);

		if (filemon) {
			error = copyinstr(SCARG(uap, path),
			    filemon->fm_fname1,
			    sizeof(filemon->fm_fname1), &done);
			if (error == 0) {
				len = snprintf(filemon->fm_msgbufr,
				    sizeof(filemon->fm_msgbufr),
				    "D %d %s\n",
				    curproc->p_pid, filemon->fm_fname1);

				filemon_output(filemon, filemon->fm_msgbufr,
				    len);
			}
			rw_exit(&filemon->fm_mtx);
		}
	}
	return (ret);
}


void
filemon_wrapper_install(void)
{
	struct sysent *sv_table = curproc->p_emul->e_sysent;

	sv_table[SYS_chdir].sy_call = (sy_call_t *) filemon_wrapper_chdir;
	sv_table[SYS_execve].sy_call = (sy_call_t *) filemon_wrapper_execve;
	sv_table[SYS_exit].sy_call = (sy_call_t *) filemon_wrapper_sys_exit;
	sv_table[SYS_fork].sy_call = (sy_call_t *) filemon_wrapper_fork;
	sv_table[SYS_link].sy_call = (sy_call_t *) filemon_wrapper_link;
	sv_table[SYS_open].sy_call = (sy_call_t *) filemon_wrapper_open;
	sv_table[SYS_rename].sy_call = (sy_call_t *) filemon_wrapper_rename;
	sv_table[SYS_symlink].sy_call = (sy_call_t *) filemon_wrapper_symlink;
	sv_table[SYS_unlink].sy_call = (sy_call_t *) filemon_wrapper_unlink;
	sv_table[SYS_vfork].sy_call = (sy_call_t *) filemon_wrapper_vfork;
}

void
filemon_wrapper_deinstall(void)
{
	struct sysent *sv_table = curproc->p_emul->e_sysent;

	sv_table[SYS_chdir].sy_call = (sy_call_t *) sys_chdir;
	sv_table[SYS_execve].sy_call = (sy_call_t *) sys_execve;
	sv_table[SYS_exit].sy_call = (sy_call_t *) sys_exit;
	sv_table[SYS_fork].sy_call = (sy_call_t *) sys_fork;
	sv_table[SYS_link].sy_call = (sy_call_t *) sys_link;
	sv_table[SYS_open].sy_call = (sy_call_t *) sys_open;
	sv_table[SYS_rename].sy_call = (sy_call_t *) sys_rename;
	sv_table[SYS_symlink].sy_call = (sy_call_t *) sys_symlink;
	sv_table[SYS_unlink].sy_call = (sy_call_t *) sys_unlink;
	sv_table[SYS_vfork].sy_call = (sy_call_t *) sys_vfork;
}
