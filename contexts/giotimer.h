#ifndef __GIO_TIMER_H__
#define __GIO_TIMER_H__

#include <glib.h>
#include "xicetimer.h"

//#include "giocontext.h"

G_BEGIN_DECLS


XiceTimer* gio_timer_create(GMainContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data);

G_END_DECLS

#endif
