#include "config.h"
#include "libuvtcp.h"
#include "agent/debug.h"

#ifdef HAVE_LIBUV

#define BUFFER_SIZE (4096)

typedef struct _UvWriteData {
	XiceSocket*    socket;
	uv_write_t req;
	struct _UvWriteData *next;
	uint8_t buffer[BUFFER_SIZE];
}UvWriteData;

typedef struct _LibuvTcp {
	uv_loop_t* loop;
	uv_tcp_t* handle;
	uv_connect_t connect;
	struct sockaddr_storage addr;
	XiceAddress xaddr;
	UvWriteData *list;
}LibuvTcp;

static void socket_close(XiceSocket *sock);

static gboolean socket_send(XiceSocket *sock, const XiceAddress *to,
	guint len, const gchar *buf);
static gboolean socket_is_reliable(XiceSocket *sock);
static int socket_get_fd(XiceSocket *sock);

static void connect_cb(uv_connect_t* req, int status);
static void on_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
void on_recv_callback(uv_stream_t* stream,
	ssize_t nread,
	const uv_buf_t* buf);
void on_write_callback(uv_write_t* req, int status);
static void on_close_callback(uv_handle_t* handle);

XiceSocket *libuv_tcp_socket_create(uv_loop_t* loop, XiceAddress* addr) {
	XiceSocket* sock = g_slice_new0(XiceSocket);
	LibuvTcp* tcp = g_slice_new0(LibuvTcp);
	
	tcp->loop = loop;
	tcp->handle = g_slice_new0(uv_tcp_t);

	uv_tcp_init(loop, tcp->handle);
	xice_address_copy_to_sockaddr(addr, (struct sockaddr*)&tcp->addr);
	tcp->connect.data = sock;
	tcp->xaddr = *addr;
	uv_tcp_connect(&tcp->connect, tcp->handle, (struct sockaddr*)&tcp->addr, connect_cb);

	sock->priv = tcp;
	sock->fileno = (gpointer)tcp->handle;

	sock->send = socket_send;
	sock->is_reliable = socket_is_reliable;
	sock->close = socket_close;
	sock->get_fd = socket_get_fd;

	return sock;
}

static void socket_close(XiceSocket *sock) {
	LibuvTcp* tcp = sock->priv;

	uv_read_stop((uv_stream_t*)tcp->handle);

	uv_close((uv_handle_t*)tcp->handle, on_close_callback);

	g_slice_free(LibuvTcp, tcp);
}

static gboolean socket_send(XiceSocket *sock, const XiceAddress *to,
	guint len, const gchar *buf) {
	size_t sent;
	LibuvTcp *tcp = sock->priv;
	uv_buf_t buffer;
	int err;
	g_assert(len <= BUFFER_SIZE);

	buffer = uv_buf_init((char*)buf, len);
	sent = uv_try_write((uv_stream_t*)tcp->handle, &buffer, 1);
	if (sent == (int)len) {
		return TRUE;
	}
	else if (sent == UV_EAGAIN || sent == UV_ENOSYS) {
		sent = 0;
	}
	else if (sent < 0 ) {
		xice_debug("uv_udp_try_send() failed : %s", uv_strerror(sent));
		return FALSE;
	}

	UvWriteData* send_data = NULL;
	if (tcp->list == NULL) {
		send_data = (UvWriteData*)g_alloca(sizeof(UvWriteData));
		send_data->socket = sock;
		send_data->req.data = (gpointer)send_data;
	}
	else {
		send_data = tcp->list;
		tcp->list = send_data->next;
		send_data->next = NULL;
	}
	memcpy(send_data->buffer, buf + sent, len - sent);
	buffer = uv_buf_init((char*)send_data->buffer, len - sent);
	err = uv_write(&send_data->req, (uv_stream_t*)tcp->handle, &buffer, 1, on_write_callback);
	if (err) {
		xice_debug("uv_udp_send() failed: %s", uv_strerror(err));
		send_data->next = tcp->list;
		tcp->list = send_data;
		return FALSE;
	}
	return TRUE;
}

static gboolean socket_is_reliable(XiceSocket *sock) {
	return TRUE;
}

static int socket_get_fd(XiceSocket *sock) {
	uv_udp_t* uv = (uv_udp_t*)sock->fileno;
	uv_os_fd_t fd;
	uv_fileno((uv_handle_t*)uv, &fd);
	return (int)fd;
}

static void connect_cb(uv_connect_t* req, int status) {
	XiceSocket* sock = req->data;
	LibuvTcp* tcp = sock->priv;
	uv_read_start((uv_stream_t*)tcp->handle, on_alloc_callback, on_recv_callback);
}

static void on_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  buf->base = malloc(65536);
	buf->len = 65536;
  g_assert(handle != NULL);
  g_assert(suggested_size <= buf->len);
}

void on_recv_callback(uv_stream_t* stream,
	ssize_t nread,
	const uv_buf_t* buf) {
	XiceSocket* sock = stream->data;
	LibuvTcp* tcp = sock->priv;
	if (nread < 0) {
		xice_debug("unexpect error.");
		sock->callback(sock, XICE_SOCKET_ERROR, sock->data, NULL, 0, NULL);
		return;
	}
	if (nread == 0) {
		return;
	}

	sock->callback(sock, XICE_SOCKET_READABLE, sock->data, buf->base, buf->len, &tcp->xaddr);
}

void on_write_callback(uv_write_t* req, int status) {
	UvWriteData* send_data = (UvWriteData*)(req->data);
	XiceSocket* socket = send_data->socket;
	LibuvTcp* tcp = socket->priv;

	send_data->next = tcp->list;
	tcp->list = send_data;

	if (status) {
		xice_debug("libuvtcp write failed : %d", status);
		socket->callback(socket, XICE_SOCKET_ERROR, socket->data, NULL, 0, NULL);
	}
}

static void on_close_callback(uv_handle_t* handle) {
	g_slice_free(uv_tcp_t, (uv_tcp_t*)handle);
}


#endif