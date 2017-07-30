#include "config.h"
#include "libuvcontext.h"
#include "libuvtimer.h"
#include "libuvtcp.h"
#include "libuvudp.h"

#ifdef HAVE_LIBUV
#include <uv.h>

typedef struct _XiceContextLibuv {
	
	uv_loop_t* loop;

}XiceContextLibuv;

static XiceSocket* create_tcp_socket(XiceContext* ctx, XiceAddress* addr);
static XiceSocket* create_udp_socket(XiceContext* ctx, XiceAddress* addr);
static XiceTimer* create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);

static void destroy(XiceContext* ctx);

XiceContext *libuv_context_create(gpointer ctx) 
{
	XiceContext* xice = g_slice_new0(XiceContext);
	XiceContextLibuv* uv = g_slice_new0(XiceContextLibuv);
	uv->loop = ctx;
	xice->priv = uv;
	xice->create_tcp_socket = create_tcp_socket;
	xice->create_udp_socket = create_udp_socket;
	xice->create_timer = create_timer;
	xice->destroy = destroy;

	return xice;
}

static XiceSocket* create_tcp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextLibuv* uv = ctx->priv;
	return libuv_tcp_socket_create(uv->loop, addr);
}

static XiceSocket* create_udp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextLibuv* uv = ctx->priv;
	return libuv_udp_socket_create(uv->loop, addr);
}

static XiceTimer* create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data) {
	XiceContextLibuv* uv = ctx->priv;
	return libuv_timer_create(uv->loop, interval, function, data);
}

static void destroy(XiceContext* ctx) {
	XiceContextLibuv *uv = ctx->priv;

	g_slice_free(XiceContextLibuv, uv);

	g_slice_free(XiceContext, ctx);
}

#endif