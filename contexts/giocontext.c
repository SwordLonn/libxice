#include <glib.h>
#include "agent/debug.h"
#include "giocontext.h"
#include "giotimer.h"
#include "giotcp.h"
#include "gioudp.h"

typedef struct _XiceContextGIO
{
	GMainContext *main_context;     /* main context pointer */
}XiceContextGIO;

static XiceSocket* gio_create_tcp_socket(XiceContext* ctx, XiceAddress* addr);
static XiceSocket* gio_create_udp_socket(XiceContext* ctx, XiceAddress* addr);
static XiceTimer* gio_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);

static void gio_destroy(XiceContext *ctx);

static void
_priv_set_socket_tos(XiceSocket *sock, gint tos)
{
	if (setsockopt(xice_socket_get_fd(sock), IPPROTO_IP,
		IP_TOS, (const char *)&tos, sizeof(tos)) < 0) {
		xice_debug("Agent : Could not set socket ToS",
			g_strerror(errno));
	}
#ifdef IPV6_TCLASS
	if (setsockopt(xice_socket_get_fd(sock), IPPROTO_IPV6,
		IPV6_TCLASS, (const char *)&tos, sizeof(tos)) < 0) {
		xice_debug("Agent : Could not set IPV6 socket ToS",
			g_strerror(errno));
	}
#endif
}

XiceContext *gio_context_create(gpointer ctx)
{
	XiceContext* xctx = g_slice_new0(XiceContext);
	XiceContextGIO* gio = g_slice_new0(XiceContextGIO);

	gio->main_context = g_main_context_ref(ctx);

	xctx->priv = gio;
	xctx->create_tcp_socket = gio_create_tcp_socket;
	xctx->create_udp_socket = gio_create_udp_socket;
	xctx->create_timer = gio_create_timer;

	xctx->destroy = gio_destroy;
	return xctx;
}

static XiceSocket *gio_create_tcp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextGIO* gio = ctx->priv;
	XiceSocket* sock = gio_tcp_socket_create(gio->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

static XiceSocket* gio_create_udp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextGIO* gio = ctx->priv;
	XiceSocket* sock = gio_udp_socket_create(gio->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

static XiceTimer* gio_create_timer(XiceContext* ctx, guint interval, 
	XiceTimerFunc function, gpointer data)
{
	XiceContextGIO* gio = ctx->priv;
	XiceTimer* timer = gio_timer_create(gio->main_context, interval, function, data);
	return timer;
}

static void gio_destroy(XiceContext *ctx) {
	XiceContextGIO* gio = ctx->priv;

	g_main_context_unref(gio->main_context);
	g_slice_free(XiceContextGIO, gio);
	ctx->priv = NULL;
	g_slice_free(XiceContext, ctx);
}
