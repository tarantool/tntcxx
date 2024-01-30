/*
 Copyright 2010-2022 Tarantool AUTHORS: please see AUTHORS file.

 Redistribution and use in source and binary forms, with or
 without modification, are permitted provided that the following
 conditions are met:

 1. Redistributions of source code must retain the above
    copyright notice, this list of conditions and the
    following disclaimer.

 2. Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials
    provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.
*/
#pragma once

#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>

#include "UnixPlainStream.hpp"

#ifdef TNTCXX_ENABLE_SSL_GOST
extern void
ENGINE_load_gost(void);
#else
static inline void
ENGINE_load_gost(void) {}
#endif // TNTCXX_ENABLE_SSL_GOST

/**
 * Holder of SSL context.
 */
class SSLContext {
public:
	SSLContext() = default;
	inline ~SSLContext();
	SSLContext(const SSLContext&) = delete;
	SSLContext &operator=(const SSLContext&) = delete;
	SSLContext(SSLContext&&) = default;
	SSLContext &operator=(SSLContext&&) = default;

	/**
	 * Create context with options. Return 0 on success, -1 on error.
	 * See get_last_error() in case of error.
	 */
	inline int create(const ConnectOptions &opts);
	/** Cast to ssl context pointer. */
	operator SSL_CTX *() const { return ssl_ctx; }
	/** Get last error that happend upon creation. */
	const char *get_last_error() const { return last_error; }

private:
	/**
	 * Dummy callback passed to SSL_CTX_set_default_passwd_cb.
	 * Used to disable command-line prompt.
	 */
	static inline int
	dummy_passwd_cb(char *, int , int , void *) { return 0; }

	/**
	 * Loads SSL private key and returns 0 on success or -1 on error.
	 *
	 * The private key file may be encrypted. This function tries to decrypt
	 * the key using passwords in the following order:
	 *  1. String stored in the passwd argument. Skipped if passwd is NULL.
	 *  2. Every line from the file specified by the passwd_file argument.
	 *     Skipped if passwd_file is NULL.
	 *  3. Empty password.
	 */
	inline int
	load_private_key(const ConnectOptions &opts);

	Resource<SSL_CTX *, nullptr> ssl_ctx;
	const char *last_error = nullptr;
};

/**
 * Unix stream that supports SSL encryption.
 * Support non-encrypted connections too.
 */
class UnixSSLStream : public UnixPlainStream {
public:
	UnixSSLStream() noexcept = default;
	inline ~UnixSSLStream();
	UnixSSLStream(const UnixSSLStream&) = delete;
	UnixSSLStream &operator=(const UnixSSLStream&) = delete;
	UnixSSLStream(UnixSSLStream &&a) noexcept = default;
	UnixSSLStream &operator=(UnixSSLStream &&a) noexcept = default;

	/**
	 * Connect to address. Return 0 on success, -1 on error.
	 * Pending (inprogress) connection has a successfull result.
	 */
	int connect(const ConnectOptions &opts);
	/**
	 * Receive data to connection.
	 * Return positive number - number of bytes was received.
	 * Return 0 if nothing was received but there's no error.
	 * Return -1 on error.
	 * One must check the stream status to understand what happens.
	 */
	ssize_t send(struct iovec *iov, size_t iov_count);
	/**
	 * Receive data to connection.
	 * Return positive number - number of bytes was received.
	 * Return 0 if nothing was received but there's no error.
	 * Return -1 on error.
	 * One must check the stream status to understand what happens.
	 */
	ssize_t recv(struct iovec *iov, size_t iov_count);

private:
	SSLContext ssl_context;
	Resource<SSL *, nullptr> ssl;
};

/////////////////////////////////////////////////////////////////////
////////////////////////// Implementation  //////////////////////////
/////////////////////////////////////////////////////////////////////

namespace {

class SSLInit {
public:
	static SSLInit &instance();
private:
	inline SSLInit();
	inline ~SSLInit();
};

SSLInit& SSLInit::instance()
{
	static SSLInit instance;
	return instance;
}

SSLInit::SSLInit()
{
	/* NB: GOST engine must be loaded before OpenSSL initialization. */
	ENGINE_load_gost();
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	OpenSSL_add_all_digests();
		OpenSSL_add_all_ciphers();
		ERR_load_crypto_strings();
#else
	OPENSSL_init_crypto(0, NULL);
	OPENSSL_init_ssl(0, NULL);
#endif
}

SSLInit::~SSLInit()
{
#ifdef OPENSSL_cleanup
	OPENSSL_cleanup();
#endif
}

} // anonymous namespace

SSLContext::~SSLContext()
{
	if (ssl_ctx != nullptr)
		SSL_CTX_free(ssl_ctx);
}

int SSLContext::create(const ConnectOptions &opts)
{
	SSLInit::instance();

	const char *cert_file = opts.ssl_cert_file.empty() ?
				nullptr : opts.ssl_cert_file.c_str();
	const char *key_file = opts.ssl_key_file.empty() ?
			       nullptr : opts.ssl_key_file.c_str();
	const char *ca_file = opts.ssl_ca_file.empty() ?
			      nullptr : opts.ssl_ca_file.c_str();
	const char *ciphers = opts.ssl_ciphers.empty() ?
			      nullptr : opts.ssl_ciphers.c_str();

	if (ssl_ctx != nullptr)
		SSL_CTX_free(ssl_ctx);
	const SSL_METHOD *method = TLS_client_method();
	ssl_ctx = SSL_CTX_new(method);
	if (ssl_ctx == NULL) {
		last_error = "SSL_CTX_new failed";
		return -1;
	}

	/*
	 * Require TLSv1.2, because other protocol versions don't seem to
	 * support the GOST cipher:
	 *
	 *   $ openssl ciphers -s -tls1_2 | tr ':' '\n' | grep GOST
	 *
	 * (Should we add a configuration parameter for this?)
	 */
	if (SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION) != 1 ||
	    SSL_CTX_set_max_proto_version(ssl_ctx, TLS1_2_VERSION) != 1) {
		last_error = "Error setting SSL protocol version";
		return -1;
	}
	if (cert_file != NULL &&
	    SSL_CTX_use_certificate_file(ssl_ctx, cert_file,
					 SSL_FILETYPE_PEM) != 1) {
		last_error = "Error loading SSL certificate";
		return -1;
	}
	if (key_file != NULL &&
	    load_private_key(opts) != 0)
		return -1;
	if (ca_file != NULL &&
	    SSL_CTX_load_verify_locations(ssl_ctx, ca_file, NULL) != 1) {
		last_error = "Error loading SSL CA";
		return -1;
	}
	if (ca_file != NULL) {
		SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER |
					    SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				   NULL);
	}
	/*
	 * NB: SSL_CTX_set_cipher_list() only works for procol versions TLSv1.2
	 * and below. For TLSv1.3 we'd have to use SSL_CTX_set_ciphersuites()
	 * instead.
	 */
	if (ciphers != NULL &&
	    SSL_CTX_set_cipher_list(ssl_ctx, ciphers) != 1) {
		last_error = "Error setting SSL ciphers";
		return -1;
	}
	return 0;
}

int SSLContext::load_private_key(const ConnectOptions &opts)
{
	const char *key_file = opts.ssl_key_file.empty() ?
			       nullptr : opts.ssl_key_file.c_str();
	const char *passwd = opts.ssl_passwd.empty() ?
			     nullptr : opts.ssl_passwd.c_str();
	const char *passwd_file = opts.ssl_passwd_file.empty() ?
				  nullptr : opts.ssl_passwd_file.c_str();

	/*
	 * Set the password callback to NULL to make the SSL library use
	 * the callback userdata for a password.
	 */
	SSL_CTX_set_default_passwd_cb(ssl_ctx, NULL);

	if (passwd != NULL) {
		/*
		 * Try to load the key file using the password specified
		 * in the passwd argument.
		 */
		SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (void *)passwd);
		int ret = SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file,
						      SSL_FILETYPE_PEM);
		SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, NULL);
		if (ret == 1)
			return 0;
	}
	if (passwd_file != NULL) {
		/*
		 * Try to load the key file using every password stored in
		 * the password file.
		 */
		FILE *f = fopen(passwd_file, "r");
		if (f == NULL) {
			last_error = "Error reading SSL password file";
			return -1;
		}
		char *buf = NULL;
		size_t buf_size = 0;
		bool is_error = false;
		bool is_loaded = false;
		while (true) {
			/* Read a line from the password file. */
			errno = 0;
			ssize_t len = getline(&buf, &buf_size, f);
			if (len <= 0) {
				if (errno == 0)
					break; /* EOF */
				last_error = "Error reading SSL password file";
				is_error = true;
				break;
			}
			char *s = buf;
			/* Trim a terminating new line. */
			if (s[len - 1] == '\n')
				s[len - 1] = '\0';
			/* Try to load the key file using the password. */
			SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, s);
			int ret = SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file,
							      SSL_FILETYPE_PEM);
			SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, NULL);
			if (ret == 1) {
				is_loaded = true;
				break;
			}
			/* Ignore the error and try another password. */
			ERR_clear_error();
		}
		free(buf);
		fclose(f);
		if (is_loaded)
			return 0;
		if (is_error)
			return -1;
	}
	/* Try to load the key file without a password. */
	SSL_CTX_set_default_passwd_cb(ssl_ctx, dummy_passwd_cb);
	int ret = SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file,
					      SSL_FILETYPE_PEM);
	SSL_CTX_set_default_passwd_cb(ssl_ctx, NULL);
	if (ret != 1) {
		last_error = "Error loading SSL private key";
		return -1;
	}
	return 0;
}

UnixSSLStream::~UnixSSLStream()
{
	if (ssl != nullptr)
		SSL_free(ssl);
}

int UnixSSLStream::connect(const ConnectOptions &opts_arg)
{
	if (ssl != nullptr) {
		SSL_free(ssl);
		ssl = nullptr;
	}

	if (UnixStream::connect(opts_arg) != 0)
		return -1;
	if (opts.transport == STREAM_PLAIN)
		return 0;
	assert(opts.transport == STREAM_SSL);

#ifdef __FreeBSD__
	// SO_NOSIGPIPE is great, but only defined on FreeBSD platforms.
	int opt = 1;
	if (::setsockopt(get_fd(), SOL_SOCKET, SO_NOSIGPIPE,
			 &opt, sizeof(opt)) != 0) {
		return US_DIE("setsockopt failed", strerror(errno));
	}
#endif

	if (ssl_context.create(opts) != 0)
		return US_DIE("SSL_context create failed",
			      ssl_context.get_last_error());

	if ((ssl = SSL_new(ssl_context)) == NULL)
		return US_DIE("SSL_new failed");

	if (SSL_set_fd(ssl, get_fd()) != 1)
		return US_DIE("SSL_set_fd failed");

	SSL_set_connect_state(ssl);

	/* Trigger client-server negotiation. */
	size_t rcvd;
	SSL_read_ex(ssl, nullptr, 0, &rcvd);

	return 0;
}

ssize_t UnixSSLStream::send(struct iovec *iov, size_t iov_count)
{
	if (opts.transport == STREAM_PLAIN)
		return UnixPlainStream::send(iov, iov_count);
	assert(opts.transport == STREAM_SSL);

	if (!(has_status(SS_ESTABLISHED))) {
		if (has_status(SS_DEAD))
			return US_DIE("Send to dead stream");
		if (check_pending() != 0)
			return -1;
		if (iov_count == 0)
			return 0;
	}

	remove_status(SS_NEED_EVENT_FOR_WRITE);
	size_t sent;
	int ret = SSL_write_ex(ssl, iov->iov_base, iov->iov_len, &sent);
	if (ret == 1)
		return static_cast<ssize_t>(sent);

	int err = SSL_get_error(ssl, ret);
	switch (err) {
	case SSL_ERROR_WANT_READ:
		return set_status(SS_NEED_READ_EVENT_FOR_WRITE);
	case SSL_ERROR_WANT_WRITE:
		return set_status(SS_NEED_WRITE_EVENT_FOR_WRITE);
	case SSL_ERROR_SSL:
		return US_DIE("SSL send failed");
	default:
		assert(err == SSL_ERROR_SYSCALL);
		if (errno == 0) {
			/*
			 * The remote end closed the socket for reading.
			 * The OpenSSL library treats this situation as
			 * a system error with errno = 0. We report it
			 * as EPIPE.
			 */
			errno = EPIPE;
		}
		return US_DIE("Send failed", strerror(errno));
	}
}

ssize_t UnixSSLStream::recv(struct iovec *iov, size_t iov_count)
{
	if (opts.transport == STREAM_PLAIN)
		return UnixPlainStream::recv(iov, iov_count);
	assert(opts.transport == STREAM_SSL);

	if (!(has_status(SS_ESTABLISHED))) {
		if (has_status(SS_DEAD))
			return US_DIE("Recv from dead stream");
		else
			return US_DIE("Recv from pending stream");
	}

	remove_status(SS_NEED_EVENT_FOR_READ);
	errno = 0;
	size_t rcvd;
	int ret = SSL_read_ex(ssl, iov->iov_base, iov->iov_len, &rcvd);
	if (ret == 1)
		return static_cast<ssize_t>(rcvd);

	int err = SSL_get_error(ssl, ret);
	switch (err) {
	case SSL_ERROR_ZERO_RETURN:
		return 0;
	case SSL_ERROR_WANT_READ:
		return set_status(SS_NEED_READ_EVENT_FOR_READ);
	case SSL_ERROR_WANT_WRITE:
		return set_status(SS_NEED_WRITE_EVENT_FOR_READ);
	case SSL_ERROR_SSL:
		return US_DIE("SSL revc failed");
	default:
		assert(err == SSL_ERROR_SYSCALL);
		if (errno == 0) {
			/*
			 * The remote end closed the socket for writing.
			 * The OpenSSL library treats this situation as
			 * a system error with errno = 0. We ignore it.
			 */
			return 0;
		}
		return US_DIE("Send failed", strerror(errno));
	}
}
