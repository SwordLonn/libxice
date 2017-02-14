
#include <glib.h>
#include "giotimer.h"
#include "giocontext.h"
#include "giotimer.h"

typedef struct _XiceTimerGIO
{
	GSource* source;
	GMainContext* context;
}XiceTimerGIO;

static void gio_timer_start(XiceTimer* timer);
static void gio_timer_stop(XiceTimer* timer);
static void gio_timer_destroy(XiceTimer* timer);

XiceTimer* gio_timer_create(GMainContext* ctx, guint interval,
	XiceTimerFunc function, gpointer data) {

	XiceTimer* timer = g_slice_new0(XiceTimer);
	XiceTimerGIO* tgio = g_slice_new0(XiceTimerGIO);
	tgio->context = ctx;
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

static gboolean priv_timeout_cb(gpointer user_data) {
	XiceTimer* timer = (XiceTimer*)user_data;
	GSource* source = g_main_current_source();
	if (g_source_is_destroyed(source)) {
		return FALSE;
	}
	if (timer->func) {
		return timer->func(timer, timer->data);
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
	g_source_attach(gio->source, gio->context);
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
		g_slice_free(XiceTimer, timer);
	}
}
