#ifndef __LIBUV_TIMER_H__
#define __LIBUV_TIMER_H__

#ifdef HAVE_LIBUV

#include <uv.h>
#include "xicetimer.h"

XiceTimer* libuv_timer_create(uv_loop_t* loop, guint interval, 
	XiceTimerFunc function, gpointer data);

#endif
#endif