#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <eh_server.h>	/* eh_server */

enum {
	LISTEN_BACKLOG = 5,
};

struct echo_server {
	struct eh_server server;
};
