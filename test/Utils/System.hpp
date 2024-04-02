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
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif

void
set_parent_death_signal(pid_t ppid_before_fork, const char *child_program_name)
{
	/**
	 * Kill the child (the current process) when the test (the parent
	 * process) is finished.
	 */
#ifdef __linux__
	if (prctl(PR_SET_PDEATHSIG, SIGTERM) == -1) {
		fprintf(stderr, "Can't launch %s: %s", child_program_name,
			strerror(errno));
		exit(EXIT_FAILURE);
	}
#endif
	if (getppid() != ppid_before_fork) {
		fprintf(stderr, "Can't launch %s: parent process exited "
				"just before prctl call", child_program_name);
		exit(EXIT_FAILURE);
	}
}

#ifdef __linux__

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
	set_parent_death_signal(ppid_before_fork, "Tarantool");
	const char *script = enable_ssl ? "test_cfg_ssl.lua" : "test_cfg.lua";
	if (execlp("tarantool", "tarantool", script, NULL) == -1) {
		fprintf(stderr, "Can't launch Tarantool: execlp failed! %s\n",
			strerror(errno));
		kill(getppid(), SIGKILL);
	}
	exit(EXIT_FAILURE);
}

#else

/**
 * Launches intermediate process, connected with parent by pipe.
 * The intermediate process lauches another process, running tarantool, and then
 * falls asleep on reading from pipe. When parent process is dead, the pipe
 * will be closed, and the intermediate process will read EOF. Right after, it
 * will kill its child process, which is Tarantool, and exit.
 * In the case, when the intermediate process fails to fork Tarantool, it kills
 * the parent process and terminates.
 */
inline int
launchTarantool(bool enable_ssl = false)
{
	pid_t ppid_before_fork = getpid();
	int p[2];
	pipe(p);
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
	 * It is necessary to close write end of pipe here if we want to read
	 * EOF when the parent process is dead.
	 */
	close(p[1]);
	pid = fork();
	if (pid == -1) {
		/*
		 * We've already returned OK in the parent, so let's kill it if
		 * the intermediate process fails to fork Tarantool.
		 */
		kill(ppid_before_fork, SIGKILL);
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		close(p[0]);
		const char *script = enable_ssl ? "test_cfg_ssl.lua" : "test_cfg.lua";
		if (execlp("tarantool", "tarantool", script, NULL) == -1) {
			fprintf(stderr, "Can't launch Tarantool: execlp failed! %s\n",
			strerror(errno));
			kill(getppid(), SIGKILL);
		}
	}
	char buf[1];
	read(p[0], buf, 1);
	kill(pid, SIGTERM);
	exit(0);
}

#endif

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

/**
 * Fork a dummy server that accepts one connection on
 * `localhost:dummy_server_port`, reads 1 byte and exits.
 */
int
launchDummyServer(const char *addr, unsigned port)
{
	pid_t ppid_before_fork = ::getpid();
	pid_t pid = ::fork();
	if (pid < 0) {
		::perror("fork failed");
		return -1;
	} else if (pid != 0) {
		::sleep(1);
		return 0;
	}

	set_parent_death_signal(ppid_before_fork, "dummy server");

	int sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		::perror("socket failed");
		::exit(EXIT_FAILURE);
	}

	int opt = 1;
	if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		       &opt, sizeof(opt)) != 0) {
		::perror("setsockopt failed");
		::exit(EXIT_FAILURE);
	}
	if (::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT,
		       &opt, sizeof(opt)) != 0) {
		::perror("setsockopt failed");
		::exit(EXIT_FAILURE);
	}

	struct sockaddr_in sock_address{};
	sock_address.sin_family = AF_INET;
	sock_address.sin_port = htons(static_cast<uint16_t>(port));
	if (::inet_aton(addr, &sock_address.sin_addr) != 1) {
		::perror("inet_aton failed");
		::exit(EXIT_FAILURE);
	}

	if (::bind(sock,
		   reinterpret_cast<const struct sockaddr *>(&sock_address),
		   sizeof(sockaddr)) != 0) {
		::perror("bind failed");
		::exit(EXIT_FAILURE);
	}

	if (::listen(sock, 1) != 0) {
		::perror("listen failed");
		::exit(EXIT_FAILURE);
	}

	int accepted_sock = ::accept(sock, nullptr, nullptr);
	if (accepted_sock < 0) {
		::perror("accept failed");
		::exit(EXIT_FAILURE);
	}

	::close(sock);

	/* Allow first `send` call to this socket to succeed. */
	char buf[1];
	if (::read(accepted_sock, buf, sizeof(buf)) < 0) {
		::perror("read failed");
		::exit(EXIT_FAILURE);
	}

	::shutdown(accepted_sock, SHUT_RDWR);
	::close(accepted_sock);

	::exit(EXIT_SUCCESS);
}
