/*
 * This file is part of the Xice GLib ICE library.
 *
 * (C) 2008-2009 Collabora Ltd.
 *  Contact: Youness Alaoui
 * (C) 2007-2009 Nokia Corporation. All rights reserved.
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

#ifndef _XICE_DISCOVERY_H
#define _XICE_DISCOVERY_H

/* note: this is a private header to libxice */

#include "stream.h"
#include "agent.h"
#include "xicecontext.h"

typedef struct
{
  XiceAgent *agent;         /**< back pointer to owner */
  XiceCandidateType type;   /**< candidate type STUN or TURN */
  XiceSocket *xicesock;  /**< XXX: should be taken from local cand: existing socket to use */
  XiceAddress server;       /**< STUN/TURN server address */
  GTimeVal next_tick;       /**< next tick timestamp */
  gboolean pending;         /**< is discovery in progress? */
  gboolean done;            /**< is discovery complete? */
  Stream *stream;
  Component *component;
  TurnServer *turn;
  StunAgent stun_agent;
  uint8_t *msn_turn_username;
  uint8_t *msn_turn_password;
  StunTimer timer;
  uint8_t stun_buffer[STUN_MAX_MESSAGE_SIZE];
  StunMessage stun_message;
  uint8_t stun_resp_buffer[STUN_MAX_MESSAGE_SIZE];
  StunMessage stun_resp_msg;
} CandidateDiscovery;

typedef struct
{
  XiceAgent *agent;         /**< back pointer to owner */
  XiceSocket *xicesock;     /**< existing socket to use */
  XiceSocket *relay_socket; /**< relay socket from which we receive */
  XiceAddress server;       /**< STUN/TURN server address */
  Stream *stream;
  Component *component;
  TurnServer *turn;
  StunAgent stun_agent;
  XiceTimer *timer_source;
  XiceTimer *tick_source;
  uint8_t *msn_turn_username;
  uint8_t *msn_turn_password;
  StunTimer timer;
  uint8_t stun_buffer[STUN_MAX_MESSAGE_SIZE];
  StunMessage stun_message;
  uint8_t stun_resp_buffer[STUN_MAX_MESSAGE_SIZE];
  StunMessage stun_resp_msg;
} CandidateRefresh;

void refresh_free_item (gpointer data, gpointer user_data);
void refresh_free (XiceAgent *agent);
void refresh_prune_stream (XiceAgent *agent, guint stream_id);
void refresh_cancel (CandidateRefresh *refresh);


void discovery_free_item (gpointer data, gpointer user_data);
void discovery_free (XiceAgent *agent);
void discovery_prune_stream (XiceAgent *agent, guint stream_id);
void discovery_schedule (XiceAgent *agent);

XiceCandidate *
discovery_add_local_host_candidate (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceAddress *address);

XiceCandidate*
discovery_add_relay_candidate (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceAddress *address,
  XiceSocket *base_socket,
  TurnServer *turn);

XiceCandidate* 
discovery_add_server_reflexive_candidate (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceAddress *address,
  XiceSocket *base_socket);

XiceCandidate* 
discovery_add_peer_reflexive_candidate (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceAddress *address,
  XiceSocket *base_socket,
  XiceCandidate *local,
  XiceCandidate *remote);

XiceCandidate *
discovery_learn_remote_peer_reflexive_candidate (
  XiceAgent *agent,
  Stream *stream,
  Component *component,
  guint32 priority, 
  const XiceAddress *remote_address,
  XiceSocket *udp_socket,
  XiceCandidate *local,
  XiceCandidate *remote);

#endif /*_XICE_CONNCHECK_H */
