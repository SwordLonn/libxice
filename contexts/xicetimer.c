
#include <glib.h>
#include "xicetimer.h"

void xice_timer_start(XiceTimer* timer) {
	g_assert(timer != NULL && timer->start != NULL);

	timer->start(timer);
}

void xice_timer_stop(XiceTimer* timer) {
	g_assert(timer != NULL && timer->stop != NULL);

	timer->stop(timer);
}

void xice_timer_destroy(XiceTimer* timer) {
	g_assert(timer != NULL && timer->destroy != NULL);
	timer->destroy(timer);
	//g_slice_free(XiceTimer, timer);  //lost a fucking saturday!!!
}