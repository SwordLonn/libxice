#include "xicecontext.h"

#include <glib.h>

#include "agent.h"
#include "socket.h"
#include "tcp-bsd.h"
#include "udp-bsd.h"

void xice_context_destroy(XiceContext* ctx) {
	g_assert(ctx != NULL);

	g_assert(ctx->destroy != NULL);

	ctx->destroy(ctx);

	g_slice_free(XiceContext, ctx);
}

XiceSocket* xice_context_tcp_bsd_socket_new(XiceContext* ctx, XiceAddress* addr) {
	g_assert(ctx != NULL && ctx->create_tcp_socket != NULL);
	g_assert(addr != NULL);
	
	return ctx->create_tcp_socket(ctx, addr);
}

XiceSocket* xice_context_udp_bsd_socket_new(XiceContext* ctx, XiceAddress* addr) {
	g_assert(ctx != NULL && ctx->create_udp_socket != NULL);
	g_assert(addr != NULL);
	
	return ctx->create_udp_socket(ctx, addr);
}

XiceTimer* xice_timer_new(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data) {
	XiceTimer* timer;

	g_assert(ctx != NULL && ctx->create_timer != NULL);
	g_assert(function != NULL);

	timer = ctx->create_timer(ctx, interval, function, data);

	return timer;
}

void xice_timer_start(XiceTimer* timer) {
	g_assert(timer != NULL && timer->start != NULL);

	timer->start(timer);
}

void xice_timer_stop(XiceTimer* timer) {
	g_assert(timer != NULL && timer->stop != NULL);

	timer->stop(timer);
}

void xice_timer_destroy(XiceTimer* timer) {
	g_assert(timer != NULL && timer->destroy != NULL);
	timer->destroy(timer);
	g_slice_free(XiceTimer, timer);
}


guint xice_add_timeout_seconds(XiceContext* ctx, guint interval,
	XiceTimeoutFunc function,
	gpointer data) {
	g_assert(ctx != NULL && ctx->add_timeout_seconds != NULL);
	g_assert(function != NULL);

	return ctx->add_timeout_seconds(ctx, interval, function, data);
}

void xice_remove_timeout(XiceContext* ctx, guint timeout) {
	g_assert(ctx != NULL && ctx->remove_timeout != NULL);

	ctx->remove_timeout(ctx, timeout);
}


static XiceSocket* gio_create_tcp_socket(XiceContext* ctx, XiceAddress* addr);
static XiceSocket* gio_create_udp_socket(XiceContext* ctx, XiceAddress* addr);

static XiceTimer* gio_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);

static guint gio_add_timeout_seconds(XiceContext* ctx, guint interval,
	XiceTimeoutFunc function,
	gpointer data);
static void gio_remove_timeout(XiceContext* ctx, guint timeout);

static void gio_destroy(XiceContext *ctx);

static void gio_timer_start(XiceTimer* timer);
static void gio_timer_stop(XiceTimer* timer);
static void gio_timer_destroy(XiceTimer* timer);

typedef struct _XiceContextGIO
{
	GMainContext *main_context;     /* main context pointer */
	GList* timeout_list;
}XiceContextGIO;

typedef struct _XiceTimerGIO
{
	GSource* source;
	XiceContextGIO* context;
}XiceTimerGIO;


typedef struct {
	XiceContext* context;
	XiceTimeoutFunc func;
	gpointer data;
	guint id;
}TimeOutCtx;

XiceContext *xice_context_new(GMainContext* ctx)
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

static XiceSocket *gio_create_tcp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextGIO* gio = ctx->priv;
	XiceSocket* sock = xice_tcp_bsd_socket_new(gio->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

static XiceSocket* gio_create_udp_socket(XiceContext* ctx, XiceAddress* addr) {
	XiceContextGIO* gio = ctx->priv;
	XiceSocket* sock = xice_udp_bsd_socket_new(gio->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

static XiceTimer* gio_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data) {
	XiceContextGIO* gio = ctx->priv;

	XiceTimer* timer = g_slice_new0(XiceTimer);
	XiceTimerGIO* tgio = g_slice_new0(XiceTimerGIO);
	tgio->context = gio;
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

static boolean timeout_func(gpointer data) {
	TimeOutCtx* ctx = (TimeOutCtx*)data;
	GSource* source = g_main_current_source();
	boolean ret = TRUE;
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

static guint gio_add_timeout_seconds(XiceContext* ctx, guint interval,
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

static void gio_remove_timeout(XiceContext* ctx, guint timeout) {
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

static void gio_destroy(XiceContext *ctx) {
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

static gboolean priv_timeout_cb(gpointer user_data) {
	XiceTimer* timer = (XiceTimer*)user_data;
	XiceTimerGIO* gio = timer->priv;
	GSource* source = g_main_current_source();
	if (g_source_is_destroyed(source)) {
		return FALSE;
	}
	if (timer->func) {
		return timer->func(timer->data);
	}
	return TRUE;
}

static void gio_timer_start(XiceTimer* timer)
{
	XiceTimerGIO* gio;
	g_assert(timer != NULL);
	gio = timer->priv;
	g_assert(gio != NULL);
	if (gio->source) {
		gio_timer_stop(timer);
	}
	gio->source = g_timeout_source_new(timer->interval);
	g_source_set_callback(gio->source, (GSourceFunc)priv_timeout_cb, timer, NULL);
	g_source_attach(gio->source, gio->context->main_context);
	timer->id = g_source_get_id(gio->source);
}

static void gio_timer_stop(XiceTimer* timer)
{
	XiceTimerGIO* gio;
	g_assert(timer != NULL);
	gio = timer->priv;
	if (gio->source) {
		g_source_destroy(gio->source);
		g_source_unref(gio->source);
		gio->source = NULL;
	}
}

static void gio_timer_destroy(XiceTimer* timer) 
{
	XiceTimerGIO* gio;
	g_assert(timer != NULL);
	gio = timer->priv;
	if (gio != NULL) {
		gio_timer_stop(timer);
		g_slice_free(XiceTimerGIO, gio);
		timer->priv = NULL;
	}
}
