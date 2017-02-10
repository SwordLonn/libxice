#include "xicecontext.h"

#include <glib.h>

#include "agent.h"
#include "xicesocket.h"
#include "giocontext.h"

void xice_context_destroy(XiceContext* ctx) {
	g_assert(ctx != NULL);

	g_assert(ctx->destroy != NULL);

	ctx->destroy(ctx);

	g_slice_free(XiceContext, ctx);
}

XiceSocket* xice_create_tcp_socket(XiceContext* ctx, XiceAddress* addr) {
	g_assert(ctx != NULL && ctx->create_tcp_socket != NULL);
	g_assert(addr != NULL);
	
	return ctx->create_tcp_socket(ctx, addr);
}

XiceSocket* xice_create_udp_socket(XiceContext* ctx, XiceAddress* addr) {
	g_assert(ctx != NULL && ctx->create_udp_socket != NULL);
	g_assert(addr != NULL);
	
	return ctx->create_udp_socket(ctx, addr);
}

XiceTimer* xice_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data) {
	XiceTimer* timer;

	g_assert(ctx != NULL && ctx->create_timer != NULL);
	g_assert(function != NULL);

	timer = ctx->create_timer(ctx, interval, function, data);

	return timer;
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

XiceContext *xice_context_create(const char* type, gpointer ctx)
{
	if (strcmp(type, "gio") == 0) {
		return gio_context_create(ctx);
	}
	return NULL;
}
