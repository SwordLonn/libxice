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

#include "tcp-turn.h"

#include <string.h>
#include <errno.h>
#include <fcntl.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

typedef struct {
  XiceTurnSocketCompatibility compatibility;
  gchar recv_buf[65536];
  guint recv_buf_len;
  guint expecting_len;
  XiceSocket *base_socket;
} TurnTcpPriv;

#define MAX_UDP_MESSAGE_SIZE 65535

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

XiceSocket *
xice_tcp_turn_socket_new (XiceSocket *base_socket,
    XiceTurnSocketCompatibility compatibility)
{
  TurnTcpPriv *priv;
  XiceSocket *sock = g_slice_new0 (XiceSocket);
  sock->priv = priv = g_slice_new0 (TurnTcpPriv);

  priv->compatibility = compatibility;
  priv->base_socket = base_socket;
  xice_socket_set_callback(base_socket, read_callback, sock);
  sock->fileno = priv->base_socket->fileno;
  sock->addr = priv->base_socket->addr;
  sock->send = socket_send;
//  sock->recv = socket_recv;
  sock->is_reliable = socket_is_reliable;
  sock->close = socket_close;
  sock->get_fd = priv->base_socket->get_fd;

  return sock;
}


static void
socket_close (XiceSocket *sock)
{
  TurnTcpPriv *priv = sock->priv;

  if (priv->base_socket)
    xice_socket_free (priv->base_socket);

  g_slice_free(TurnTcpPriv, sock->priv);
}

static gboolean read_callback(
	XiceSocket *socket,
	XiceSocketCondition condition,
	gpointer data,
	gchar *buf,
	guint len,
	XiceAddress *from) {
  XiceSocket* sock = data;
  TurnTcpPriv *priv = sock->priv;
  guint padlen;
  guint headerlen = 0;
  guint copy = 0;

  if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_DRAFT9 ||
	  priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_RFC5766)
	  headerlen = 4;
  else if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_GOOGLE)
	  headerlen = 2;
  else
	  return FALSE;
  //do our best to reduse memory copy.
  if (len < 0 || condition != XICE_SOCKET_READABLE) {
	  return sock->callback(sock, condition, sock->data, buf, len, from);
  }
next:
  if (len == 0) {
	  return TRUE;
  }

  if (priv->expecting_len == 0 && priv->recv_buf_len == 0) {
	if (len < headerlen) {
		memcpy(priv->recv_buf, buf, len);
		priv->recv_buf_len = len;
		return TRUE;
	}
    if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_DRAFT9 ||
        priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_RFC5766) {
      guint16 magic = ntohs (*(guint16*)buf);
      guint16 packetlen = ntohs (*(guint16*)(buf + 2));
	  priv->expecting_len = (magic < 0x4000 ? 20 : 4) + packetlen;
	  padlen = (priv->expecting_len % 4) ? 4 - (priv->expecting_len % 4) : 0;
    }
    else if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_GOOGLE) {
      guint len = ntohs (*(guint16*)buf);
      priv->expecting_len = len;
	  padlen = 0;
    }

	if (len >= priv->expecting_len + padlen) {
		if (sock->callback) {
			sock->callback(sock, XICE_SOCKET_READABLE, sock->data, buf, priv->expecting_len + padlen, from);
		}
		buf += priv->expecting_len + padlen;
		len -= priv->expecting_len + padlen;
		priv->recv_buf_len = 0;
		priv->expecting_len = 0;
		goto next;
	}
	else {
		priv->expecting_len = priv->expecting_len + padlen - len;
		memcpy(priv->recv_buf, buf, len);
		priv->recv_buf_len = len;
		return TRUE;
	}
  }
  else {
	  if (priv->expecting_len == 0) {
		  if (len + priv->recv_buf_len < headerlen) {
			  memcpy(priv->recv_buf + priv->recv_buf_len, buf, len);
			  priv->recv_buf_len += len;
			  return TRUE;
		  }
		  if (priv->recv_buf_len < headerlen) {
			  memcpy(priv->recv_buf + priv->recv_buf_len, buf, headerlen - priv->recv_buf_len);
			  buf += headerlen - priv->recv_buf_len;
			  len -= headerlen - priv->recv_buf_len;
			  priv->recv_buf_len = headerlen;
		  }
		  if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_DRAFT9 ||
			  priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_RFC5766) {
			  guint16 magic = ntohs(*(guint16*)priv->recv_buf);
			  guint16 packetlen = ntohs(*(guint16*)(priv->recv_buf + 2));
			  priv->expecting_len = (magic < 0x4000 ? 20 : 4) + packetlen;
			  padlen = (priv->expecting_len % 4) ? 4 - (priv->expecting_len % 4) : 0;
		  }
		  else if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_GOOGLE) {
			  guint len = ntohs(*(guint16*)priv->recv_buf);
			  priv->expecting_len = len;
			  padlen = 0;
		  }			  
	  }
	  copy = min(len, padlen + priv->expecting_len - priv->recv_buf_len);
	  memcpy(priv->recv_buf + priv->recv_buf_len, buf, copy);
	  buf += copy;
	  len -= copy;
	  priv->recv_buf_len += copy;

	  if (priv->recv_buf_len >= padlen + priv->expecting_len) {
		  if (sock->callback) {
			  sock->callback(sock, XICE_SOCKET_READABLE, sock->data, priv->recv_buf, priv->expecting_len + padlen, from);
		  }
		  memmove(priv->recv_buf, priv->recv_buf + priv->expecting_len + padlen, priv->recv_buf_len - priv->expecting_len - padlen);
		  priv->recv_buf_len -= priv->expecting_len + padlen;
		  priv->expecting_len = 0;
		  goto next;
	  }
	  else {
		  priv->expecting_len -= priv->recv_buf_len - padlen;
		  return TRUE;
	  }
  }

  return TRUE;
}

static gboolean
socket_send (XiceSocket *sock, const XiceAddress *to,
    guint len, const gchar *buf)
{
  TurnTcpPriv *priv = sock->priv;
  gchar padbuf[3] = {0, 0, 0};
  int padlen = (len%4) ? 4 - (len%4) : 0;
  gchar buffer[MAX_UDP_MESSAGE_SIZE + sizeof(guint16) + sizeof(padbuf)];
  guint buffer_len = 0;

  if (priv->compatibility != XICE_TURN_SOCKET_COMPATIBILITY_DRAFT9 &&
      priv->compatibility != XICE_TURN_SOCKET_COMPATIBILITY_RFC5766)
    padlen = 0;

  if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_GOOGLE) {
    guint16 tmpbuf = htons (len);
    memcpy (buffer + buffer_len, (gchar *)&tmpbuf, sizeof(guint16));
    buffer_len += sizeof(guint16);
  }

  memcpy (buffer + buffer_len, buf, len);
  buffer_len += len;

  if (priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_DRAFT9 ||
      priv->compatibility == XICE_TURN_SOCKET_COMPATIBILITY_RFC5766) {
    memcpy (buffer + buffer_len, padbuf, padlen);
    buffer_len += padlen;
  }
  return xice_socket_send (priv->base_socket, to, buffer_len, buffer);

}


static gboolean
socket_is_reliable (XiceSocket *sock)
{
  return TRUE;
}

