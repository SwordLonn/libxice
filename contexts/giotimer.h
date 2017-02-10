#ifndef __GIO_TIMER_H__
#define __GIO_TIMER_H__

#include <glib.h>
#include "xicetimer.h"

//#include "giocontext.h"

G_BEGIN_DECLS

typedef struct _XiceTimerGIO
{
	GSource* source;
	GMainContext* context;
}XiceTimerGIO;

void gio_timer_start(XiceTimer* timer);
void gio_timer_stop(XiceTimer* timer);
void gio_timer_destroy(XiceTimer* timer);

G_END_DECLS

#endif
