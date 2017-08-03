#include "libuvbufpool.h"
#include <stdlib.h>
#include <assert.h>

#define bufbase(ptr) ((bufbase_t *)((char *)(ptr) - sizeof(bufbase_t)))
#define buflen(ptr) (bufbase(ptr)->len)

static void bufpool_enqueue(bufpool_t *pool, void *ptr) {
	int idx;
	for (idx = 0; idx < pool->size; ++idx) {
		if (!pool->bufs[idx]) break;
	}
	assert(idx < pool->size);
	pool->bufs[idx] = ptr;
}

static void *bufpool_alloc(bufpool_t *pool, int len) {
	bufbase_t *base = malloc(sizeof(bufbase_t) + len);
	if (!base) return 0;
	base->pool = pool;
	base->len = len;
	return (char *)base + sizeof(bufbase_t);
}

static void *bufpool_grow(bufpool_t *pool) {

	int idx = pool->size;
	void *buf;
	if (idx == BUFPOOL_CAPACITY) return 0;
	buf = bufpool_alloc(pool, BUF_SIZE);
	if (!buf) return 0;
	pool->bufs[idx] = 0;
	pool->size = idx + 1;
	return buf;
}

static void* bufpool_dequeue(bufpool_t *pool) {
	int idx;
	void *ptr;
	for (idx = 0; idx < pool->size; ++idx) {
		ptr = pool->bufs[idx];
		if (ptr) {
			pool->bufs[idx] = 0;
			return ptr;
		}
	}
	return bufpool_grow(pool);
}

static void *bufpool_dummy() {
	return bufpool_alloc(0, DUMMY_BUF_SIZE);
}

static void bufpool_free(void *ptr) {
	if (!ptr) return;
	free(bufbase(ptr));
}

void bufpool_release(void *ptr) {
	bufbase_t *base;
	if (!ptr) return;
	base = bufbase(ptr);
	if (base->pool) 
		bufpool_enqueue(base->pool, ptr);
	else free(base);
}

void *bufpool_acquire(bufpool_t *pool, int *len) {
	void *buf = bufpool_dequeue(pool);
	if (!buf) buf = bufpool_dummy();
	*len = buf ? buflen(buf) : 0;
	return buf;
}

void bufpool_init(bufpool_t *pool) {
	pool->size = 0;
}

void bufpool_done(bufpool_t *pool) {
	int idx;
	for (idx = 0; idx < pool->size; ++idx) bufpool_free(pool->bufs[idx]);
}