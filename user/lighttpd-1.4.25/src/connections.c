#include "buffer.h"
#include "server.h"
#include "log.h"
#include "connections.h"
#include "fdevent.h"

#include "request.h"
#include "response.h"
#include "network.h"
#include "http_chunk.h"
#include "stat_cache.h"
#include "joblist.h"

#include "plugin.h"

#include "inet_ntop_cache.h"

#include "chunk_decoder.h"

#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#ifdef USE_OPENSSL
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#include "sys-socket.h"
#include "../../nvram/nvram_lib.h"

typedef struct {
	        PLUGIN_DATA;
} plugin_data;

static connection *connections_get_new_connection(server *srv) {
	connections *conns = srv->conns;
	size_t i;

	if (conns->size == 0) {
		conns->size = 128;
		conns->ptr = NULL;
		conns->ptr = malloc(sizeof(*conns->ptr) * conns->size);
		for (i = 0; i < conns->size; i++) {
			conns->ptr[i] = connection_init(srv);
		}
	} else if (conns->size == conns->used) {
		conns->size += 128;
		conns->ptr = realloc(conns->ptr, sizeof(*conns->ptr) * conns->size);

		for (i = conns->used; i < conns->size; i++) {
			conns->ptr[i] = connection_init(srv);
		}
	}

	connection_reset(srv, conns->ptr[conns->used]);
#if 0
	fprintf(stderr, "%s.%d: add: ", __FILE__, __LINE__);
	for (i = 0; i < conns->used + 1; i++) {
		fprintf(stderr, "%d ", conns->ptr[i]->fd);
	}
	fprintf(stderr, "\n");
#endif

	conns->ptr[conns->used]->ndx = conns->used;
	return conns->ptr[conns->used++];
}

static int connection_del(server *srv, connection *con) {
	size_t i;
	connections *conns = srv->conns;
	connection *temp;

	if (con == NULL) return -1;

	if (-1 == con->ndx) return -1;

	i = con->ndx;

	/* not last element */

	if (i != conns->used - 1) {
		temp = conns->ptr[i];
		conns->ptr[i] = conns->ptr[conns->used - 1];
		conns->ptr[conns->used - 1] = temp;

		conns->ptr[i]->ndx = i;
		conns->ptr[conns->used - 1]->ndx = -1;
	}

	conns->used--;

	con->ndx = -1;
#if 0
	fprintf(stderr, "%s.%d: del: (%d)", __FILE__, __LINE__, conns->used);
	for (i = 0; i < conns->used; i++) {
		fprintf(stderr, "%d ", conns->ptr[i]->fd);
	}
	fprintf(stderr, "\n");
#endif
	return 0;
}

int connection_close(server *srv, connection *con) {
#ifdef USE_OPENSSL
	server_socket *srv_sock = con->srv_socket;
#endif

#ifdef USE_OPENSSL
	if (srv_sock->is_ssl) {
		if (con->ssl) SSL_free(con->ssl);
		con->ssl = NULL;
	}
#endif

	/* If a transfer-encoding "chunked" connection gets closed, destroy the chunk decoder */
	if (con->chunk_decoder != NULL) {
		chunk_decoder_free((lp_chunk_decoder)con->chunk_decoder);
		con->chunk_decoder = NULL;
	}

	fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
	fdevent_unregister(srv->ev, con->fd);
#ifdef __WIN32
	if (closesocket(con->fd)) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"(warning) close:", con->fd, strerror(errno));
	}
#else
	if (close(con->fd)) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"(warning) close:", con->fd, strerror(errno));
	}
#endif

	srv->cur_fds--;
#if 0
	log_error_write(srv, __FILE__, __LINE__, "sd",
			"closed()", con->fd);
#endif

	connection_del(srv, con);
	connection_set_state(srv, con, CON_STATE_CONNECT);

	return 0;
}

#if 0
static void dump_packet(const unsigned char *data, size_t len) {
	size_t i, j;

	if (len == 0) return;

	for (i = 0; i < len; i++) {
		if (i % 16 == 0) fprintf(stderr, "  ");

		fprintf(stderr, "%02x ", data[i]);

		if ((i + 1) % 16 == 0) {
			fprintf(stderr, "  ");
			for (j = 0; j <= i % 16; j++) {
				unsigned char c;

				if (i-15+j >= len) break;

				c = data[i-15+j];

				fprintf(stderr, "%c", c > 32 && c < 128 ? c : '.');
			}

			fprintf(stderr, "\n");
		}
	}

	if (len % 16 != 0) {
		for (j = i % 16; j < 16; j++) {
			fprintf(stderr, "   ");
		}

		fprintf(stderr, "  ");
		for (j = i & ~0xf; j < len; j++) {
			unsigned char c;

			c = data[j];
			fprintf(stderr, "%c", c > 32 && c < 128 ? c : '.');
		}
		fprintf(stderr, "\n");
	}
}
#endif

static int connection_handle_read_ssl(server *srv, connection *con) {
#ifdef USE_OPENSSL
	int r, ssl_err, len, count = 0, read_offset, toread;
	buffer *b = NULL;

	if (!con->conf.is_ssl) return -1;

	ERR_clear_error();
	do {
		if (NULL != con->read_queue->last) {
			b = con->read_queue->last->mem;
		}

		if (NULL == b || b->size - b->used < 1024) {
			b = chunkqueue_get_append_buffer(con->read_queue);
			len = SSL_pending(con->ssl);
			if (len < 4*1024) len = 4*1024; /* always alloc >= 4k buffer */
			buffer_prepare_copy(b, len + 1);

			/* overwrite everything with 0 */
			memset(b->ptr, 0, b->size);
		}

		read_offset = (b->used > 0) ? b->used - 1 : 0;
		toread = b->size - 1 - read_offset;

		len = SSL_read(con->ssl, b->ptr + read_offset, toread);

		if (len > 0) {
			if (b->used > 0) b->used--;
			b->used += len;
			b->ptr[b->used++] = '\0';

			con->bytes_read += len;

			count += len;
		}
	} while (len == toread && count < MAX_READ_LIMIT);


	if (len < 0) {
		int oerrno = errno;
		switch ((r = SSL_get_error(con->ssl, len))) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
			con->is_readable = 0;

			/* the manual says we have to call SSL_read with the same arguments next time.
			 * we ignore this restriction; no one has complained about it in 1.5 yet, so it probably works anyway.
			 */

			return 0;
		case SSL_ERROR_SYSCALL:
			/**
			 * man SSL_get_error()
			 *
			 * SSL_ERROR_SYSCALL
			 *   Some I/O error occurred.  The OpenSSL error queue may contain more
			 *   information on the error.  If the error queue is empty (i.e.
			 *   ERR_get_error() returns 0), ret can be used to find out more about
			 *   the error: If ret == 0, an EOF was observed that violates the
			 *   protocol.  If ret == -1, the underlying BIO reported an I/O error
			 *   (for socket I/O on Unix systems, consult errno for details).
			 *
			 */
			while((ssl_err = ERR_get_error())) {
				/* get all errors from the error-queue */
				log_error_write(srv, __FILE__, __LINE__, "sds", "SSL:",
						r, ERR_error_string(ssl_err, NULL));
			}

			switch(oerrno) {
			default:
				log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL:",
						len, r, oerrno,
						strerror(oerrno));
				break;
			}

			break;
		case SSL_ERROR_ZERO_RETURN:
			/* clean shutdown on the remote side */

			if (r == 0) {
				/* FIXME: later */
			}

			/* fall thourgh */
		default:
			while((ssl_err = ERR_get_error())) {
				switch (ERR_GET_REASON(ssl_err)) {
				case SSL_R_SSL_HANDSHAKE_FAILURE:
				case SSL_R_TLSV1_ALERT_UNKNOWN_CA:
				case SSL_R_SSLV3_ALERT_CERTIFICATE_UNKNOWN:
				case SSL_R_SSLV3_ALERT_BAD_CERTIFICATE:
					if (!con->conf.log_ssl_noise) continue;
					break;
				default:
					break;
				}
				/* get all errors from the error-queue */
				log_error_write(srv, __FILE__, __LINE__, "sds", "SSL:",
				                r, ERR_error_string(ssl_err, NULL));
			}
			break;
		}

		connection_set_state(srv, con, CON_STATE_ERROR);

		return -1;
	} else if (len == 0) {
		con->is_readable = 0;
		/* the other end close the connection -> KEEP-ALIVE */

		return -2;
	}

	return 0;
#else
	UNUSED(srv);
	UNUSED(con);
	return -1;
#endif
}

static int connection_handle_read(server *srv, connection *con) {
	int len;
	buffer *b;
	int toread, read_offset;

	if (con->conf.is_ssl) {
		return connection_handle_read_ssl(srv, con);
	}

	b = (NULL != con->read_queue->last) ? con->read_queue->last->mem : NULL;

	/* default size for chunks is 4kb; only use bigger chunks if FIONREAD tells
	 *  us more than 4kb is available
	 * if FIONREAD doesn't signal a big chunk we fill the previous buffer
	 *  if it has >= 1kb free
	 */
#if defined(__WIN32)
	if (NULL == b || b->size - b->used < 1024) {
		b = chunkqueue_get_append_buffer(con->read_queue);
		buffer_prepare_copy(b, 4 * 1024);
	}

	read_offset = (b->used == 0) ? 0 : b->used - 1;
	len = recv(con->fd, b->ptr + read_offset, b->size - 1 - read_offset, 0);
#else
	if (ioctl(con->fd, FIONREAD, &toread) || toread == 0 || toread <= 4*1024) {
		if (NULL == b || b->size - b->used < 1024) {
			b = chunkqueue_get_append_buffer(con->read_queue);
			buffer_prepare_copy(b, 4 * 1024);
		}
	} else {
		if (toread > MAX_READ_LIMIT) toread = MAX_READ_LIMIT;
		b = chunkqueue_get_append_buffer(con->read_queue);
		buffer_prepare_copy(b, toread + 1);
	}

	read_offset = (b->used == 0) ? 0 : b->used - 1;
	len = read(con->fd, b->ptr + read_offset, b->size - 1 - read_offset);
#endif

	if (len < 0) {
		con->is_readable = 0;

		if (errno == EAGAIN) return 0;
		if (errno == EINTR) {
			/* we have been interrupted before we could read */
			con->is_readable = 1;
			return 0;
		}

		if (errno != ECONNRESET) {
			/* expected for keep-alive */
			log_error_write(srv, __FILE__, __LINE__, "ssd", "connection closed - read failed: ", strerror(errno), errno);
		}

		connection_set_state(srv, con, CON_STATE_ERROR);

		return -1;
	} else if (len == 0) {
		con->is_readable = 0;
		/* the other end close the connection -> KEEP-ALIVE */

		/* pipelining */

		return -2;
	} else if ((size_t)len < b->size - 1) {
		/* we got less then expected, wait for the next fd-event */

		con->is_readable = 0;
	}

	if (b->used > 0) b->used--;
	b->used += len;
	b->ptr[b->used++] = '\0';

	con->bytes_read += len;
#if 0
	dump_packet(b->ptr, len);
#endif

	return 0;
}

static int connection_handle_write_prepare(server *srv, connection *con) {
	if (con->mode == DIRECT) {
		/* static files */
		switch(con->request.http_method) {
		case HTTP_METHOD_GET:
		case HTTP_METHOD_POST:
		case HTTP_METHOD_HEAD:
		case HTTP_METHOD_PUT:
		case HTTP_METHOD_MKCOL:
		case HTTP_METHOD_DELETE:
		case HTTP_METHOD_COPY:
		case HTTP_METHOD_MOVE:
		case HTTP_METHOD_PROPFIND:
		case HTTP_METHOD_PROPPATCH:
		case HTTP_METHOD_LOCK:
		case HTTP_METHOD_UNLOCK:
			break;
		case HTTP_METHOD_OPTIONS:
			/*
			 * 400 is coming from the request-parser BEFORE uri.path is set
			 * 403 is from the response handler when noone else catched it
			 *
			 * */
			if ((!con->http_status || con->http_status == 200) && con->uri.path->used &&
			    con->uri.path->ptr[0] != '*') {
				response_header_insert(srv, con, CONST_STR_LEN("Allow"), CONST_STR_LEN("OPTIONS, GET, HEAD, POST"));

				con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
				con->parsed_response &= ~HTTP_CONTENT_LENGTH;

				con->http_status = 200;
				con->file_finished = 1;

				chunkqueue_reset(con->write_queue);
			}
			break;
		default:
			switch(con->http_status) {
			case 400: /* bad request */
			case 414: /* overload request header */
			case 505: /* unknown protocol */
			case 207: /* this was webdav */
				break;
			default:
				con->http_status = 501;
				break;
			}
			break;
		}
	}

	if (con->http_status == 0) {
		con->http_status = 403;
	}

	switch(con->http_status) {
	case 204: /* class: header only */
	case 205:
	case 304:
		/* disable chunked encoding again as we have no body */
		con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
		con->parsed_response &= ~HTTP_CONTENT_LENGTH;
		chunkqueue_reset(con->write_queue);

		con->file_finished = 1;
		break;
	default: /* class: header + body */
		if (con->mode != DIRECT) break;

		/* only custom body for 4xx and 5xx */
		if (con->http_status < 400 || con->http_status >= 600) break;

		con->file_finished = 0;

		buffer_reset(con->physical.path);

		/* try to send static errorfile */
		if (!buffer_is_empty(con->conf.errorfile_prefix)) {
			stat_cache_entry *sce = NULL;

			buffer_copy_string_buffer(con->physical.path, con->conf.errorfile_prefix);
			buffer_append_long(con->physical.path, con->http_status);
			buffer_append_string_len(con->physical.path, CONST_STR_LEN(".html"));

			if (HANDLER_ERROR != stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
				con->file_finished = 1;

				http_chunk_append_file(srv, con, con->physical.path, 0, sce->st.st_size);
				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_BUF_LEN(sce->content_type));
			}
		}

		if (!con->file_finished) {
			buffer *b;

			buffer_reset(con->physical.path);

			con->file_finished = 1;
			b = chunkqueue_get_append_buffer(con->write_queue);

			/* build default error-page */
			buffer_copy_string_len(b, CONST_STR_LEN(
					   "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n"
					   "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"\n"
					   "         \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
					   "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
					   " <head>\n"
					   "  <title>"));
			buffer_append_long(b, con->http_status);
			buffer_append_string_len(b, CONST_STR_LEN(" - "));
			buffer_append_string(b, get_http_status_name(con->http_status));

			buffer_append_string_len(b, CONST_STR_LEN(
					     "</title>\n"
					     " </head>\n"
					     " <body>\n"
					     "  <h1>"));
			buffer_append_long(b, con->http_status);
			buffer_append_string_len(b, CONST_STR_LEN(" - "));
			buffer_append_string(b, get_http_status_name(con->http_status));

			buffer_append_string_len(b, CONST_STR_LEN("</h1>\n"
					     " </body>\n"
					     "</html>\n"
					     ));

			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("text/html"));
		}
		break;
	}

	if (con->file_finished) {
		/* we have all the content and chunked encoding is not used, set a content-length */

		if ((!(con->parsed_response & HTTP_CONTENT_LENGTH)) &&
		    (con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) == 0) {
			off_t qlen = chunkqueue_length(con->write_queue);

			/**
			 * The Content-Length header only can be sent if we have content:
			 * - HEAD doesn't have a content-body (but have a content-length)
			 * - 1xx, 204 and 304 don't have a content-body (RFC 2616 Section 4.3)
			 *
			 * Otherwise generate a Content-Length header as chunked encoding is not 
			 * available
			 */
			if ((con->http_status >= 100 && con->http_status < 200) ||
			    con->http_status == 204 ||
			    con->http_status == 304) {
				data_string *ds;
				/* no Content-Body, no Content-Length */
				if (NULL != (ds = (data_string*) array_get_element(con->response.headers, "Content-Length"))) {
					buffer_reset(ds->value); /* Headers with empty values are ignored for output */
				}
			} else if (qlen > 0 || con->request.http_method != HTTP_METHOD_HEAD) {
				/* qlen = 0 is important for Redirects (301, ...) as they MAY have
				 * a content. Browsers are waiting for a Content otherwise
				 */
				buffer_copy_off_t(srv->tmp_buf, qlen);

				response_header_overwrite(srv, con, CONST_STR_LEN("Content-Length"), CONST_BUF_LEN(srv->tmp_buf));
			}
		}
	} else {
		/**
		 * the file isn't finished yet, but we have all headers
		 *
		 * to get keep-alive we either need:
		 * - Content-Length: ... (HTTP/1.0 and HTTP/1.0) or
		 * - Transfer-Encoding: chunked (HTTP/1.1)
		 */

		if (((con->parsed_response & HTTP_CONTENT_LENGTH) == 0) &&
		    ((con->response.transfer_encoding & HTTP_TRANSFER_ENCODING_CHUNKED) == 0)) {
			con->keep_alive = 0;
		}

		/**
		 * if the backend sent a Connection: close, follow the wish
		 *
		 * NOTE: if the backend sent Connection: Keep-Alive, but no Content-Length, we
		 * will close the connection. That's fine. We can always decide the close 
		 * the connection
		 *
		 * FIXME: to be nice we should remove the Connection: ... 
		 */
		if (con->parsed_response & HTTP_CONNECTION) {
			/* a subrequest disable keep-alive although the client wanted it */
			if (con->keep_alive && !con->response.keep_alive) {
				con->keep_alive = 0;
			}
		}
	}

	if (con->request.http_method == HTTP_METHOD_HEAD) {
		/**
		 * a HEAD request has the same as a GET 
		 * without the content
		 */
		con->file_finished = 1;

		chunkqueue_reset(con->write_queue);
		con->response.transfer_encoding &= ~HTTP_TRANSFER_ENCODING_CHUNKED;
	}

	http_response_write_header(srv, con);

	return 0;
}

static int connection_handle_write(server *srv, connection *con) {
	switch(network_write_chunkqueue(srv, con, con->write_queue)) {
	case 0:
		if (con->file_finished) {
			connection_set_state(srv, con, CON_STATE_RESPONSE_END);
			joblist_append(srv, con);
		}
		break;
	case -1: /* error on our side */
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"connection closed: write failed on fd", con->fd);
		connection_set_state(srv, con, CON_STATE_ERROR);
		joblist_append(srv, con);
		break;
	case -2: /* remote close */
		connection_set_state(srv, con, CON_STATE_ERROR);
		joblist_append(srv, con);
		break;
	case 1:
		con->is_writable = 0;

		/* not finished yet -> WRITE */
		break;
	}

	return 0;
}



connection *connection_init(server *srv) {
	connection *con;

	UNUSED(srv);

	con = calloc(1, sizeof(*con));

	con->fd = 0;
	con->ndx = -1;
	con->fde_ndx = -1;
	con->bytes_written = 0;
	con->bytes_read = 0;
	con->bytes_header = 0;
	con->loops_per_request = 0;

#define CLEAN(x) \
	con->x = buffer_init();

	CLEAN(request.uri);
	CLEAN(request.request_line);
	CLEAN(request.request);
	CLEAN(request.pathinfo);

	CLEAN(request.orig_uri);

	CLEAN(uri.scheme);
	CLEAN(uri.authority);
	CLEAN(uri.path);
	CLEAN(uri.path_raw);
	CLEAN(uri.query);

	CLEAN(physical.doc_root);
	CLEAN(physical.path);
	CLEAN(physical.basedir);
	CLEAN(physical.rel_path);
	CLEAN(physical.etag);
	CLEAN(parse_request);

	CLEAN(authed_user);
	CLEAN(server_name);
	CLEAN(error_handler);
	CLEAN(dst_addr_buf);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
	CLEAN(tlsext_server_name);
#endif

#undef CLEAN
	con->write_queue = chunkqueue_init();
	con->read_queue = chunkqueue_init();
	con->request_content_queue = chunkqueue_init();
	chunkqueue_set_tempdirs(con->request_content_queue, srv->srvconf.upload_tempdirs);

	con->request.headers      = array_init();
	con->response.headers     = array_init();
	con->environment     = array_init();

	/* init plugin specific connection structures */

	con->plugin_ctx = calloc(1, (srv->plugins.used + 1) * sizeof(void *));

	con->cond_cache = calloc(srv->config_context->used, sizeof(cond_cache_t));
	config_setup_connection(srv, con);

	return con;
}

void connections_free(server *srv) {
	connections *conns = srv->conns;
	size_t i;

	for (i = 0; i < conns->size; i++) {
		connection *con = conns->ptr[i];

		connection_reset(srv, con);

		chunkqueue_free(con->write_queue);
		chunkqueue_free(con->read_queue);
		chunkqueue_free(con->request_content_queue);
		array_free(con->request.headers);
		array_free(con->response.headers);
		array_free(con->environment);

#define CLEAN(x) \
	buffer_free(con->x);

		CLEAN(request.uri);
		CLEAN(request.request_line);
		CLEAN(request.request);
		CLEAN(request.pathinfo);

		CLEAN(request.orig_uri);

		CLEAN(uri.scheme);
		CLEAN(uri.authority);
		CLEAN(uri.path);
		CLEAN(uri.path_raw);
		CLEAN(uri.query);

		CLEAN(physical.doc_root);
		CLEAN(physical.path);
		CLEAN(physical.basedir);
		CLEAN(physical.etag);
		CLEAN(physical.rel_path);
		CLEAN(parse_request);

		CLEAN(authed_user);
		CLEAN(server_name);
		CLEAN(error_handler);
		CLEAN(dst_addr_buf);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
		CLEAN(tlsext_server_name);
#endif
#undef CLEAN
		free(con->plugin_ctx);
		free(con->cond_cache);

		free(con);
	}

	free(conns->ptr);
}


int connection_reset(server *srv, connection *con) {
	size_t i;

	plugins_call_connection_reset(srv, con);

	con->is_readable = 1;
	con->is_writable = 1;
	con->http_status = 0;
	con->file_finished = 0;
	con->file_started = 0;
	con->got_response = 0;

	con->parsed_response = 0;

	con->bytes_written = 0;
	con->bytes_written_cur_second = 0;
	con->bytes_read = 0;
	con->bytes_header = 0;
	con->loops_per_request = 0;

	con->request.http_method = HTTP_METHOD_UNSET;
	con->request.http_version = HTTP_VERSION_UNSET;

	con->request.http_if_modified_since = NULL;
	con->request.http_if_none_match = NULL;

	con->response.keep_alive = 0;
	con->response.content_length = -1;
	con->response.transfer_encoding = 0;

	con->mode = DIRECT;

	/* Destroy the chunk decoder if it is still persistent */
	if (con->chunk_decoder != NULL) {
		chunk_decoder_free(con->chunk_decoder);
		con->chunk_decoder = NULL;
	}


#define CLEAN(x) \
	if (con->x) buffer_reset(con->x);

	CLEAN(request.uri);
	CLEAN(request.request_line);
	CLEAN(request.pathinfo);
	CLEAN(request.request);

	/* CLEAN(request.orig_uri); */

	CLEAN(uri.scheme);
	/* CLEAN(uri.authority); */
	/* CLEAN(uri.path); */
	CLEAN(uri.path_raw);
	/* CLEAN(uri.query); */

	CLEAN(physical.doc_root);
	CLEAN(physical.path);
	CLEAN(physical.basedir);
	CLEAN(physical.rel_path);
	CLEAN(physical.etag);

	CLEAN(parse_request);

	CLEAN(authed_user);
	CLEAN(server_name);
	CLEAN(error_handler);
#if defined USE_OPENSSL && ! defined OPENSSL_NO_TLSEXT
	CLEAN(tlsext_server_name);
#endif
#undef CLEAN

#define CLEAN(x) \
	if (con->x) con->x->used = 0;

#undef CLEAN

#define CLEAN(x) \
		con->request.x = NULL;

	CLEAN(http_host);
	CLEAN(http_range);
	CLEAN(http_content_type);
#undef CLEAN
	con->request.content_length = 0;

	array_reset(con->request.headers);
	array_reset(con->response.headers);
	array_reset(con->environment);

	chunkqueue_reset(con->write_queue);
	chunkqueue_reset(con->request_content_queue);

	/* the plugins should cleanup themself */
	for (i = 0; i < srv->plugins.used; i++) {
		plugin *p = ((plugin **)(srv->plugins.ptr))[i];
		plugin_data *pd = p->data;

		if (!pd) continue;

		if (con->plugin_ctx[pd->id] != NULL) {
			log_error_write(srv, __FILE__, __LINE__, "sb", "missing cleanup in", p->name);
		}

		con->plugin_ctx[pd->id] = NULL;
	}

	/* The cond_cache gets reset in response.c */
	/* config_cond_cache_reset(srv, con); */

	con->header_len = 0;
	con->in_error_handler = 0;

	config_setup_connection(srv, con);

	return 0;
}
//Gemtek add for security usb
int check_security_usbfun(connection *con) //0:stop 1:pass 2:login pass
{
		//system("security_usb_login.sh check");
		if(!strcmp(nvram_safe_get("current_security_usb_in"),"0"))
		{
			return 1;
		}
		else
		{
			char str[INET6_ADDRSTRLEN];
			FILE *fp=NULL;
			char buff[1024]={0};
			char access_ip[25+1]={0};
			int pass=0;
			inet_ntop(AF_INET6, &(con->dst_addr.ipv6.sin6_addr), str, INET6_ADDRSTRLEN);
			sprintf(access_ip,"%s",strrchr(str,':')+1);
			if(access("/tmp/security_usb_login_list", F_OK) == 0)
    	{
        fp = fopen("/tmp/security_usb_login_list", "r");
        if(fp != NULL)
        {
            memset(buff, 0x00, sizeof(buff));
            while(fgets(buff, 1024, fp) != NULL)
            {
                if (strncasecmp(buff,access_ip,strlen(access_ip))==0 && buff[strlen(access_ip)]==' ')
                {
                	fclose(fp);
                  switch(con->state) 
                  {
                  	case CON_STATE_READ: //copy in device
                    	con->security_usb_count=0;
                    	memset(buff, 0x00, sizeof(buff));
                    	sprintf(buff,"security_usb_login.sh time %s &",access_ip);
    									system(buff);
                  		break;
                  	case CON_STATE_READ_POST: //client upload
                  		con->security_usb_count++;
                  		if(con->security_usb_count>1000)
                    	{
                    		con->security_usb_count=0;
                    		memset(buff, 0x00, sizeof(buff));
                    		sprintf(buff,"security_usb_login.sh time %s &",access_ip);
    										system(buff);
    									}
                  		break;
                  	case CON_STATE_WRITE: //download
                  		con->security_usb_count++;
                  		if(con->security_usb_count>100)
                    	{
                    		con->security_usb_count=0;
                    		memset(buff, 0x00, sizeof(buff));
                    		sprintf(buff,"security_usb_login.sh time %s &",access_ip);
    										system(buff);
    									}
                  		break;
                	}
                  return 2;
                }
            }
            fclose(fp);
        }
    	}
		}
		return 0;
}
//Gemtek add end

/**
 * handle all header and content read
 *
 * we get called by the state-engine and by the fdevent-handler
 */
static int connection_handle_read_state(server *srv, connection *con)  {
	connection_state_t ostate = con->state;
	chunk *c, *last_chunk;
	off_t last_offset;
	chunkqueue *cq = con->read_queue;
	chunkqueue *dst_cq = con->request_content_queue;
	lp_chunk_decoder dec = NULL;
	int is_closed = 0; /* the connection got closed, if we don't have a complete header, -> error */

	int ofd;
	buffer *url;
	char buf[2048] = {0};

	if (con->is_readable) {
		con->read_idle_ts = srv->cur_ts;

		switch(connection_handle_read(srv, con)) {
		case -1:
			return -1;
		case -2:
			is_closed = 1;
			break;
		default:
			break;
		}
	}

	/* the last chunk might be empty */
	for (c = cq->first; c;) {
		if (cq->first == c && c->mem->used == 0) {
			/* the first node is empty */
			/* ... and it is empty, move it to unused */

			cq->first = c->next;
			if (cq->first == NULL) cq->last = NULL;

			c->next = cq->unused;
			cq->unused = c;
			cq->unused_chunks++;

			c = cq->first;
		} else if (c->next && c->next->mem->used == 0) {
			chunk *fc;
			/* next node is the last one */
			/* ... and it is empty, move it to unused */

			fc = c->next;
			c->next = fc->next;

			fc->next = cq->unused;
			cq->unused = fc;
			cq->unused_chunks++;

			/* the last node was empty */
			if (c->next == NULL) {
				cq->last = c;
			}

			c = c->next;
		} else {
			c = c->next;
		}
	}

	//Gemtek add for security usb
	/*
	if(con && con->physical.path && con->physical.path->ptr && !strncasecmp(con->physical.path->ptr,"/media/USB",10))
  {
  	check_security_usbfun(con);
  }
  */  
	//Gemtek add end

	/* we might have got several packets at once
	 */

	switch(ostate) {
	case CON_STATE_READ:
		/* if there is a \r\n\r\n in the chunkqueue
		 *
		 * scan the chunk-queue twice
		 * 1. to find the \r\n\r\n
		 * 2. to copy the header-packet
		 *
		 */

		last_chunk = NULL;
		last_offset = 0;

		for (c = cq->first; c; c = c->next) {
			buffer b;
			size_t i;

			b.ptr = c->mem->ptr + c->offset;
			b.used = c->mem->used - c->offset;
			if (b.used > 0) b.used--; /* buffer "used" includes terminating zero */

			for (i = 0; i < b.used; i++) {
				char ch = b.ptr[i];

				if ('\r' == ch) {
					/* chec if \n\r\n follows */
					size_t j = i+1;
					chunk *cc = c;
					const char header_end[] = "\r\n\r\n";
					int header_end_match_pos = 1;

					for ( ; cc; cc = cc->next, j = 0 ) {
						buffer bb;
						bb.ptr = cc->mem->ptr + cc->offset;
						bb.used = cc->mem->used - cc->offset;
						if (bb.used > 0) bb.used--; /* buffer "used" includes terminating zero */

						for ( ; j < bb.used; j++) {
							ch = bb.ptr[j];

							if (ch == header_end[header_end_match_pos]) {
								header_end_match_pos++;
								if (4 == header_end_match_pos) {
									last_chunk = cc;
									last_offset = j+1;
									goto found_header_end;
								}
							} else {
								goto reset_search;
							}
						}
					}
				}
reset_search: ;
			}
		}
found_header_end:

		/* found */
		if (last_chunk) {
			buffer_reset(con->request.request);

			for (c = cq->first; c; c = c->next) {
				buffer b;

				b.ptr = c->mem->ptr + c->offset;
				b.used = c->mem->used - c->offset;

				if (c == last_chunk) {
					b.used = last_offset + 1;
				}

				buffer_append_string_buffer(con->request.request, &b);

				if (c == last_chunk) {
					c->offset += last_offset;

					break;
				} else {
					/* the whole packet was copied */
					c->offset = c->mem->used - 1;
				}
			}

			connection_set_state(srv, con, CON_STATE_REQUEST_END);
		} else if (chunkqueue_length(cq) > 64 * 1024) {
			log_error_write(srv, __FILE__, __LINE__, "s", "oversized request-header -> sending Status 414");

			con->http_status = 414; /* Request-URI too large */
			con->keep_alive = 0;
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		}
		break;
	case CON_STATE_READ_POST:
		for (c = cq->first; c && (dst_cq->bytes_in != (off_t)con->request.content_length); c = c->next) {
			off_t weWant, weHave, toRead;

			weWant = con->request.content_length - dst_cq->bytes_in;

			assert(c->mem->used);

			weHave = c->mem->used - c->offset - 1;

			toRead = weHave > weWant ? weWant : weHave;

			/* the new way, copy everything into a chunkqueue whcih might use tempfiles */
			if (con->request.content_length > 64 * 1024 || con->request.http_method == HTTP_METHOD_PUT) { //Gemtek add
				chunk *dst_c = NULL;
				/* copy everything to max 1Mb sized tempfiles */

				/*
				 * if the last chunk is
				 * - smaller than 1Mb (size < 1Mb)
				 * - not read yet (offset == 0)
				 * -> append to it
				 * otherwise
				 * -> create a new chunk
				 *
				 * */

				if (dst_cq->last &&
				    dst_cq->last->type == FILE_CHUNK &&
				    dst_cq->last->file.is_temp &&
				    dst_cq->last->offset == 0) {
					/* ok, take the last chunk for our job */

			 		if (dst_cq->last->file.length < 100 * 1024 * 1024) {
						dst_c = dst_cq->last;

						if (dst_c->file.fd == -1) {
							/* this should not happen as we cache the fd, but you never know */
							dst_c->file.fd = open(dst_c->file.name->ptr, O_WRONLY | O_APPEND);
#ifdef FD_CLOEXEC
							fcntl(dst_c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
						}
					} else {
						/* the chunk is too large now, close it */
						dst_c = dst_cq->last;

						if (dst_c->file.fd != -1) {
							close(dst_c->file.fd);
							dst_c->file.fd = -1;
						}
						dst_c = chunkqueue_get_append_tempfile(dst_cq);
					}
				} else {
					dst_c = chunkqueue_get_append_tempfile(dst_cq);
				}

				/* we have a chunk, let's write to it */

				if (dst_c->file.fd == -1) {
					/* we don't have file to write to,
					 * EACCES might be one reason.
					 *
					 * Instead of sending 500 we send 413 and say the request is too large
					 *  */

					log_error_write(srv, __FILE__, __LINE__, "sbs",
							"denying upload as opening to temp-file for upload failed:",
							dst_c->file.name, strerror(errno));

					con->http_status = 413; /* Request-Entity too large */
					con->keep_alive = 0;
					connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

					break;
				}
				//Gemtek add 
				if(con->request.http_method == HTTP_METHOD_PUT)
				{
					memset(buf,0,sizeof(buf));
					sprintf(buf,"/media%s",con->request.uri->ptr);					
					
					url = buffer_init_string(buf);
					buffer_urldecode_path(url);
					
					if(dst_cq->bytes_in == 0)
					{
						ofd = open(url->ptr,O_WRONLY | O_CREAT | O_TRUNC);
					}
					else
					{
						ofd = open(url->ptr,O_WRONLY | O_APPEND | O_CREAT);
					}
					
					if (toRead != write(ofd, c->mem->ptr + c->offset, toRead)) {
						/* write failed for some reason ... disk full ? */
						log_error_write(srv, __FILE__, __LINE__, "sbs",
								"denying upload as writing to file failed:",
								dst_c->file.name, strerror(errno));

						con->http_status = 413; /* Request-Entity too large */
						con->keep_alive = 0;
						connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

						close(dst_c->file.fd);
						dst_c->file.fd = -1;
						close(ofd);
						break;
					}
					buffer_free(url);
					close(ofd);									
				}
				else
				{
					if (toRead != write(dst_c->file.fd, c->mem->ptr + c->offset, toRead)) {
						/* write failed for some reason ... disk full ? */
						log_error_write(srv, __FILE__, __LINE__, "sbs",
								"denying upload as writing to file failed:",
								dst_c->file.name, strerror(errno));

						con->http_status = 413; /* Request-Entity too large */
						con->keep_alive = 0;
						connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

						close(dst_c->file.fd);
						dst_c->file.fd = -1;

						break;
					}
				}
				//Gemtek end

				dst_c->file.length += toRead;

				if (dst_cq->bytes_in + toRead == (off_t)con->request.content_length) {
					/* we read everything, close the chunk */
					close(dst_c->file.fd);
					dst_c->file.fd = -1;
				}
			} else {
				buffer *b;

				if (dst_cq->last &&
				    dst_cq->last->type == MEM_CHUNK) {
					b = dst_cq->last->mem;
				} else {
					b = chunkqueue_get_append_buffer(dst_cq);
					/* prepare buffer size for remaining POST data; is < 64kb */
					buffer_prepare_copy(b, con->request.content_length - dst_cq->bytes_in + 1);
				}
				buffer_append_string_len(b, c->mem->ptr + c->offset, toRead);
			}

			c->offset += toRead;
			dst_cq->bytes_in += toRead;
		}

		/* Content is ready */
		if (dst_cq->bytes_in == (off_t)con->request.content_length) {
			connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
		}

		break;

#define FREE_AND_QUIT {\
chunk_decoder_free(dec);\
con->chunk_decoder = NULL;\
dec = NULL;\
connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);\
}

	/* Added by Andreas Stockel for "transfer-encoding: chunked" support */
	case CON_STATE_READ_CHUNKED:
		/* Check whether the chunk decoder has been created, if not, create it */		
		
		if (con->chunk_decoder == NULL)	{
			dec = chunk_decoder_create();
			con->chunk_decoder = dec;
		} else {
			dec = (lp_chunk_decoder)con->chunk_decoder;
		}

		for (c = cq->first; c != NULL; c = c->next) {
			/* Reserve some memory which should contain the decoded data */
			int toRead = c->mem->used - c->offset - 1;
			char *target = malloc(toRead);
			memset(target, 0, toRead);

			size_t decoded_size = 0;

			int res = chunk_decoder_work(dec, c->mem->ptr + c->offset, target, toRead,
				&decoded_size);
			if (res >= 0)
			{
				/* the new way, copy everything into a chunkqueue whcih might use tempfiles */
				chunk *dst_c = NULL;

				/* copy everything to max 1Mb sized tempfiles */

				/*
				 * if the last chunk is
				 * - smaller than 1Mb (size < 1Mb)
				 * - not read yet (offset == 0)
				 * -> append to it
				 * otherwise
				 * -> create a new chunk
				 *
				 * */

				if (dst_cq->last &&
					dst_cq->last->type == FILE_CHUNK &&
					dst_cq->last->file.is_temp &&
					dst_cq->last->offset == 0) {
					/* ok, take the last chunk for our job */

			 		if (dst_cq->last->file.length < 100 * 1024 * 1024) {
						dst_c = dst_cq->last;

						if (dst_c->file.fd == -1) {
							/* this should not happen as we cache the fd, but you never know */
							dst_c->file.fd = open(dst_c->file.name->ptr, O_WRONLY | O_APPEND);
#ifdef FD_CLOEXEC
							fcntl(dst_c->file.fd, F_SETFD, FD_CLOEXEC);
#endif
						}
					} else {
						/* the chunk is too large now, close it */
						dst_c = dst_cq->last;

						if (dst_c->file.fd != -1) {
							close(dst_c->file.fd);
							dst_c->file.fd = -1;
						}
						dst_c = chunkqueue_get_append_tempfile(dst_cq);
					}
				} else {
					dst_c = chunkqueue_get_append_tempfile(dst_cq);
				}

				/* we have a chunk, let's write to it */

				if (dst_c->file.fd == -1) {
					/* we don't have file to write to,
					 * EACCES might be one reason.
					 *
					 * Instead of sending 500 we send 413 and say the request is too large
					 *  */

					log_error_write(srv, __FILE__, __LINE__, "sbs",
							"denying upload as opening to temp-file for upload failed:",
							dst_c->file.name, strerror(errno));

					con->http_status = 413; /* Request-Entity too large */
					con->keep_alive = 0;

					FREE_AND_QUIT;
					free(target);

					break;
				}
				
				//Gemtek
					memset(buf,0,sizeof(buf));
					sprintf(buf,"/media%s",con->request.uri->ptr);					
					
					url = buffer_init_string(buf);
					buffer_urldecode_path(url);
					
					if(dst_cq->bytes_in == 0)
					{
						ofd = open(url->ptr,O_WRONLY | O_CREAT | O_TRUNC);
					}
					else
					{
						ofd = open(url->ptr,O_WRONLY | O_APPEND | O_CREAT);
					}
					
					if ((int)decoded_size != write(ofd, target, decoded_size)) {
						/* write failed for some reason ... disk full ? */
						log_error_write(srv, __FILE__, __LINE__, "sbs",
								"denying upload as writing to file failed:",
								dst_c->file.name, strerror(errno));

						con->http_status = 413; /* Request-Entity too large */
						con->keep_alive = 0;

						close(dst_c->file.fd);
						dst_c->file.fd = -1;
						FREE_AND_QUIT;
						free(target);
						close(ofd);
						break;
					}
					buffer_free(url);
					close(ofd);
				
				//Gemtek end
				
				/*
				if ((int)decoded_size != write(dst_c->file.fd, target, decoded_size)) {
					// write failed for some reason ... disk full ? 
					log_error_write(srv, __FILE__, __LINE__, "sbs",
							"denying upload as writing to file failed:",
							dst_c->file.name, strerror(errno));

					con->http_status = 413; // Request-Entity too large 
					con->keep_alive = 0;

					close(dst_c->file.fd);
					dst_c->file.fd = -1;

					FREE_AND_QUIT;
					free(target);

					break;
				}
				*/

				if (res == 1) {
					/* we read everything, close the chunk */
					close(dst_c->file.fd);
					dst_c->file.fd = -1;
				}

				dst_c->file.length += decoded_size;
				dst_cq->bytes_in += decoded_size;
				c->offset += toRead;
			}
			free(target);
		}

		/* Check whether the file upload limit has been exhausted */

		/* divide by 1024 as srvconf.max_request_size is in kBytes */
		if (dec != NULL) {
			if (srv->srvconf.max_request_size != 0 &&
				(dec->size >> 10) > srv->srvconf.max_request_size) {
				con->http_status = 413;
				con->keep_alive = 0;

				log_error_write(srv, __FILE__, __LINE__, "sos",
						"request-size too long:", (off_t) dec->size, "-> 413");

				FREE_AND_QUIT;
			}
		}

		if (dec != NULL) {
			if (dec->state >= CHUNK_DECODER_STATE_END) {
				switch (dec->state) {
					case CHUNK_DECODER_STATE_END:
						/* Set the content length to allow the plugins to treat this request as
						   if it had been send with content_lenght */
						con->request.content_length = dec->size;

						break;
					case CHUNK_DECODER_STATE_ERROR:
						/* Chunk decoding failed. Return an error 400, bad request */
						log_error_write(srv, __FILE__, __LINE__, "s",
							"chunked transfer-encoding decoding failed.");

						con->http_status = 400;
						con->keep_alive = 0;
						break;
				}
				FREE_AND_QUIT;
			}
		}

		break;


	default:
		break;
	}

	/* the connection got closed and we didn't got enough data to leave one of the READ states
	 * the only way is to leave here */
	if (is_closed && ostate == con->state) {
		connection_set_state(srv, con, CON_STATE_ERROR);
	}

	chunkqueue_remove_finished_chunks(cq);

	return 0;
}

static handler_t connection_handle_fdevent(void *s, void *context, int revents) {
	server     *srv = (server *)s;
	connection *con = context;

	joblist_append(srv, con);

	if (revents & FDEVENT_IN) {
		con->is_readable = 1;
#if 0
		log_error_write(srv, __FILE__, __LINE__, "sd", "read-wait - done", con->fd);
#endif
	}
	if (revents & FDEVENT_OUT) {
		con->is_writable = 1;
		/* we don't need the event twice */
	}


	if (revents & ~(FDEVENT_IN | FDEVENT_OUT)) {
		/* looks like an error */

		/* FIXME: revents = 0x19 still means that we should read from the queue */
		if (revents & FDEVENT_HUP) {
			if (con->state == CON_STATE_CLOSE) {
				con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
			} else {
				/* sigio reports the wrong event here
				 *
				 * there was no HUP at all
				 */
#ifdef USE_LINUX_SIGIO
				if (srv->ev->in_sigio == 1) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
						"connection closed: poll() -> HUP", con->fd);
				} else {
					connection_set_state(srv, con, CON_STATE_ERROR);
				}
#else
				connection_set_state(srv, con, CON_STATE_ERROR);
#endif

			}
		} else if (revents & FDEVENT_ERR) {
#ifndef USE_LINUX_SIGIO
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"connection closed: poll() -> ERR", con->fd);
#endif
			connection_set_state(srv, con, CON_STATE_ERROR);
		} else {
			log_error_write(srv, __FILE__, __LINE__, "sd",
					"connection closed: poll() -> ???", revents);
		}
	}

	if (con->state == CON_STATE_READ ||
	    con->state == CON_STATE_READ_POST) {
		connection_handle_read_state(srv, con);
	}

	if (con->state == CON_STATE_WRITE &&
	    !chunkqueue_is_empty(con->write_queue) &&
	    con->is_writable) {

		if (-1 == connection_handle_write(srv, con)) {
			connection_set_state(srv, con, CON_STATE_ERROR);

			log_error_write(srv, __FILE__, __LINE__, "ds",
					con->fd,
					"handle write failed.");
		} else if (con->state == CON_STATE_WRITE) {
			con->write_request_ts = srv->cur_ts;
		}
	}

	if (con->state == CON_STATE_CLOSE) {
		/* flush the read buffers */
		int len;
		char buf[1024];

		len = read(con->fd, buf, sizeof(buf));
		if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR) ) {
			con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
		}
	}

	return HANDLER_FINISHED;
}


connection *connection_accept(server *srv, server_socket *srv_socket) {
	/* accept everything */

	/* search an empty place */
	int cnt;
	sock_addr cnt_addr;
	socklen_t cnt_len;
	/* accept it and register the fd */

	/**
	 * check if we can still open a new connections
	 *
	 * see #1216
	 */

	if (srv->conns->used >= srv->max_conns) {
		return NULL;
	}

	cnt_len = sizeof(cnt_addr);

	if (-1 == (cnt = accept(srv_socket->fd, (struct sockaddr *) &cnt_addr, &cnt_len))) {
		switch (errno) {
		case EAGAIN:
#if EWOULDBLOCK != EAGAIN
		case EWOULDBLOCK:
#endif
		case EINTR:
			/* we were stopped _before_ we had a connection */
		case ECONNABORTED: /* this is a FreeBSD thingy */
			/* we were stopped _after_ we had a connection */
			break;
		case EMFILE:
			/* out of fds */
			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "ssd", "accept failed:", strerror(errno), errno);
		}
		return NULL;
	} else {
		connection *con;

		srv->cur_fds++;

		/* ok, we have the connection, register it */
#if 0
		log_error_write(srv, __FILE__, __LINE__, "sd",
				"appected()", cnt);
#endif
		srv->con_opened++;

		con = connections_get_new_connection(srv);

		con->fd = cnt;
		con->fde_ndx = -1;
#if 0
		gettimeofday(&(con->start_tv), NULL);
#endif
		fdevent_register(srv->ev, con->fd, connection_handle_fdevent, con);

		connection_set_state(srv, con, CON_STATE_REQUEST_START);

		con->connection_start = srv->cur_ts;
		con->dst_addr = cnt_addr;
		buffer_copy_string(con->dst_addr_buf, inet_ntop_cache_get_ip(srv, &(con->dst_addr)));
		con->srv_socket = srv_socket;

		if (-1 == (fdevent_fcntl_set(srv->ev, con->fd))) {
			log_error_write(srv, __FILE__, __LINE__, "ss", "fcntl failed: ", strerror(errno));
			return NULL;
		}
#ifdef USE_OPENSSL
		/* connect FD to SSL */
		if (srv_socket->is_ssl) {
			if (NULL == (con->ssl = SSL_new(srv_socket->ssl_ctx))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));

				return NULL;
			}

#ifndef OPENSSL_NO_TLSEXT
			SSL_set_app_data(con->ssl, con);
#endif
			SSL_set_accept_state(con->ssl);
			con->conf.is_ssl=1;

			if (1 != (SSL_set_fd(con->ssl, cnt))) {
				log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
						ERR_error_string(ERR_get_error(), NULL));
				return NULL;
			}
		}
#endif
		return con;
	}
}


int connection_state_machine(server *srv, connection *con) {
	int done = 0, r;
#ifdef USE_OPENSSL
	server_socket *srv_sock = con->srv_socket;
#endif

	if (srv->srvconf.log_state_handling) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"state at start",
				con->fd,
				connection_get_state(con->state));
	}
	
	//Gemtek add for security usb
	if(con && con->physical.path && con->physical.path->ptr && !strncasecmp(con->physical.path->ptr,"/media/USB",10))
  {
  	int status = check_security_usbfun(con);
  	if(status==0)
  		done=1;
  }
  //Gemtek add for security usb

	while (done == 0) {
		size_t ostate = con->state;

		switch (con->state) {
		case CON_STATE_REQUEST_START: /* transient */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			con->request_start = srv->cur_ts;
			con->read_idle_ts = srv->cur_ts;

			con->request_count++;
			con->loops_per_request = 0;

			connection_set_state(srv, con, CON_STATE_READ);

			/* patch con->conf.is_ssl if the connection is a ssl-socket already */

#ifdef USE_OPENSSL
			con->conf.is_ssl = srv_sock->is_ssl;
#endif

			break;
		case CON_STATE_REQUEST_END: /* transient */
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			buffer_reset(con->uri.authority);
			buffer_reset(con->uri.path);
			buffer_reset(con->uri.query);
			buffer_reset(con->request.orig_uri);

			/* As we've now got three possible results, a "switch" is needed (Andreas Stockel) */
			int res = http_request_parse(srv, con);
			switch (res)
			{
				case 1:
					/* we have to read some data from the POST request */
					connection_set_state(srv, con, CON_STATE_READ_POST);
					break;
				case 2:
					/* we have to read chunk encoded data */
					connection_set_state(srv, con, CON_STATE_READ_CHUNKED);
					break;
				default:
					connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);
			}

			break;
		case CON_STATE_HANDLE_REQUEST:
			/*
			 * the request is parsed
			 *
			 * decided what to do with the request
			 * -
			 *
			 *
			 */

			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			switch (r = http_response_prepare(srv, con)) {
			case HANDLER_FINISHED:
				if (con->mode == DIRECT) {
					if (con->http_status == 404 ||
					    con->http_status == 403) {
						/* 404 error-handler */

						if (con->in_error_handler == 0 &&
						    (!buffer_is_empty(con->conf.error_handler) ||
						     !buffer_is_empty(con->error_handler))) {
							/* call error-handler */

							con->error_handler_saved_status = con->http_status;
							con->http_status = 0;

							if (buffer_is_empty(con->error_handler)) {
								buffer_copy_string_buffer(con->request.uri, con->conf.error_handler);
							} else {
								buffer_copy_string_buffer(con->request.uri, con->error_handler);
							}
							buffer_reset(con->physical.path);

							con->in_error_handler = 1;

							connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

							done = -1;
							break;
						} else if (con->in_error_handler) {
							/* error-handler is a 404 */

							con->http_status = con->error_handler_saved_status;
						}
					} else if (con->in_error_handler) {
						/* error-handler is back and has generated content */
						/* if Status: was set, take it otherwise use 200 */
					}
				}
				if (con->http_status == 0) con->http_status = 200;

				/* we have something to send, go on */
				connection_set_state(srv, con, CON_STATE_RESPONSE_START);
				break;
			case HANDLER_WAIT_FOR_FD:
				srv->want_fds++;

				fdwaitqueue_append(srv, con);

				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

				break;
			case HANDLER_COMEBACK:
				done = -1;
			case HANDLER_WAIT_FOR_EVENT:
				/* come back here */
				connection_set_state(srv, con, CON_STATE_HANDLE_REQUEST);

				break;
			case HANDLER_ERROR:
				/* something went wrong */
				connection_set_state(srv, con, CON_STATE_ERROR);
				break;
			default:
				log_error_write(srv, __FILE__, __LINE__, "sdd", "unknown ret-value: ", con->fd, r);
				break;
			}

			break;
		case CON_STATE_RESPONSE_START:
			/*
			 * the decision is done
			 * - create the HTTP-Response-Header
			 *
			 */

			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			if (-1 == connection_handle_write_prepare(srv, con)) {
				connection_set_state(srv, con, CON_STATE_ERROR);

				break;
			}

			connection_set_state(srv, con, CON_STATE_WRITE);
			break;
		case CON_STATE_RESPONSE_END: /* transient */
			/* log the request */

			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			plugins_call_handle_request_done(srv, con);

			srv->con_written++;

			if (con->keep_alive) {
				connection_set_state(srv, con, CON_STATE_REQUEST_START);

#if 0
				con->request_start = srv->cur_ts;
				con->read_idle_ts = srv->cur_ts;
#endif
			} else {
				switch(r = plugins_call_handle_connection_close(srv, con)) {
				case HANDLER_GO_ON:
				case HANDLER_FINISHED:
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "sd", "unhandling return value", r);
					break;
				}

#ifdef USE_OPENSSL
				if (srv_sock->is_ssl) {
					switch (SSL_shutdown(con->ssl)) {
					case 1:
						/* done */
						break;
					case 0:
						/* wait for fd-event
						 *
						 * FIXME: wait for fdevent and call SSL_shutdown again
						 *
						 */

						break;
					default:
						log_error_write(srv, __FILE__, __LINE__, "ss", "SSL:",
								ERR_error_string(ERR_get_error(), NULL));
					}
				}
#endif
				if ((0 == shutdown(con->fd, SHUT_WR))) {
					con->close_timeout_ts = srv->cur_ts;
					connection_set_state(srv, con, CON_STATE_CLOSE);
				} else {
					connection_close(srv, con);
				}

				srv->con_closed++;
			}

			connection_reset(srv, con);

			break;
		case CON_STATE_CONNECT:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			chunkqueue_reset(con->read_queue);

			con->request_count = 0;

			break;
		case CON_STATE_CLOSE:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			/* we have to do the linger_on_close stuff regardless
			 * of con->keep_alive; even non-keepalive sockets may
			 * still have unread data, and closing before reading
			 * it will make the client not see all our output.
			 */
			{
				int len;
				char buf[1024];

				len = read(con->fd, buf, sizeof(buf));
				if (len == 0 || (len < 0 && errno != EAGAIN && errno != EINTR) ) {
					con->close_timeout_ts = srv->cur_ts - (HTTP_LINGER_TIMEOUT+1);
				}
			}

			if (srv->cur_ts - con->close_timeout_ts > HTTP_LINGER_TIMEOUT) {
				connection_close(srv, con);

				if (srv->srvconf.log_state_handling) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"connection closed for fd", con->fd);
				}
			}

			break;
		case CON_STATE_READ_CHUNKED: /* Added by Andreas Stockel for "transfer-encoding: chunked" support */
		case CON_STATE_READ_POST:
		case CON_STATE_READ:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			connection_handle_read_state(srv, con);
			break;
		case CON_STATE_WRITE:
			if (srv->srvconf.log_state_handling) {
				log_error_write(srv, __FILE__, __LINE__, "sds",
						"state for fd", con->fd, connection_get_state(con->state));
			}

			/* only try to write if we have something in the queue */
			if (!chunkqueue_is_empty(con->write_queue)) {
#if 0
				log_error_write(srv, __FILE__, __LINE__, "dsd",
						con->fd,
						"packets to write:",
						con->write_queue->used);
#endif
			}
			if (!chunkqueue_is_empty(con->write_queue) && con->is_writable) {
				if (-1 == connection_handle_write(srv, con)) {
					log_error_write(srv, __FILE__, __LINE__, "ds",
							con->fd,
							"handle write failed.");
					connection_set_state(srv, con, CON_STATE_ERROR);
				} else if (con->state == CON_STATE_WRITE) {
					con->write_request_ts = srv->cur_ts;
				}
			}

			break;
		case CON_STATE_ERROR: /* transient */

			/* even if the connection was drop we still have to write it to the access log */
			if (con->http_status) {
				plugins_call_handle_request_done(srv, con);
			}
#ifdef USE_OPENSSL
			if (srv_sock->is_ssl) {
				int ret, ssl_r;
				unsigned long err;
				ERR_clear_error();
				switch ((ret = SSL_shutdown(con->ssl))) {
				case 1:
					/* ok */
					break;
				case 0:
					ERR_clear_error();
					if (-1 != (ret = SSL_shutdown(con->ssl))) break;

					/* fall through */
				default:

					switch ((ssl_r = SSL_get_error(con->ssl, ret))) {
					case SSL_ERROR_WANT_WRITE:
					case SSL_ERROR_WANT_READ:
						break;
					case SSL_ERROR_SYSCALL:
						/* perhaps we have error waiting in our error-queue */
						if (0 != (err = ERR_get_error())) {
							do {
								log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
										ssl_r, ret,
										ERR_error_string(err, NULL));
							} while((err = ERR_get_error()));
						} else {
							log_error_write(srv, __FILE__, __LINE__, "sddds", "SSL (error):",
									ssl_r, ret, errno,
									strerror(errno));
						}
	
						break;
					default:
						while((err = ERR_get_error())) {
							log_error_write(srv, __FILE__, __LINE__, "sdds", "SSL:",
									ssl_r, ret,
									ERR_error_string(err, NULL));
						}
	
						break;
					}
				}
			}
			ERR_clear_error();
#endif

			switch(con->mode) {
			case DIRECT:
#if 0
				log_error_write(srv, __FILE__, __LINE__, "sd",
						"emergency exit: direct",
						con->fd);
#endif
				break;
			default:
				switch(r = plugins_call_handle_connection_close(srv, con)) {
				case HANDLER_GO_ON:
				case HANDLER_FINISHED:
					break;
				default:
					log_error_write(srv, __FILE__, __LINE__, "sd", "unhandling return value", r);
					break;
				}
				break;
			}

			connection_reset(srv, con);

			/* close the connection */
			if ((0 == shutdown(con->fd, SHUT_WR))) {
				con->close_timeout_ts = srv->cur_ts;
				connection_set_state(srv, con, CON_STATE_CLOSE);

				if (srv->srvconf.log_state_handling) {
					log_error_write(srv, __FILE__, __LINE__, "sd",
							"shutdown for fd", con->fd);
				}
			} else {
				connection_close(srv, con);
			}

			con->keep_alive = 0;

			srv->con_closed++;

			break;
		default:
			log_error_write(srv, __FILE__, __LINE__, "sdd",
					"unknown state:", con->fd, con->state);

			break;
		}

		if (done == -1) {
			done = 0;
		} else if (ostate == con->state) {
			done = 1;
		}
	}

	if (srv->srvconf.log_state_handling) {
		log_error_write(srv, __FILE__, __LINE__, "sds",
				"state at exit:",
				con->fd,
				connection_get_state(con->state));
	}

	switch(con->state) {
	case CON_STATE_READ_CHUNKED: /* Added by Andreas Stoeckel for "transfer-encoding: chunked" support */
	case CON_STATE_READ_POST:
	case CON_STATE_READ:
	case CON_STATE_CLOSE:
		fdevent_event_add(srv->ev, &(con->fde_ndx), con->fd, FDEVENT_IN);
		break;
	case CON_STATE_WRITE:
		/* request write-fdevent only if we really need it
		 * - if we have data to write
		 * - if the socket is not writable yet
		 */
		if (!chunkqueue_is_empty(con->write_queue) &&
		    (con->is_writable == 0) &&
		    (con->traffic_limit_reached == 0)) {
			fdevent_event_add(srv->ev, &(con->fde_ndx), con->fd, FDEVENT_OUT);
		} else {
			fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
		}
		break;
	default:
		fdevent_event_del(srv->ev, &(con->fde_ndx), con->fd);
		break;
	}

	return 0;
}
