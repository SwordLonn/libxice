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

/*
 * @file candidate.c
 * @brief ICE candidate functions
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
#define XICEAPI_EXPORT
#endif

#include <string.h>

#include "agent.h"
#include "component.h"

/* (ICE 4.1.1 "Gathering Candidates") ""Every candidate is a transport
 * address. It also has a type and a base. Three types are defined and 
 * gathered by this specification - host candidates, server reflexive 
 * candidates, and relayed candidates."" (ID-19) */

XICEAPI_EXPORT XiceCandidate *
xice_candidate_new (XiceCandidateType type)
{
  XiceCandidate *candidate;

  candidate = g_slice_new0 (XiceCandidate);
  candidate->type = type;
  return candidate;
}


XICEAPI_EXPORT void
xice_candidate_free (XiceCandidate *candidate)
{
  /* better way of checking if socket is allocated? */

  if (candidate->username)
    g_free (candidate->username);

  if (candidate->password)
    g_free (candidate->password);

  g_slice_free (XiceCandidate, candidate);
}


guint32
xice_candidate_jingle_priority (XiceCandidate *candidate)
{
  switch (candidate->type)
    {
    case XICE_CANDIDATE_TYPE_HOST:             return 1000;
    case XICE_CANDIDATE_TYPE_SERVER_REFLEXIVE: return 900;
    case XICE_CANDIDATE_TYPE_PEER_REFLEXIVE:   return 900;
    case XICE_CANDIDATE_TYPE_RELAYED:          return 500;
    }

  /* appease GCC */
  return 0;
}

guint32
xice_candidate_msn_priority (XiceCandidate *candidate)
{
  switch (candidate->type)
    {
    case XICE_CANDIDATE_TYPE_HOST:             return 830;
    case XICE_CANDIDATE_TYPE_SERVER_REFLEXIVE: return 550;
    case XICE_CANDIDATE_TYPE_PEER_REFLEXIVE:   return 550;
    case XICE_CANDIDATE_TYPE_RELAYED:          return 450;
    }

  /* appease GCC */
  return 0;
}


/*
 * ICE 4.1.2.1. "Recommended Formula" (ID-19):
 * returns number between 1 and 0x7effffff 
 */
guint32
xice_candidate_ice_priority_full (
  // must be ∈ (0, 126) (max 2^7 - 2)
  guint type_preference,
  // must be ∈ (0, 65535) (max 2^16 - 1)
  guint local_preference,
  // must be ∈ (0, 255) (max 2 ^ 8 - 1)
  guint component_id)
{
  return (
      0x1000000 * type_preference +
      0x100 * local_preference +
      (0x100 - component_id));
}


guint32
xice_candidate_ice_priority (const XiceCandidate *candidate)
{
  guint8 type_preference = 0;

  switch (candidate->type)
    {
    case XICE_CANDIDATE_TYPE_HOST:
      type_preference = XICE_CANDIDATE_TYPE_PREF_HOST; break;
    case XICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
      type_preference = XICE_CANDIDATE_TYPE_PREF_PEER_REFLEXIVE; break;
    case XICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
      type_preference = XICE_CANDIDATE_TYPE_PREF_SERVER_REFLEXIVE; break;
    case XICE_CANDIDATE_TYPE_RELAYED:
      type_preference = XICE_CANDIDATE_TYPE_PREF_RELAYED; break;
    }

  /* return _candidate_ice_priority (type_preference, 1, candidate->component_id); */
  return xice_candidate_ice_priority_full (type_preference, 1, candidate->component_id);
}

/*
 * Calculates the pair priority as specified in ICE
 * sect 5.7.2. "Computing Pair Priority and Ordering Pairs" (ID-19).
 */
guint64
xice_candidate_pair_priority (guint32 o_prio, guint32 a_prio)
{
  guint32 max = o_prio > a_prio ? o_prio : a_prio;
  guint32 min = o_prio < a_prio ? o_prio : a_prio;

  return ((guint64)1 << 32) * min + 2 * max + (o_prio > a_prio ? 1 : 0);
}

/*
 * Copies a candidate
 */
XICEAPI_EXPORT XiceCandidate *
xice_candidate_copy (const XiceCandidate *candidate)
{
  XiceCandidate *copy = xice_candidate_new (candidate->type);

  memcpy (copy, candidate, sizeof(XiceCandidate));

  copy->username = g_strdup (copy->username);
  copy->password = g_strdup (copy->password);

  return copy;
}
