#ifndef __XICE_CONTEXT_H__
#define __XICE_CONTEXT_H__

#include <glib-object.h>
#include "xicesocket.h"
#include "xicetimer.h"

G_BEGIN_DECLS

typedef struct _XiceContext XiceContext;

struct _XiceContext {

	//functions
	XiceSocket* (*create_tcp_socket)(XiceContext* ctx, XiceAddress* addr);
	XiceSocket* (*create_udp_socket)(XiceContext* ctx, XiceAddress* addr);

	XiceTimer* (*create_timer)(XiceContext* ctx, guint interval,
		XiceTimerFunc function, gpointer data);
	
	void (*destroy)(XiceContext *ctx);

	//attributes
	void* priv;
};

XiceContext* xice_context_create(const char* type, gpointer data);
void xice_context_destroy(XiceContext* ctx);

XiceSocket* xice_create_tcp_socket(XiceContext* ctx, XiceAddress* addr);
XiceSocket* xice_create_udp_socket(XiceContext* ctx, XiceAddress* addr);

XiceTimer* xice_create_timer(XiceContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);

G_END_DECLS

#endif
