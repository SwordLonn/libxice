#include "xicecontext.h"

#include <glib.h>

#include "agent.h"
#include "socket.h"
#include "tcp-bsd.h"
#include "udp-bsd.h"

struct _XiceContext
{
	GMainContext *main_context;     /* main context pointer */
	int ref_count;
	GList* timeout_list;
};

struct _XiceTimer
{
	GSource* source;
	XiceContext* context;
	guint interval;
	XiceTimerFunc func;
	gpointer data;
	guint id;
	int ref_count;
};


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

XiceSocket *xice_context_tcp_bsd_socket_new(XiceContext* ctx, XiceAddress* addr) {
	XiceSocket* sock = xice_tcp_bsd_socket_new(ctx->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

XiceSocket* xice_context_udp_bsd_socket_new(XiceContext* ctx, XiceAddress* addr) {
	XiceSocket* sock = xice_udp_bsd_socket_new(ctx->main_context, addr);
	if (sock != NULL) {
		_priv_set_socket_tos(sock, 0);
	}
	return sock;
}

XiceContext* xice_context_new(GMainContext* ctx) {
	XiceContext* context;
	context = g_slice_new0(XiceContext);
	context->ref_count = 1;
	context->main_context = ctx;
	context->timeout_list = g_list_alloc();
	g_main_context_ref(ctx);
	return context;
}

XiceContext* xice_context_ref(XiceContext* ctx) {
	g_atomic_int_inc(&ctx->ref_count);
	return ctx;
}

void xice_context_unref(XiceContext* ctx) {
	GList* i;
	if (!g_atomic_int_dec_and_test(&ctx->ref_count))
		return;

	g_main_context_unref(ctx->main_context);
	for (i = ctx->timeout_list; i; i = i->next) {
		TimeOutCtx* ctx = i->data;
		g_slice_free(TimeOutCtx, ctx);
	}
	g_list_free(ctx->timeout_list);
	g_slice_free(XiceContext, ctx);
}

XiceTimer* xice_timer_new(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data)
{
	XiceTimer* timer = g_slice_new0(XiceTimer);

	timer->ref_count = 1;
	timer->context = ctx;
	timer->interval = interval;
	timer->func = function;
	timer->data = data;
	timer->source = NULL;

	return timer;
}

static gboolean priv_timeout_cb(gpointer user_data) {
	XiceTimer* timer = (XiceTimer*)user_data;
	GSource* source = g_main_current_source();
	if (g_source_is_destroyed(source)) {
		return FALSE;
	}
	if (timer->func) {
		return timer->func(timer->data);
	}
	return TRUE;
}

void xice_timer_start(XiceTimer* timer) {
	if (timer->source) {
		xice_timer_stop(timer);
	}
	timer->source = g_timeout_source_new(timer->interval);
	g_source_set_callback(timer->source, (GSourceFunc)priv_timeout_cb, timer, NULL);
	g_source_attach(timer->source, timer->context->main_context);
}

void xice_timer_stop(XiceTimer* timer) {
	xice_timer_destroy(timer);
}

void xice_timer_destroy(XiceTimer* timer) {
	if (timer->source) {
		g_source_destroy(timer->source);
		g_source_unref(timer->source);
		timer->source = NULL;
	}
}

XiceTimer* xice_timer_ref(XiceTimer* timer) {
	g_atomic_int_inc(&timer->ref_count);
	return timer;
}

void xice_timer_unref(XiceTimer* timer) {
	
	if (!g_atomic_int_dec_and_test(&timer->ref_count))
		return;
	if (timer->source) {
		xice_timer_destroy(timer);
	}
	g_slice_free(XiceTimer, timer);
}

static boolean timeout_func(gpointer data) {
	TimeOutCtx* ctx = (TimeOutCtx*)data;
	GSource* source = g_main_current_source();
	boolean ret = TRUE;
	GList* i;
	if (g_source_is_destroyed(source)) {
		ret = FALSE;
		goto end;
	}
	if (ctx->func) {
		ret = ctx->func(ctx->id, ctx->data);
	}
end:
	//TODO remove from ctx
	for (i = ctx->context->timeout_list; i; i = i->next) {
		TimeOutCtx* tctx = i->data;
		if (tctx->id == ctx->id) {
			g_list_remove(ctx->context->timeout_list, i);
			g_slice_free(TimeOutCtx, tctx);
			break;
		}
	}
	return ret;
}

guint xice_add_timeout_seconds(XiceContext* ctx, guint interval,
	XiceTimeoutFunc     function,
	gpointer        data) {
	TimeOutCtx* tctx = g_slice_new0(TimeOutCtx);
	tctx->func = function;
	tctx->data = data;
	tctx->context = ctx;
    tctx->id =  g_timeout_add_seconds(interval,
        (GSourceFunc)timeout_func, tctx);

	g_list_append(ctx->timeout_list, tctx);
	return tctx->id;
}

void xice_remove_timeout(XiceContext* ctx, guint timeout) {
	GList* i;
	for (i = ctx->timeout_list; i; i = i->next) {
		TimeOutCtx* tctx = i->data;
		if (tctx->id == timeout) {
			g_list_remove(ctx->timeout_list, i);
			g_slice_free(TimeOutCtx, tctx);
			break;
		}
	}
	
	g_source_remove(timeout);
}