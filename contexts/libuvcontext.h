#ifndef __LIBUV_CONTEXT_H__
#define __LIBUV_CONTEXT_H__

#ifdef HAVE_LIBUV

#include "xicecontext.h"

XiceContext *libuv_context_create(const char* type, gpointer ctx);

#endif
#endif 