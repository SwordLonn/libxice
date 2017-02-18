/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2006-2009 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2006-2009 Nokia Corporation. All rights reserved.
 *  Contact: Kai Vehmanen
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Xice GLib ICE library.
 *
 * The Initial Developers of the Original Code are Collabora Ltd and Nokia
 * Corporation. All Rights Reserved.
 *
 * Contributors:
 *   Dafydd Harries, Collabora Ltd.
 *   Youness Alaoui, Collabora Ltd.
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * the GNU Lesser General Public License Version 2.1 (the "LGPL"), in which
 * case the provisions of LGPL are applicable instead of those above. If you
 * wish to allow use of your version of this file only under the terms of the
 * LGPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replace
 * them with the notice and other provisions required by the LGPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the LGPL.
 */

/*
 * Implementation of UDP socket interface using Berkeley sockets. (See
 * http://en.wikipedia.org/wiki/Berkeley_sockets.)
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif


#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "gioudp.h"
#include "agent/debug.h"

#ifndef G_OS_WIN32
#include <unistd.h>
#endif


static void socket_close (XiceSocket *sock);
static gint socket_recv (XiceSocket *sock, XiceAddress *from,
    guint len, gchar *buf);
static gboolean socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf);
static gboolean socket_is_reliable (XiceSocket *sock);
static int socket_get_fd(XiceSocket *sock);

struct UdpBsdSocketPrivate
{
  XiceAddress xiceaddr;
  GSocketAddress *gaddr;
  GSource* source;
  GMainContext* ctx;
};

static gboolean
socket_callback(
	GSocket *gsocket,
	GIOCondition condition,
	gpointer data)
{
#define MAX_BUFFER_SIZE 65536
	XiceSocket* sock = (XiceSocket* )data;
	GSource* source = g_main_current_source();
	gchar buf[MAX_BUFFER_SIZE];
	gint len;
	XiceAddress addr;
	if (g_source_is_destroyed(source)) {
		xice_debug("Source was destroyed. ");
		return sock->callback(sock, XICE_SOCKET_ERROR, sock->data, NULL, 0, NULL);
	}

	len = socket_recv(sock, &addr, MAX_BUFFER_SIZE, buf);

	if (len < 0) {
		xice_debug("socket recv error.");
		return sock->callback(sock, XICE_SOCKET_ERROR, sock->data, buf, len, &addr);
	}
	else if (len == 0) {
		xice_debug("socket recv nothing.");
		return TRUE;
	}

	return sock->callback(sock, XICE_SOCKET_READABLE, sock->data, buf, len, &addr);
}

static void
attach_socket (
    XiceSocket *socket)
{
  struct UdpBsdSocketPrivate *priv = (struct UdpBsdSocketPrivate*)socket->priv;

  priv->source = g_socket_create_source((GSocket*)socket->fileno, G_IO_IN | G_IO_ERR, NULL);

  g_source_set_callback(priv->source, (GSourceFunc)socket_callback, socket, NULL);

  g_source_attach(priv->source, priv->ctx);

}

XiceSocket *
gio_udp_socket_create(GMainContext* ctx, XiceAddress *addr)
{
  struct sockaddr_storage name;
  XiceSocket *sock = g_slice_new0 (XiceSocket);
  GSocket *gsock = NULL;
  gboolean gret = FALSE;
  GSocketAddress *gaddr;
  struct UdpBsdSocketPrivate *priv;

  if (addr != NULL) {
    xice_address_copy_to_sockaddr(addr, (struct sockaddr *)&name);
  } else {
    memset (&name, 0, sizeof (name));
    name.ss_family = AF_UNSPEC;
  }

  if (name.ss_family == AF_UNSPEC || name.ss_family == AF_INET) {
    gsock = g_socket_new (G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
        G_SOCKET_PROTOCOL_UDP, NULL);
    name.ss_family = AF_INET;
#ifdef HAVE_SA_LEN
    name.ss_len = sizeof (struct sockaddr_in);
#endif
  } else if (name.ss_family == AF_INET6) {
    gsock = g_socket_new (G_SOCKET_FAMILY_IPV6, G_SOCKET_TYPE_DATAGRAM,
        G_SOCKET_PROTOCOL_UDP, NULL);
    name.ss_family = AF_INET6;
#ifdef HAVE_SA_LEN
    name.ss_len = sizeof (struct sockaddr_in6);
#endif
  }

  if (gsock == NULL) {
    g_slice_free (XiceSocket, sock);
    return NULL;
  }

  /* GSocket: All socket file descriptors are set to be close-on-exec. */
  g_socket_set_blocking (gsock, 0);
  gaddr = g_socket_address_new_from_native (&name, sizeof (name));
  if (gaddr != NULL) {
    gret = g_socket_bind (gsock, gaddr, FALSE, NULL);
    g_object_unref (gaddr);
  }

  if (gret == FALSE) {
    g_slice_free (XiceSocket, sock);
    g_socket_close (gsock, NULL);
    g_object_unref (gsock);
    return NULL;
  }

  gaddr = g_socket_get_local_address (gsock, NULL);
  if (gaddr == NULL ||
      !g_socket_address_to_native (gaddr, &name, sizeof(name), NULL)) {
    g_slice_free (XiceSocket, sock);
    g_socket_close (gsock, NULL);
    g_object_unref (gsock);
    return NULL;
  }

  g_object_unref (gaddr);

  xice_address_set_from_sockaddr (&sock->addr, (struct sockaddr *)&name);

  priv = sock->priv = g_slice_new0 (struct UdpBsdSocketPrivate);
  xice_address_init (&priv->xiceaddr);
  priv->ctx = ctx;

  sock->fileno = (gpointer)gsock;
  sock->send = socket_send;
  sock->is_reliable = socket_is_reliable;
  sock->close = socket_close;
  sock->get_fd = socket_get_fd;

  attach_socket(sock);

  return sock;
}

static void
socket_close (XiceSocket *sock)
{
  struct UdpBsdSocketPrivate *priv = sock->priv;

  if (priv->gaddr)
    g_object_unref (priv->gaddr);

  if (priv->source) {
	  g_source_destroy(priv->source);
	  g_source_unref(priv->source);
  }
  if (sock->fileno) {
    g_socket_close ((GSocket*)sock->fileno, NULL);
    g_object_unref ((GSocket*)sock->fileno);
    sock->fileno = NULL;
  }
  g_slice_free(struct UdpBsdSocketPrivate, sock->priv);
}

static gint
socket_recv (XiceSocket *sock, XiceAddress *from, guint len, gchar *buf)
{
  GSocketAddress *gaddr = NULL;
  GError *gerr = NULL;
  gint recvd;

  recvd = g_socket_receive_from ((GSocket*)sock->fileno, &gaddr, buf, len, NULL, &gerr);

  if (recvd < 0) {
    if (g_error_matches(gerr, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)
        || g_error_matches(gerr, G_IO_ERROR, G_IO_ERROR_FAILED))
      recvd = 0;

    g_error_free (gerr);
  }

  if (recvd > 0 && from != NULL && gaddr != NULL) {
    struct sockaddr_storage sa;

    g_socket_address_to_native (gaddr, &sa, sizeof (sa), NULL);
    xice_address_set_from_sockaddr (from, (struct sockaddr *)&sa);
  }

  if (gaddr != NULL)
    g_object_unref (gaddr);

  return recvd;
}

static gboolean
socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf)
{
  struct UdpBsdSocketPrivate *priv = sock->priv;
  size_t sent;

  if (!xice_address_is_valid (&priv->xiceaddr) ||
      !xice_address_equal (&priv->xiceaddr, to)) {
    struct sockaddr_storage sa;
    GSocketAddress *gaddr;

    if (priv->gaddr)
      g_object_unref (priv->gaddr);
    xice_address_copy_to_sockaddr (to, (struct sockaddr *)&sa);
    gaddr = g_socket_address_new_from_native (&sa, sizeof(sa));
    if (gaddr == NULL)
      return -1;
    priv->gaddr = gaddr;
    priv->xiceaddr = *to;
  }

  sent = g_socket_send_to ((GSocket*)sock->fileno, priv->gaddr, buf, len, NULL, NULL);

  return sent == (size_t)len;
}

static gboolean
socket_is_reliable (XiceSocket *sock)
{
  return FALSE;
}

static int socket_get_fd(XiceSocket *sock)
{
	GSocket* gsock = (GSocket*)sock->fileno;
	return g_socket_get_fd(gsock);
}

