/*
 * copy from
 * https://stackoverflow.com/questions/28511541/libuv-allocated-memory-buffers-re-use-techniques#
 */
#ifndef __LIBUV_BUFPOOL_H__
#define __LIBUV_BUFPOOL_H__

#define BUFPOOL_CAPACITY 100
#define BUF_SIZE 64000
#define DUMMY_BUF_SIZE 64000

typedef struct bufpool_s bufpool_t;

struct bufpool_s {
	void *bufs[BUFPOOL_CAPACITY];
	int size;
};

typedef struct bufbase_s bufbase_t;

struct bufbase_s {
	bufpool_t *pool;
	int len;
};

void bufpool_init(bufpool_t *pool);
void bufpool_done(bufpool_t *pool);
void bufpool_release(void *ptr);
void *bufpool_acquire(bufpool_t *pool, int *len);

#endif