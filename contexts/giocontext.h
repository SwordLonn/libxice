#ifndef __GIO_CONTEXT_H__
#define __GIO_CONTEXT_H__

#include "xicecontext.h"

G_BEGIN_DECLS

XiceSocket* gio_create_tcp_socket(XiceContext* ctx, XiceAddress* addr);
XiceSocket* gio_create_udp_socket(XiceContext* ctx, XiceAddress* addr);

XiceTimer* gio_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);

void gio_destroy(XiceContext *ctx);

typedef struct _XiceContextGIO
{
	GMainContext *main_context;     /* main context pointer */
	GList* timeout_list;
}XiceContextGIO;

XiceContext* gio_context_create(gpointer data);

G_END_DECLS

#endif
