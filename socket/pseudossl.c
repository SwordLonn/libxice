/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2008-2009 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2008-2009 Nokia Corporation. All rights reserved.
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
 * Implementation of TCP relay socket interface using TCP Berkeley sockets. (See
 * http://en.wikipedia.org/wiki/Berkeley_sockets.)
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "pseudossl.h"
#include "agent/debug.h"
#include <string.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

typedef struct {
  gboolean handshaken;
  XiceSocket *base_socket;
  GQueue send_queue;
} PseudoSSLPriv;


struct to_be_sent {
  guint length;
  gchar *buf;
  XiceAddress to;
};


static const gchar SSL_SERVER_HANDSHAKE[] = {
  0x16, 0x03, 0x01, 0x00, 0x4a, 0x02, 0x00, 0x00,
  0x46, 0x03, 0x01, 0x42, 0x85, 0x45, 0xa7, 0x27,
  0xa9, 0x5d, 0xa0, 0xb3, 0xc5, 0xe7, 0x53, 0xda,
  0x48, 0x2b, 0x3f, 0xc6, 0x5a, 0xca, 0x89, 0xc1,
  0x58, 0x52, 0xa1, 0x78, 0x3c, 0x5b, 0x17, 0x46,
  0x00, 0x85, 0x3f, 0x20, 0x0e, 0xd3, 0x06, 0x72,
  0x5b, 0x5b, 0x1b, 0x5f, 0x15, 0xac, 0x13, 0xf9,
  0x88, 0x53, 0x9d, 0x9b, 0xe8, 0x3d, 0x7b, 0x0c,
  0x30, 0x32, 0x6e, 0x38, 0x4d, 0xa2, 0x75, 0x57,
  0x41, 0x6c, 0x34, 0x5c, 0x00, 0x04, 0x00};

static const gchar SSL_CLIENT_HANDSHAKE[] = {
  0x80, 0x46, 0x01, 0x03, 0x01, 0x00, 0x2d, 0x00,
  0x00, 0x00, 0x10, 0x01, 0x00, 0x80, 0x03, 0x00,
  0x80, 0x07, 0x00, 0xc0, 0x06, 0x00, 0x40, 0x02,
  0x00, 0x80, 0x04, 0x00, 0x80, 0x00, 0x00, 0x04,
  0x00, 0xfe, 0xff, 0x00, 0x00, 0x0a, 0x00, 0xfe,
  0xfe, 0x00, 0x00, 0x09, 0x00, 0x00, 0x64, 0x00,
  0x00, 0x62, 0x00, 0x00, 0x03, 0x00, 0x00, 0x06,
  0x1f, 0x17, 0x0c, 0xa6, 0x2f, 0x00, 0x78, 0xfc,
  0x46, 0x55, 0x2e, 0xb1, 0x83, 0x39, 0xf1, 0xea};

static gboolean read_callback(
	XiceSocket *socket,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from);

static void socket_close (XiceSocket *sock);

static gboolean socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf);
static gboolean socket_is_reliable (XiceSocket *sock);

static void add_to_be_sent (XiceSocket *sock, const XiceAddress *to,
    const gchar *buf, guint len);
static void free_to_be_sent (struct to_be_sent *tbs);


XiceSocket *
xice_pseudossl_socket_new (XiceSocket *base_socket)
{
  PseudoSSLPriv *priv;
  XiceSocket *sock = g_slice_new0 (XiceSocket);
  sock->priv = priv = g_slice_new0 (PseudoSSLPriv);

  priv->handshaken = FALSE;
  priv->base_socket = base_socket;

  xice_socket_set_callback(base_socket, read_callback, sock);
  sock->fileno = priv->base_socket->fileno;
  sock->addr = priv->base_socket->addr;
  sock->send = socket_send;
  sock->is_reliable = socket_is_reliable;
  sock->close = socket_close;
  sock->get_fd = priv->base_socket->get_fd;

  /* We send 'to' NULL because it will always be to an already connected
   * TCP base socket, which ignores the destination */
  xice_socket_send (priv->base_socket, NULL,
      sizeof(SSL_CLIENT_HANDSHAKE), SSL_CLIENT_HANDSHAKE);

  return sock;
}


static void
socket_close (XiceSocket *sock)
{
  PseudoSSLPriv *priv = sock->priv;

  if (priv->base_socket)
    xice_socket_free (priv->base_socket);

  g_queue_foreach (&priv->send_queue, (GFunc) free_to_be_sent, NULL);
  g_queue_clear (&priv->send_queue);

  g_slice_free(PseudoSSLPriv, sock->priv);
}

static gboolean read_callback(
	XiceSocket *base_sock,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from) {
	XiceSocket* sock = (XiceSocket*)data;
	PseudoSSLPriv *priv = sock->priv;
	if (len <= 0 || condition != XICE_SOCKET_READABLE) {
		goto error;
	}

	if (priv->handshaken) {
		if (priv->base_socket && sock->callback) {
			return sock->callback(sock, XICE_SOCKET_READABLE, sock->data, buf, len, from);
		}
		else {
			return FALSE;
		}
	}

	if (len >= sizeof(SSL_SERVER_HANDSHAKE) &&
		memcmp(SSL_SERVER_HANDSHAKE, buf, sizeof(SSL_SERVER_HANDSHAKE)) == 0) {
		//
		struct to_be_sent *tbs = NULL;
		priv->handshaken = TRUE;
		while ((tbs = g_queue_pop_head(&priv->send_queue))) {
			xice_socket_send(priv->base_socket, &tbs->to, tbs->length, tbs->buf);
			g_free(tbs->buf);
			g_slice_free(struct to_be_sent, tbs);
		}
		if (sock->callback && len > sizeof(SSL_SERVER_HANDSHAKE)) {
			return sock->callback(sock, XICE_SOCKET_READABLE, sock->data, 
				buf + sizeof(SSL_CLIENT_HANDSHAKE), len - sizeof(SSL_CLIENT_HANDSHAKE),
				from);
		}
		return TRUE;
	}
error:
	if (priv->base_socket)
		xice_socket_free(priv->base_socket);
	priv->base_socket = NULL;
	if (condition == XICE_SOCKET_READABLE)
		condition = XICE_SOCKET_ERROR;
	sock->callback(sock, condition, sock->data, NULL, 0, NULL);
	return FALSE;
}

static gboolean
socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf)
{
  PseudoSSLPriv *priv = sock->priv;

  if (priv->handshaken) {
    if (priv->base_socket)
      return xice_socket_send (priv->base_socket, to, len, buf);
    else
      return FALSE;
  } else {
    add_to_be_sent (sock, to, buf, len);
  }
  return TRUE;
}


static gboolean
socket_is_reliable (XiceSocket *sock)
{
  return TRUE;
}


static void
add_to_be_sent (XiceSocket *sock, const XiceAddress *to,
    const gchar *buf, guint len)
{
  PseudoSSLPriv *priv = sock->priv;
  struct to_be_sent *tbs = NULL;

  if (len <= 0)
    return;

  tbs = g_slice_new0 (struct to_be_sent);
  tbs->buf = g_memdup (buf, len);
  tbs->length = len;
  if (to)
    tbs->to = *to;
  g_queue_push_tail (&priv->send_queue, tbs);

}

static void
free_to_be_sent (struct to_be_sent *tbs)
{
  g_free (tbs->buf);
  g_slice_free (struct to_be_sent, tbs);
}
