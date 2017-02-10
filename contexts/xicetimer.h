#ifndef __XICE_TIMER_H__
#define __XICE_TIMER_H__
#include <glib.h>

G_BEGIN_DECLS

typedef struct _XiceTimer XiceTimer;
typedef gboolean(*XiceTimerFunc) (gpointer data);
typedef gboolean(*XiceTimeoutFunc) (guint source, gpointer data);

struct _XiceTimer {

	//functions
	void(*start)(XiceTimer* timer);
	void(*stop)(XiceTimer* stop);
	void(*destroy)(XiceTimer *timer);

	//attributes
	void* priv;
	guint interval;
	XiceTimerFunc func;
	gpointer data;
	guint id;
};

void xice_timer_start(XiceTimer* timer);
void xice_timer_stop(XiceTimer* timer);
void xice_timer_destroy(XiceTimer* timer);

G_END_DECLS

#endif
