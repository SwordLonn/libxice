/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2006-2010 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2006-2010 Nokia Corporation. All rights reserved.
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
 *   Kai Vehmanen, Nokia
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

#ifndef _XICE_COMPONENT_H
#define _XICE_COMPONENT_H

#include <glib.h>

typedef struct _Component Component;

#include "agent.h"
#include "candidate.h"
#include "stun/stunagent.h"
#include "stun/usages/timer.h"
#include "pseudotcp.h"
#include "stream.h"
#include "contexts/xicesocket.h"

G_BEGIN_DECLS


/* (ICE §4.1.1.1, ID-19)
 * ""For RTP-based media streams, the RTP itself has a component
 * ID of 1, and RTCP a component ID of 2.  If an agent is using RTCP it MUST
 * obtain a candidate for it.  If an agent is using both RTP and RTCP, it
 * would end up with 2*K host candidates if an agent has K interfaces.""
 */

typedef struct _CandidatePair CandidatePair;
typedef struct _CandidatePairKeepalive CandidatePairKeepalive;
typedef struct _IncomingCheck IncomingCheck;

struct _CandidatePairKeepalive
{
  XiceAgent *agent;
  XiceTimer *tick_source;
  guint stream_id;
  guint component_id;
  StunTimer timer;
  uint8_t stun_buffer[STUN_MAX_MESSAGE_SIZE];
  StunMessage stun_message;
};

struct _CandidatePair
{
  XiceCandidate *local;
  XiceCandidate *remote;
  guint64 priority;           /**< candidate pair priority */
  CandidatePairKeepalive keepalive;
};

struct _IncomingCheck
{
  XiceAddress from;
  XiceSocket *local_socket;
  guint32 priority;
  gboolean use_candidate;
  uint8_t *username;
  uint16_t username_len;
};

typedef struct {
  XiceAgent *agent;
  Stream *stream;
  Component *component;
} TcpUserData;

struct _Component
{
  XiceComponentType type;
  guint id;                    /**< component id */
  XiceComponentState state;
  GSList *local_candidates;    /**< list of Candidate objs */
  GSList *remote_candidates;   /**< list of Candidate objs */
  GSList *sockets;             /**< list of XiceSocket objs */
  GSList *gctxs;            /**< list of GSource objs */
  GSList *incoming_checks;     /**< list of IncomingCheck objs */
  GList *turn_servers;             /**< List of TURN servers */
  CandidatePair selected_pair; /**< independent from checklists, 
				    see ICE 11.1. "Sending Media" (ID-19) */
  XiceCandidate *restart_candidate; /**< for storing active remote candidate during a restart */
  XiceAgentRecvFunc g_source_io_cb; /**< function called on io cb */
  gpointer data;                    /**< data passed to the io function */
  //GMainContext *ctx;                /**< context for data callbacks for this
  //                                     component */
  XiceContext *ctx;

  PseudoTcpSocket *tcp;
  XiceTimer* tcp_clock;
  TcpUserData *tcp_data;
  gboolean tcp_readable;
  guint min_port;
  guint max_port;
};

Component *
component_new (guint component_id);

void
component_free (Component *cmp);

gboolean
component_find_pair (Component *cmp, XiceAgent *agent, const gchar *lfoundation, const gchar *rfoundation, CandidatePair *pair);

gboolean
component_restart (Component *cmp);

void
component_update_selected_pair (Component *component, const CandidatePair *pair);

XiceCandidate *
component_find_remote_candidate (const Component *component, const XiceAddress *addr, XiceCandidateTransport transport);

XiceCandidate *
component_set_selected_remote_candidate (XiceAgent *agent, Component *component,
    XiceCandidate *candidate);

G_END_DECLS

#endif /* _XICE_COMPONENT_H */

