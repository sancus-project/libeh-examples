#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "tcp_echo.h"

#include <eh.h>		/* UNUSED() */
#include <eh_watcher.h>	/* eh_signal_init() */
#include <eh_socket.h>	/* eh_socket_ntop() */
#include <eh_log.h>	/* debugf(), syserrf() */

#include <assert.h>
#include <stdlib.h>

static void echo_on_conn_read(struct eh_connection *, unsigned char *, size_t);
static void echo_on_conn_close(struct eh_connection *);
static bool echo_on_conn_error(struct eh_connection *, enum eh_connection_error);

/*
 * data
 */
static struct ev_signal sig[2];
static struct eh_connection_cb echo_connection_callbacks = {
	.on_read = echo_on_conn_read,
	.on_close = echo_on_conn_close,
	.on_error = echo_on_conn_error,
};

/*
 * helpers
 */
static struct echo_conn *echo_new(int fd)
{
	struct echo_conn *self = malloc(sizeof(*self));
	if (self) {
		struct eh_connection *conn = &self->conn;
		memset(self, '\0', sizeof(*self));

		eh_connection_init(conn, fd,
				   self->read_buffer, sizeof(self->read_buffer),
				   self->write_buffer, sizeof(self->write_buffer));

		conn->cb = &echo_connection_callbacks;
	}
	return self;
}

static inline void echo_free(struct echo_conn *self)
{
	free(self);
}

/*
 * eh_connection callbacks
 */
static void echo_on_conn_close(struct eh_connection *conn)
{
	struct echo_conn *self = (struct echo_conn *)conn;

	debugf("%s: closed", self->name);
	echo_free(self);
}

static bool echo_on_conn_error(struct eh_connection *conn, enum eh_connection_error error)
{
	struct echo_conn *self = (struct echo_conn *)conn;

	switch (error) {
	case EH_CONNECTION_READ_ERROR:
		syserrf("%s: read()", self->name); break;
	case EH_CONNECTION_WRITE_ERROR:
		syserrf("%s: write()", self->name); break;
	case EH_CONNECTION_READ_FULL:
		errf("%s: read buffer is full", self->name); break;
	case EH_CONNECTION_WRITE_FULL:
		errf("%s: write buffer is full", self->name); break;
	case EH_CONNECTION_READ_WATCHER_ERROR:
		errf("%s: read watcher failed", self->name); break;
	case EH_CONNECTION_WRITE_WATCHER_ERROR:
		errf("%s: write watcher failed", self->name); break;
	}
	return true; /* close connection */
}

static void echo_on_conn_read(struct eh_connection *conn, unsigned char *buffer,
			      size_t len)
{
	struct echo_conn *self = (struct echo_conn *)conn;

	debugf("%s: read buffer at %p has %zu bytes", self->name, buffer, len);
	eh_connection_write(conn, buffer, len);
}

/*
 * eh_server callbacks
 */

static struct eh_connection *echo_on_connect(struct eh_server *UNUSED(server), int fd,
					     struct sockaddr *sa, socklen_t sa_len)
{
	struct echo_conn *self = echo_new(fd);
	if (self == NULL) {
		syserrf("echo_new(%d)", fd);
	} else if (eh_socket_ntop(self->name, sizeof(self->name), sa, sa_len) < 0) {
		syserr("eh_socket_ntop");
		echo_free(self);
	} else {
		/* happy case */
		debugf("%s: connected via fd %d", self->name, fd);

		return &self->conn;
	}

	return NULL;
}

/*
 * higher level functions
 */
static int echo_init(struct echo_server *self, const char *addr, unsigned port)
{
	struct eh_server *server = &self->server;

	memset(self, '\0', sizeof(*self));

	switch (eh_server_ipv4_tcp(server, addr, port)) {
	case -1:
		syserrf("eh_server_ipv4_tcp(..., \"%s\", %u)", addr, port);
		return -1;
	case 0:
		errf("eh_server_ipv4_tcp(..., \"%s\", %u): bad address",
		     addr, port);
		return -1;
	}

	server->on_connect = echo_on_connect;
	/* server->on_stop = echo_on_server_stop */
	/* server->on_error = echo_on_server_error; */
	return 1;
}

static inline void echo_start(struct echo_server *self, struct ev_loop *loop)
{
	eh_server_listen(&self->server, LISTEN_BACKLOG);
	eh_server_start(&self->server, loop);
}

static inline void echo_stop(struct echo_server *self, struct ev_loop *loop)
{
	eh_server_stop(&self->server, loop);
}

static void echo_signaled_stop(struct ev_loop *loop, struct ev_signal *w, int UNUSED(revents))
{
	struct echo_server *self = w->data;

	debugf("signal %d", w->signum);

	for (int i=0; i<2; i++)
		eh_signal_stop(&sig[i], loop);

	echo_stop(self, loop);
}

/*
 * and the main...
 */
int main(int UNUSED(argc), char * UNUSED(argv[]))
{
	struct ev_loop *loop = ev_default_loop(0);
	struct echo_server server;

	if (echo_init(&server, "0", 12345) < 0)
		return -1;

	eh_signal_init(&sig[0], echo_signaled_stop, &server, SIGINT);
	eh_signal_init(&sig[1], echo_signaled_stop, &server, SIGTERM);

	for (int i=0; i<2; i++)
		eh_signal_start(&sig[i], loop);

	echo_start(&server, loop);
	ev_loop(loop, 0);
	return 0;
}
