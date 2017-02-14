#include "config.h"

#ifdef HAVE_LIBUV
#include "libuvudp.h"
#include "agent/debug.h"

typedef struct _LibuvUdp {
	uv_loop_t* loop;
	uv_udp_t* handle;
	XiceAddress xiceaddr;
	struct sockaddr_storage addr;
}LibuvUdp;

static void socket_close(XiceSocket *sock);
static gint socket_recv(XiceSocket *sock, XiceAddress *from,
	guint len, gchar *buf);
static gboolean socket_send(XiceSocket *sock, const XiceAddress *to,
	guint len, const gchar *buf);
static gboolean socket_is_reliable(XiceSocket *sock);
static int socket_get_fd(XiceSocket *sock);

static void on_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static void on_recv_callback(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
	const struct sockaddr* addr, unsigned int flags);
static void on_send_callback(uv_udp_send_t* req, int status);
static void on_close_callback(uv_handle_t* handle);

XiceSocket* libuv_udp_socket_create(uv_loop_t* loop, XiceAddress* addr) {
	XiceSocket *sock = g_slice_new0(XiceSocket);
	LibuvUdp *uv = g_slice_new0(LibuvUdp);
	struct sockaddr_storage name;
	int flags = 0;

	uv->loop = loop;
	uv->handle = g_slice_new0(uv_udp_t);
	uv->handle->data = sock;

	uv_udp_init(uv->loop, uv->handle);

	sock->priv = uv;
	sock->fileno = (gpointer)uv->handle;
	sock->send = socket_send;
	sock->recv = socket_recv;
	sock->is_reliable = socket_is_reliable;
	sock->close = socket_close;
	sock->get_fd = socket_get_fd;

	xice_address_copy_to_sockaddr(addr, (struct sockaddr *)&name);
	if (name.ss_family == AF_INET6) {
		flags |= UV_UDP_IPV6ONLY;
	}
	xice_address_set_from_sockaddr(&sock->addr, (struct sockaddr *)&name);
	xice_address_init(&uv->xiceaddr);

	uv_udp_bind(uv->handle, (struct sockaddr *)&name, flags);

	uv_udp_recv_start(uv->handle, (uv_alloc_cb)on_alloc_callback, (uv_udp_recv_cb)on_recv_callback);

	return sock;
}

static int socket_get_fd(XiceSocket *sock) {
	uv_udp_t* uv = (uv_udp_t*)sock->fileno;
	uv_os_fd_t fd;
	uv_fileno((uv_handle_t*)uv, &fd);
	return (int)fd;
}

static void socket_close(XiceSocket *sock) {
	LibuvUdp* udp = sock->priv;

	uv_udp_recv_stop(udp->handle);

	uv_close((uv_handle_t*)udp->handle, on_close_callback);

	g_slice_free(LibuvUdp, udp);

}

static gint socket_recv(XiceSocket *sock, XiceAddress *from,
	guint len, gchar *buf) {
	//TODO perhaps we have to turn to another io mode with callback carried datas
}

typedef struct _UvSendData
{
	XiceSocket*    socket;
	uv_udp_send_t req;
	uint8_t       store[1];
}UvSendData;

static gboolean socket_send(XiceSocket *sock, const XiceAddress *to,
	guint len, const gchar *buf) {
	size_t sent;
	LibuvUdp *udp = sock->priv;
	uv_buf_t buffer;
	int err;
	if(!xice_address_is_valid(&udp->xiceaddr) ||
		!xice_address_equal(&udp->xiceaddr, to)) {
		udp->xiceaddr = *to;
		xice_address_copy_to_sockaddr(to, (struct sockaddr *)&udp->addr);
	}

	buffer = uv_buf_init((char*)buf, len);
	sent = uv_udp_try_send(udp->handle, &buffer, 1, (struct sockaddr *)&udp->addr);
	if (sent == (int)len) {
		return TRUE;
	}
	else if (sent >= 0) {
		xice_debug("datagram truncated (juts %d of %d bytes were sent)", sent, len);
		return FALSE;
	}
	else if (sent != UV_EAGAIN) {
		xice_debug("uv_udp_try_send() failed : %s", uv_strerror(sent));
		return FALSE;
	}
	UvSendData* send_data = (UvSendData*)g_alloca(sizeof(UvSendData) + len);
	send_data->socket = sock;
	send_data->req.data = (gpointer)send_data;
	buffer = uv_buf_init((char*)send_data->store, len);
	err = uv_udp_send(&send_data->req, udp->handle, &buffer, 1, (struct sockaddr *)&udp->addr, on_send_callback);
	if (err) {
		xice_debug("uv_udp_send() failed: %s", uv_strerror(err));
		g_free(send_data);
		return FALSE;
	}
	return TRUE;
}

static gboolean socket_is_reliable(XiceSocket *sock) {
	return FALSE;
}

static void on_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
	static guchar slab[65536];
	g_assert(handle != NULL);
	g_assert(suggested_size <= sizeof(slab));
	buf->base = slab;
	buf->len = sizeof(slab);
}

static void on_recv_callback(uv_udp_t* handle, ssize_t nread, const uv_buf_t* buf,
	const struct sockaddr* addr, unsigned int flags) {

}

static void on_send_callback(uv_udp_send_t* req, int status) {
	UvSendData* send_data = (UvSendData*)(req->data);
	XiceSocket* socket = send_data->socket;

	// Delete the UvSendData struct (which includes the uv_req_t and the store char[]).
	g_free(send_data);
	// Just notify the UdpSocket when error.
	//if (status)
	//	socket->onUvSendError(status);
}

static void on_close_callback(uv_handle_t* handle) {
	g_slice_free(uv_udp_t, (uv_udp_t*)handle);
}

#endif