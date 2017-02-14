#ifndef __LIBUV_TCP_H__
#define __LIBUV_TCP_H__

#ifdef HAVE_LIBUV

#include <uv.h>
#include "xicesocket.h"

XiceSocket *libuv_tcp_socket_create(uv_loop_t* loop, XiceAddress* addr);

#endif
#endif