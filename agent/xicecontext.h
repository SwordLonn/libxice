#ifndef __XICE_CONTEXT_H__
#define __XICE_CONTEXT_H__

#include <glib-object.h>
#include "socket.h"

typedef struct _XiceContext XiceContext;
typedef struct _XiceTimer XiceTimer;
typedef gboolean(*XiceTimerFunc) (gpointer data);
typedef gboolean(*XiceTimeoutFunc) (guint source, gpointer data);

XiceContext* xice_context_new(GMainContext* ctx);

XiceContext* xice_context_ref(XiceContext* ctx);
void xice_context_unref(XiceContext* ctx);

XiceSocket* xice_context_tcp_bsd_socket_new(XiceContext* ctx, XiceAddress* addr);
XiceSocket* xice_context_udp_bsd_socket_new(XiceContext* ctx, XiceAddress* addr);

XiceTimer* xice_timer_new(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);
void xice_timer_start(XiceTimer* timer);
void xice_timer_stop(XiceTimer* timer);
void xice_timer_destroy(XiceTimer* timer);
XiceTimer* xice_timer_ref(XiceTimer* timer);
void xice_timer_unref(XiceTimer* timer);

guint xice_add_timeout_seconds(XiceContext* ctx, guint interval,
	XiceTimeoutFunc     function,
	gpointer        data);

void xice_remove_timeout(XiceContext* ctx, guint timeout);

#endif