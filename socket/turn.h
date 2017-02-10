/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2008 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2008 Nokia Corporation
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

#ifndef _TURN_H
#define _TURN_H


typedef enum {
  XICE_TURN_SOCKET_COMPATIBILITY_DRAFT9,
  XICE_TURN_SOCKET_COMPATIBILITY_GOOGLE,
  XICE_TURN_SOCKET_COMPATIBILITY_MSN,
  XICE_TURN_SOCKET_COMPATIBILITY_OC2007,
  XICE_TURN_SOCKET_COMPATIBILITY_RFC5766,
} XiceTurnSocketCompatibility;


#include "contexts/xicesocket.h"
#include "stun/stunmessage.h"
#include "contexts/xicecontext.h"

G_BEGIN_DECLS

gint
xice_turn_socket_parse_recv (XiceSocket *sock, XiceSocket **from_sock,
    XiceAddress *from, guint len, gchar *buf,
    XiceAddress *recv_from, gchar *recv_buf, guint recv_len);

gboolean
xice_turn_socket_set_peer (XiceSocket *sock, XiceAddress *peer);

XiceSocket*
xice_turn_socket_new (XiceContext *ctx, XiceAddress *addr,
    XiceSocket *base_socket, XiceAddress *server_addr,
    gchar *username, gchar *password, XiceTurnSocketCompatibility compatibility);

void
xice_turn_socket_set_ms_realm(XiceSocket *sock, StunMessage *msg);

void
xice_turn_socket_set_ms_connection_id (XiceSocket *sock, StunMessage *msg);


G_END_DECLS

#endif /* _TURN_H */

