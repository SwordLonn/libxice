#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include "libuvbufpool.h"

uv_loop_t *recv_loop;
uv_loop_t *send_loop;
uv_udp_t send_socket;
uv_udp_t recv_socket;
uv_thread_t send_thread;
uv_thread_t recv_thread;

uv_udp_send_t send_req;
uv_buf_t discover_msg;
struct sockaddr_in send_addr;


void alloc_buffer(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
	int len = suggested_size;
	void *ptr = bufpool_acquire(handle->loop->data, &len);
	*buf = uv_buf_init(ptr, len);
}

void on_read(uv_udp_t *req, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned flags) {
	
	if (nread < 0) {
		fprintf(stderr, "Read error %s\n", uv_err_name(nread));
		uv_close((uv_handle_t*)req, NULL);
		free(buf->base);
		free(buf->base);
		return;
	}
	printf("recv\n");

	bufpool_release(buf->base);
}


void on_send(uv_udp_send_t *req, int status) {
	static send_n = 0;
	send_n++;
	if (send_n == 1000) {
		uv_stop(recv_loop);
		uv_stop(send_loop);
		return;
	}
		
	if (status) {
		fprintf(stderr, "Send error %s\n", uv_strerror(status));
		return;
	}
	uv_udp_send(&send_req, &send_socket, &discover_msg, 1, (const struct sockaddr *)&send_addr, on_send);
}

static char send_buffer[6000];

void send_func(uv_handle_t* arg) {
	discover_msg.len = 6000;
	discover_msg.base = send_buffer;
	uv_udp_init(send_loop, &send_socket);
	uv_ip4_addr("127.0.0.1", 12345, &send_addr);
	uv_udp_send(&send_req, &send_socket, &discover_msg, 1, (const struct sockaddr *)&send_addr, on_send);
	uv_run(send_loop, UV_RUN_DEFAULT);
}

void pool_test(bufpool_t *pool) {
	void* ptr[200];
	size_t len = 50000;
	int test_n = 1000;
	while (test_n--) {
		// allocate 1 - 49
		for (int i = 0; i < 80; i++) {
			ptr[i] = bufpool_acquire(pool, &len);
		}

		// free 50 - 79
		for (int i = 79; i > 49; i--) {
			bufpool_release(ptr[i]);
		}

		// allocate 80 - 199
		for (int i = 80; i < 200; i++) {
			ptr[i] = bufpool_acquire(pool, &len);
		}

		// free all
		for (int i = 0; i < 50; i++) {
			bufpool_release(ptr[i]);
		}

		for (int i = 80; i < 200; i++) {
			bufpool_release(ptr[i]);
		}
	}
}
static bufpool_t* global_pool = NULL;
void recv_func(uv_handle_t* arg) {
	uv_udp_init(recv_loop, &recv_socket);
	struct sockaddr_in recv_addr;
	uv_ip4_addr("0.0.0.0", 12345, &recv_addr);
	uv_udp_bind(&recv_socket, (const struct sockaddr *)&recv_addr, UV_UDP_REUSEADDR);

	// init buf pool
	bufpool_t *pool = malloc(sizeof(*pool));
	bufpool_init(pool);
	pool_test(pool);
	global_pool = pool;
	recv_loop->data = (void*)pool;
	uv_thread_create(&send_thread, send_func, NULL);
	uv_udp_recv_start(&recv_socket, alloc_buffer, on_read);

	uv_run(recv_loop, UV_RUN_DEFAULT);
}

int main() {
	recv_loop = malloc(sizeof(uv_loop_t));
	send_loop = malloc(sizeof(uv_loop_t));
	uv_loop_init(recv_loop);
	uv_loop_init(send_loop);
	srand(0);
	uv_thread_create(&recv_thread, recv_func, NULL);
	uv_thread_join(&send_thread);
	uv_thread_join(&recv_thread);

	bufpool_done(global_pool);
	uv_loop_close(recv_loop);
	uv_loop_close(send_loop);

	return 0;
}
