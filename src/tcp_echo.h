#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <eh_server.h>
#include <eh_list.h>

enum {
	LISTEN_BACKLOG = 5,

	READ_BUFFER_SIZE = 1024,
	WRITE_BUFFER_SIZE = 1024,

	CONN_MAXNAME = 64,
};

struct echo_server {
	struct eh_server server;

	struct eh_list connections;
};

struct echo_conn {
	struct eh_connection conn;
	struct eh_list siblings;

	uint8_t read_buffer[READ_BUFFER_SIZE];
	uint8_t write_buffer[WRITE_BUFFER_SIZE];

	char name[CONN_MAXNAME];
};
