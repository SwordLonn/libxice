#include "config.h"

#ifdef HAVE_LIBUV
#include "libuvtimer.h"

typedef struct _XiceTimerLibuv {
	uv_loop_t* loop;
	uv_timer_t* handle;
}XiceTimerLibuv;

static void libuv_timer_start(XiceTimer* timer);
static void libuv_timer_stop(XiceTimer* timer);
static void libuv_timer_destroy(XiceTimer* timer);

XiceTimer* libuv_timer_create(uv_loop_t* loop, guint interval,
	XiceTimerFunc function, gpointer data) {
	XiceTimer* timer = g_slice_new0(XiceTimer);
	XiceTimerLibuv* uv = g_slice_new0(XiceTimerLibuv);
	uv->loop = loop;
	uv->handle = g_slice_new0(uv_timer_t);
	uv->handle->data = timer;
	uv_timer_init(loop, uv->handle);

	timer->interval = interval;
	timer->func = function;
	timer->data = data;
	timer->priv = uv;

	timer->start = libuv_timer_start;
	timer->stop = libuv_timer_stop;
	timer->destroy = libuv_timer_destroy;

	return timer;
}

static void timer_callback(uv_timer_t *timer) {
	XiceTimer* xt = timer->data;
	xt->func(xt, xt->data);
}

static void libuv_timer_start(XiceTimer* timer) {
	XiceTimerLibuv* uv = timer->priv;
	uv_timer_start(uv->handle, timer_callback, timer->interval, 0);
}

static void libuv_timer_stop(XiceTimer* timer) {
	XiceTimerLibuv* uv = timer->priv;
	uv_timer_stop(uv->handle);
}

static void on_close(uv_handle_t* handle) {
	g_slice_free(uv_timer_t, (uv_timer_t*)handle);
}

static void libuv_timer_destroy(XiceTimer* timer) {
	XiceTimerLibuv* uv = timer->priv;
	uv_close((uv_handle_t*)uv->handle, on_close);

	g_slice_free(XiceTimerLibuv, uv);
	g_slice_free(XiceTimer, timer);
}

#endif