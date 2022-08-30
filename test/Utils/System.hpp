#pragma once
/*
 * Copyright 2010-2020, Tarantool AUTHORS, please see AUTHORS file.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

inline int
launchTarantool(bool enable_ssl = false)
{
	pid_t ppid_before_fork = getpid();
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Can't launch Tarantool: fork failed! %s",
			strerror(errno));
		return -1;
	}
	/* Returning from parent. */
	if (pid != 0)
		return 0;
	/*
	 * Kill child (i.e. Tarantool process) when the test is finished.
	 * Doesn't work on MacOS tho.
	 */
#ifdef __linux__
	if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
		fprintf(stderr, "Can't launch Tarantool: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
	if (getppid() != ppid_before_fork) {
		fprintf(stderr, "Can't launch Tarantool: parent process exited "\
				"just before prctl call");
		exit(EXIT_FAILURE);
	}
	const char *script = enable_ssl ? "test_cfg_ssl.lua" : "test_cfg.lua";
	if (execlp("tarantool", "tarantool", script, NULL) == -1) {
		fprintf(stderr, "Can't launch Tarantool: execlp failed! %s\n",
			strerror(errno));
		kill(getppid(), SIGKILL);
	}
	exit(EXIT_FAILURE);
}

inline int
cleanDir() {
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Failed to clean directory: fork failed! %s\n",
			strerror(errno));
		return -1;
	}
	if (pid != 0) {
		int status;
		wait(&status);
		if (WIFEXITED(status) != 0)
			return 0;
		fprintf(stderr, "wait: child finished with error \n");
		return -1;
	}
	if (execlp("/bin/sh", "/bin/sh", "-c",
		   "rm -f *.xlog *.snap tarantool.log", NULL) == -1) {
		fprintf(stderr, "Failed to clean directory: execlp failed! %s\n",
			strerror(errno));
	}
	exit(EXIT_FAILURE);
}

inline int
genSSLCert() {
	pid_t pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Failed to clean directory: fork failed! %s\n",
			strerror(errno));
		return -1;
	}
	if (pid != 0) {
		int status;
		wait(&status);
		if (WIFEXITED(status) != 0)
			return 0;
		fprintf(stderr, "wait: child finished with error \n");
		return -1;
	}
	if (execlp("/bin/sh", "/bin/sh", "-c",
		   "./test_gen_ssl.sh", NULL) == -1) {
		fprintf(stderr, "Failed to generate ssl: execlp failed! %s\n",
			strerror(errno));
	}
	exit(EXIT_FAILURE);
}
