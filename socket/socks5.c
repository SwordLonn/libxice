/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2008 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2008 Nokia Corporation. All rights reserved.
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

#include "socks5.h"

#include <string.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

typedef enum {
  SOCKS_STATE_INIT,
  SOCKS_STATE_AUTH,
  SOCKS_STATE_CONNECT,
  SOCKS_STATE_CONNECTED,
  SOCKS_STATE_ERROR
} SocksState;

typedef struct {
  SocksState state;
  XiceSocket *base_socket;
  XiceAddress addr;
  gchar *username;
  gchar *password;
  GQueue send_queue;
  gchar *recv_buf;
  gint recv_len;
} Socks5Priv;


struct to_be_sent {
  guint length;
  gchar *buf;
  XiceAddress to;
};

static gboolean read_callback(
	XiceSocket *socket,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from);

static void socket_close (XiceSocket *sock);
//static gint socket_recv (XiceSocket *sock, XiceAddress *from,
//    guint len, gchar *buf);
static gboolean socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf);
static gboolean socket_is_reliable (XiceSocket *sock);

static void add_to_be_sent (XiceSocket *sock, const XiceAddress *to,
    const gchar *buf, guint len);
static void free_to_be_sent (struct to_be_sent *tbs);


XiceSocket *
xice_socks5_socket_new (XiceSocket *base_socket,
    XiceAddress *addr, gchar *username, gchar *password)
{
  Socks5Priv *priv;
  XiceSocket *sock = NULL;

  if (addr) {
    sock = g_slice_new0 (XiceSocket);
    sock->priv = priv = g_slice_new0 (Socks5Priv);

    priv->base_socket = base_socket;
    priv->addr = *addr;
    priv->username = g_strdup (username);
    priv->password = g_strdup (password);

	xice_socket_set_callback(base_socket, read_callback, sock);

    sock->fileno = priv->base_socket->fileno;
    sock->addr = priv->base_socket->addr;
    sock->send = socket_send;
    //sock->recv = socket_recv;
    sock->is_reliable = socket_is_reliable;
    sock->close = socket_close;
	sock->get_fd = priv->base_socket->get_fd;

    /* Send SOCKS5 handshake */
    {
      gchar msg[4];
      gint len = 3;

      msg[0] = 0x05; /* SOCKS version */
      msg[1] = 0x01; /* number of methods supported */
      msg[2] = 0x00; /* no authentication method*/

      g_debug ("user/pass : %s - %s", username, password);
      /* add support for authentication method */
      if (username || password) {
        msg[1] = 0x02; /* number of methods supported */
        msg[3] = 0x02; /* authentication method */
        len++;
      }

      /* We send 'to' NULL because it will always be to an already connected
       * TCP base socket, which ignores the destination */
      xice_socket_send (priv->base_socket, NULL, len, msg);
      priv->state = SOCKS_STATE_INIT;
    }
  }

  return sock;
}


static void
socket_close (XiceSocket *sock)
{
  Socks5Priv *priv = sock->priv;

  if (priv->base_socket)
    xice_socket_free (priv->base_socket);

  if (priv->username)
    g_free (priv->username);

  if (priv->password)
    g_free (priv->password);
  if (priv->recv_len > 0) {
	  g_free(priv->recv_buf);
  }

  g_queue_foreach (&priv->send_queue, (GFunc) free_to_be_sent, NULL);
  g_queue_clear (&priv->send_queue);

  g_slice_free(Socks5Priv, sock->priv);
}

static gboolean read_callback(
	XiceSocket *base_socket,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from) {
	XiceSocket* sock = (XiceSocket*)data;
	Socks5Priv *priv = sock->priv;
	if (len <= 0) {
		return FALSE;
	}
	if (priv->state == SOCKS_STATE_CONNECTED) {
		if (priv->base_socket && sock->callback) {
			return sock->callback(sock, XICE_SOCKET_READABLE, sock->data, buf, len, &priv->addr);
		} else {
			return FALSE;
		}
	}
	priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len + len);
	memcpy(priv->recv_buf + priv->recv_len, buf, len);
	priv->recv_len += len;
next:
	switch (priv->state) {
	case SOCKS_STATE_CONNECTED:
	{
		guint read = priv->recv_len;
		gboolean ret = FALSE;
		if (sock->callback && priv->recv_len > 0) {
			ret = sock->callback(sock, XICE_SOCKET_READABLE, sock->data, priv->recv_buf, read, from);
		}
		priv->recv_len -= read;
		memmove(priv->recv_buf, priv->recv_buf + read, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);
		
		return ret;
	}
	case SOCKS_STATE_INIT:
	{
		gchar data[2];
		guint last = priv->recv_len;

		xice_debug("Socks5 state Init");

		if (last < 2)
			goto not_enough_data;

		data[0] = priv->recv_buf[0];
		data[1] = priv->recv_buf[1];
		priv->recv_len -= 2;
		memmove(priv->recv_buf, priv->recv_buf + 2, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);

		if (data[0] != 0x05) {
			goto error;
		}
		if (data[1] != 0x02 && data[1] != 0x00)
			goto error;
		if (data[1] == 0x02) {
			gchar msg[515];
			gint len = 0;
			if (!priv->username && !priv->password)
				goto error;

			gint ulen = 0;
			gint plen = 0;

			if (priv->username)
				ulen = strlen(priv->username);
			if (ulen > 255) {
				xice_debug("Socks5 username length > 255");
				goto error;
			}

			if (priv->password)
				plen = strlen(priv->password);
			if (plen > 255) {
				xice_debug("Socks5 password length > 255");
				goto error;
			}

			msg[len++] = 0x01; /* auth version */
			msg[len++] = ulen; /* username length */
			if (ulen > 0)
				memcpy(msg + len, priv->username, ulen); /* Username */
			len += ulen;
			msg[len++] = plen; /* Password length */
			if (plen > 0)
				memcpy(msg + len, priv->password, plen); /* Password */
			len += plen;

			xice_socket_send(priv->base_socket, NULL, len, msg);
			priv->state = SOCKS_STATE_AUTH;

			goto next;
		}
		else if (data[1] == 0x00) {
			goto send_connect;
		}
	}
	break;
	case SOCKS_STATE_AUTH:
	{
		gchar* data = priv->recv_buf;
		guint last = priv->recv_len;

		xice_debug("Socks5 state auth");
		
		if (last < 2)
			goto not_enough_data;
		if (data[0] != 0x01 || data[1] != 0x00)
			goto error;
		priv->recv_len -= 2;
		memmove(priv->recv_buf, priv->recv_buf + 2, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);
		
		goto send_connect;
	}
	break;
	case SOCKS_STATE_CONNECT:
	{
		gchar* data = priv->recv_buf;
		guint last = priv->recv_len;
		struct to_be_sent *tbs = NULL;
		guint skip = 0;
		if (last < 4)
			goto not_enough_data;
		if (data[0] != 0x05)
			goto error;
		if (data[1] != 0x00)
			goto error;
		if (data[2] != 0x00)
			goto error;
		if (data[3] != 0x01 && data[3] != 0x04)
			goto error;

		if (data[3] == 0x01 && last < 6 + 4)
			goto not_enough_data;

		if (data[3] == 0x04 && last < 18 + 4)
			goto not_enough_data;
		
		skip = data[3] == 0x01 ? 6 : 18;
		
		priv->recv_len -= skip;
		memmove(priv->recv_buf, priv->recv_buf + 2, priv->recv_len);
		priv->recv_buf = g_realloc(priv->recv_buf, priv->recv_len);

		while ((tbs = g_queue_pop_head(&priv->send_queue))) {
			xice_socket_send(priv->base_socket, &tbs->to,
				tbs->length, tbs->buf);
			g_free(tbs->buf);
			g_slice_free(struct to_be_sent, tbs);
		}
		priv->state = SOCKS_STATE_CONNECTED;
		goto next;
	}
	break;
	default:
		/* Unknown status */
		goto error;
	}
not_enough_data:
	return TRUE;

send_connect:
	{
		gchar msg[22];
		gint len = 0;
		struct sockaddr_storage name;
		xice_address_copy_to_sockaddr(&priv->addr, (struct sockaddr *)&name);

		msg[len++] = 0x05; /* SOCKS version */
		msg[len++] = 0x01; /* connect command */
		msg[len++] = 0x00; /* reserved */
		if (name.ss_family == AF_INET) {
			msg[len++] = 0x01; /* IPV4 address type */
							   /* Address */
			memcpy(msg + len, &((struct sockaddr_in *) &name)->sin_addr, 4);
			len += 4;
			/* Port */
			memcpy(msg + len, &((struct sockaddr_in *) &name)->sin_port, 2);
			len += 2;
		}
		else if (name.ss_family == AF_INET6) {
			msg[len++] = 0x04; /* IPV6 address type */
							   /* Address */
			memcpy(msg + len, &((struct sockaddr_in6 *) &name)->sin6_addr, 16);
			len += 16;
			/* Port */
			memcpy(msg + len, &((struct sockaddr_in6 *) &name)->sin6_port, 2);
			len += 2;
		}

		xice_socket_send(priv->base_socket, NULL, len, msg);
		priv->state = SOCKS_STATE_CONNECT;

		goto next;
	}
error:
	xice_debug("Socks5 error");
	if (priv->base_socket)
		xice_socket_free(priv->base_socket);
	priv->base_socket = NULL;
	priv->state = SOCKS_STATE_ERROR;
	if (condition == XICE_SOCKET_READABLE)
		condition = XICE_SOCKET_ERROR;
	sock->callback(sock, condition, sock->data, NULL, 0, NULL);

	return FALSE;

}

static gboolean
socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf)
{
  Socks5Priv *priv = sock->priv;

  if (priv->state == SOCKS_STATE_CONNECTED) {
    if (priv->base_socket)
      return xice_socket_send (priv->base_socket, to, len, buf);
    else
      return FALSE;
  } else if (priv->state == SOCKS_STATE_ERROR) {
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
  Socks5Priv *priv = sock->priv;
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
