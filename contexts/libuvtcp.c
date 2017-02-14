#include "config.h"
#include "libuvtcp.h"

#ifdef HAVE_LIBUV

XiceSocket *libuv_tcp_socket_create(uv_loop_t* loop, XiceAddress* addr) {
	return NULL;
}

#endif