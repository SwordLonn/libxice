
#include <glib.h>
#include "giotimer.h"
#include "giocontext.h"
#include "giotimer.h"

static gboolean priv_timeout_cb(gpointer user_data) {
	XiceTimer* timer = (XiceTimer*)user_data;
	//XiceTimerGIO* gio = timer->priv;
	GSource* source = g_main_current_source();
	if (g_source_is_destroyed(source)) {
		return FALSE;
	}
	if (timer->func) {
		return timer->func(timer, timer->data);
	}
	return TRUE;
}

void gio_timer_start(XiceTimer* timer)
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
	g_source_attach(gio->source, gio->context);
	timer->id = g_source_get_id(gio->source);
}

void gio_timer_stop(XiceTimer* timer)
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

void gio_timer_destroy(XiceTimer* timer)
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
