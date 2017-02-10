#include <glib.h>
#include "agent/debug.h"
#include "giocontext.h"
#include "giotimer.h"
#include "giotcp.h"
#include "gioudp.h"

typedef struct {
	XiceContext* context;
	XiceTimeoutFunc func;
	gpointer data;
	guint id;
}TimeOutCtx;

static void
_priv_set_socket_tos(XiceSocket *sock, gint tos)
{
	if (setsockopt(xice_socket_get_fd(sock), IPPROTO_IP,
		IP_TOS, (const char *)&tos, sizeof(tos)) < 0) {
		//xice_debug("Agent : Could not set socket ToS",
		//	g_strerror(errno));
	}
#ifdef IPV6_TCLASS
	if (setsockopt(xice_socket_get_fd(sock), IPPROTO_IPV6,
		IPV6_TCLASS, (const char *)&tos, sizeof(tos)) < 0) {
		//xice_debug("Agent : Could not set IPV6 socket ToS",
		//	g_strerror(errno));
	}
#endif
}

XiceContext *gio_context_create(gpointer ctx)
{
	XiceContext* xctx = g_slice_new0(XiceContext);
	XiceContextGIO* gio = g_slice_new0(XiceContextGIO);

	gio->main_context = g_main_context_ref(ctx);
	gio->timeout_list = NULL;

	xctx->priv = gio;
	xctx->create_tcp_socket = gio_create_tcp_socket;
	xctx->create_udp_socket = gio_create_udp_socket;
	xctx->create_timer = gio_create_timer;
	xctx->add_timeout_seconds = gio_add_timeout_seconds;
	xctx->remove_timeout = gio_remove_timeout;
	xctx->destroy = gio_destroy;
	return xctx;
}

XiceSocket *gio_create_tcp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextGIO* gio = ctx->priv;
	XiceSocket* sock = xice_tcp_bsd_socket_new(gio->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

XiceSocket* gio_create_udp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextGIO* gio = ctx->priv;
	XiceSocket* sock = xice_udp_bsd_socket_new(gio->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

XiceTimer* gio_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data) {
	XiceContextGIO* gio = ctx->priv;

	XiceTimer* timer = g_slice_new0(XiceTimer);
	XiceTimerGIO* tgio = g_slice_new0(XiceTimerGIO);
	tgio->context = gio->main_context;
	timer->interval = interval;
	timer->func = function;
	timer->data = data;
	tgio->source = NULL;

	timer->priv = tgio;

	timer->start = gio_timer_start;
	timer->stop = gio_timer_stop;
	timer->destroy = gio_timer_destroy;

	return timer;
}

void gio_destroy(XiceContext *ctx) {
	XiceContextGIO* gio = ctx->priv;
	GList* i;

	for (i = gio->timeout_list; i; i = i->next) {
		TimeOutCtx* tctx = i->data;
		g_source_remove(tctx->id);
		g_slice_free(TimeOutCtx, tctx);
	}

	g_list_free_full(gio->timeout_list, NULL);

	g_main_context_unref(gio->main_context);
	g_slice_free(XiceContextGIO, gio);
	ctx->priv = NULL;
}

static gboolean timeout_func(gpointer data) {
	TimeOutCtx* ctx = (TimeOutCtx*)data;
	GSource* source = g_main_current_source();
	gboolean ret = TRUE;
	GList* i;
	XiceContextGIO* gio = ctx->context->priv;

	if (g_source_is_destroyed(source)) {
		ret = FALSE;
		goto end;
	}
	if (ctx->func) {
		ret = ctx->func(ctx->id, ctx->data);
	}

end:
	for (i = gio->timeout_list; i; i = i->next) {
		TimeOutCtx* tctx = i->data;
		if (tctx->id == ctx->id) {
			gio->timeout_list = g_list_remove(gio->timeout_list, i);
			g_slice_free(TimeOutCtx, tctx);
			break;
		}
	}
	return ret;
}

guint gio_add_timeout_seconds(XiceContext* ctx, guint interval,
	XiceTimeoutFunc function,
	gpointer data) {
	TimeOutCtx* tctx = g_slice_new0(TimeOutCtx);
	XiceContextGIO* gio = ctx->priv;
	tctx->func = function;
	tctx->data = data;
	tctx->context = ctx;
	tctx->id = g_timeout_add_seconds(interval,
		(GSourceFunc)timeout_func, tctx);

	gio->timeout_list = g_list_prepend(gio->timeout_list, tctx);
	return tctx->id;
}

void gio_remove_timeout(XiceContext* ctx, guint timeout) {
	XiceContextGIO* gio = ctx->priv;
	GList* i;
	for (i = gio->timeout_list; i; i = i->next) {
		TimeOutCtx* tctx = i->data;
		if (tctx->id == timeout) {
			gio->timeout_list = g_list_remove(gio->timeout_list, i);
			g_slice_free(TimeOutCtx, tctx);
			break;
		}
	}
	g_source_remove(timeout);
}
