/*
 * This file is part of the Xice GLib ICE library.
 *
 * Unit test for ICE in dribble mode (adding remote candidates while the
 * machine is running).
 *
 * (C) 2007 Nokia Corporation. All rights reserved.
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "agent.h"

#include <stdlib.h>
#include <string.h>
#include <uv.h>

static XiceComponentState global_lagent_state = XICE_COMPONENT_STATE_LAST;
static XiceComponentState global_ragent_state = XICE_COMPONENT_STATE_LAST;
static guint global_components_ready = 0;
static guint global_components_ready_exit = 0;
static guint global_components_failed = 0;
static guint global_components_failed_exit = 0;
static uv_loop_t *global_mainloop = NULL;
static gboolean global_lagent_gathering_done = FALSE;
static gboolean global_ragent_gathering_done = FALSE;
static gboolean global_lagent_ibr_received = FALSE;
static gboolean global_ragent_ibr_received = FALSE;
static int global_lagent_cands = 0;
static int global_ragent_cands = 0;
static gint global_ragent_read = 0;

static void priv_print_global_status (void)
{
  g_debug ("\tgathering_done=%d", global_lagent_gathering_done && global_ragent_gathering_done);
  g_debug ("\tlstate=%d", global_lagent_state);
  g_debug ("\trstate=%d", global_ragent_state);
  g_debug ("\tL cands=%d R cands=%d", global_lagent_cands, global_ragent_cands);
}

static void timer_cb (uv_timer_t *handle)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, handle);

  /* signal status via a global variable */

  /* note: should not be reached, abort */
  g_error ("ERROR: test has got stuck, aborting...");

  return FALSE;
}

static void quit_loop_cb (uv_handle_t h)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, h);

  uv_stop(global_mainloop);
  // uv_loop_close(global_mainloop);
  return FALSE;
}

static void cb_xice_recv (XiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, user_data);

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)component_id; (void)buf;

  /*
   * Lets ignore stun packets that got through
   */
  if (len < 8)
    return;
  if (strncmp ("12345678", buf, 8))
    return;

  if (GPOINTER_TO_UINT (user_data) == 2) {
    global_ragent_read = len;
    uv_stop(global_mainloop);
    // uv_loop_close(global_mainloop);
  }
}

static void cb_candidate_gathering_done(XiceAgent *agent, guint stream_id, gpointer data)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, data);

  if (GPOINTER_TO_UINT (data) == 1)
    global_lagent_gathering_done = TRUE;
  else if (GPOINTER_TO_UINT (data) == 2)
    global_ragent_gathering_done = TRUE;

  if (global_lagent_gathering_done &&
    global_ragent_gathering_done) {
    uv_stop(global_mainloop);
    // uv_loop_close(global_mainloop);
  }

  /* XXX: dear compiler, these are for you: */
  (void)agent;
}

static void cb_component_state_changed (XiceAgent *agent, guint stream_id, guint component_id, guint state, gpointer data)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, data);

  if (GPOINTER_TO_UINT (data) == 1)
    global_lagent_state = state;
  else if (GPOINTER_TO_UINT (data) == 2)
    global_ragent_state = state;
  
  if (state == XICE_COMPONENT_STATE_READY)
    global_components_ready++;
  if (state == XICE_COMPONENT_STATE_FAILED)
    global_components_failed++;

  g_debug ("test-dribble: checks READY/EXIT-AT %u/%u.", global_components_ready, global_components_ready_exit);
  g_debug ("test-dribble: checks FAILED/EXIT-AT %u/%u.", global_components_failed, global_components_failed_exit);

  /* signal status via a global variable */
  if (global_components_ready == global_components_ready_exit &&
      global_components_failed == global_components_failed_exit) {
    uv_stop(global_mainloop);
    // uv_loop_close(global_mainloop);
    return;
  }

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)data; (void)component_id;
}

static void cb_new_selected_pair(XiceAgent *agent, guint stream_id, guint component_id, 
				 gchar *lfoundation, gchar* rfoundation, gpointer data)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, data);

  if (GPOINTER_TO_UINT (data) == 1)
    ++global_lagent_cands;
  else if (GPOINTER_TO_UINT (data) == 2)
    ++global_ragent_cands;

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)component_id; (void)lfoundation; (void)rfoundation;
}

static void cb_new_candidate(XiceAgent *agent, guint stream_id, guint component_id, 
			     gchar *foundation, gpointer data)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, data);

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)data; (void)component_id; (void)foundation;
}

static void cb_initial_binding_request_received(XiceAgent *agent, guint stream_id, gpointer data)
{
  g_debug ("test-dribble:%s: %p", G_STRFUNC, data);

  if (GPOINTER_TO_UINT (data) == 1)
    global_lagent_ibr_received = TRUE;
  else if (GPOINTER_TO_UINT (data) == 2)
    global_ragent_ibr_received = TRUE;

  /* XXX: dear compiler, these are for you: */
  (void)agent; (void)stream_id; (void)data;
}


int main (void)
{
  XiceAgent *lagent, *ragent;      /* agent's L and R */
  XiceAddress baseaddr;
  uv_timer_t timer_id;
  GSList *cands, *i;
  guint ls_id, rs_id;
  XiceContext* context;
#ifdef G_OS_WIN32
  WSADATA w;

  WSAStartup(0x0202, &w);
#endif

  g_type_init ();
#if !GLIB_CHECK_VERSION(2,31,8)
  g_thread_init (NULL);
#endif

  global_mainloop = malloc(sizeof(uv_loop_t));
  uv_loop_init(global_mainloop);
  context = xice_context_create("libuv", (gpointer)global_mainloop);
  /* step: create the agents L and R */
  lagent = xice_agent_new (context,
      XICE_COMPATIBILITY_GOOGLE);
  ragent = xice_agent_new (context,
      XICE_COMPATIBILITY_GOOGLE);

  if (!xice_address_set_from_string (&baseaddr, "127.0.0.1"))
    g_assert_not_reached ();
  xice_agent_add_local_address (lagent, &baseaddr);
  xice_agent_add_local_address (ragent, &baseaddr);

  /* step: add a timer to catch state changes triggered by signals */
  uv_timer_init(global_mainloop, &timer_id);
  uv_timer_start(&timer_id, timer_cb, 30000, 30000);
  


  g_signal_connect (G_OBJECT (lagent), "candidate-gathering-done",
      G_CALLBACK (cb_candidate_gathering_done), GUINT_TO_POINTER(1));
  g_signal_connect (G_OBJECT (ragent), "candidate-gathering-done",
      G_CALLBACK (cb_candidate_gathering_done), GUINT_TO_POINTER (2));
  g_signal_connect (G_OBJECT (lagent), "component-state-changed",
      G_CALLBACK (cb_component_state_changed), GUINT_TO_POINTER (1));
  g_signal_connect (G_OBJECT (ragent), "component-state-changed",
      G_CALLBACK (cb_component_state_changed), GUINT_TO_POINTER (2));
  g_signal_connect (G_OBJECT (lagent), "new-selected-pair",
      G_CALLBACK (cb_new_selected_pair), GUINT_TO_POINTER(1));
  g_signal_connect (G_OBJECT (ragent), "new-selected-pair",
      G_CALLBACK (cb_new_selected_pair), GUINT_TO_POINTER (2));
  g_signal_connect (G_OBJECT (lagent), "new-candidate",
      G_CALLBACK (cb_new_candidate), GUINT_TO_POINTER (1));
  g_signal_connect (G_OBJECT (ragent), "new-candidate",
      G_CALLBACK (cb_new_candidate), GUINT_TO_POINTER (2));
  g_signal_connect (G_OBJECT (lagent), "initial-binding-request-received",
      G_CALLBACK (cb_initial_binding_request_received),
      GUINT_TO_POINTER (1));
  g_signal_connect (G_OBJECT (ragent), "initial-binding-request-received",
      G_CALLBACK (cb_initial_binding_request_received),
      GUINT_TO_POINTER (2));
  
  /* step: run test */
  g_debug ("test-dribble: running test");

  /* step: initialize variables modified by the callbacks */
  global_components_ready = 0;
  global_components_ready_exit = 2;
  global_components_failed = 0;
  global_components_failed_exit = 0;
  global_lagent_gathering_done = FALSE;
  global_ragent_gathering_done = FALSE;
  global_lagent_ibr_received =
    global_ragent_ibr_received = FALSE;
  global_lagent_cands =
    global_ragent_cands = 0;

  g_object_set (G_OBJECT (lagent), "controlling-mode", TRUE, NULL);
  g_object_set (G_OBJECT (ragent), "controlling-mode", FALSE, NULL);
  
  /* step: add one stream, with RTP+RTCP components, to each agent */
  ls_id = xice_agent_add_stream (lagent, 1);

  rs_id = xice_agent_add_stream (ragent, 1);
  g_assert (ls_id > 0);
  g_assert (rs_id > 0);
  

  xice_agent_gather_candidates (lagent, ls_id);
  xice_agent_gather_candidates (ragent, rs_id);

  /* step: attach to mainloop (needed to register the fds) */
  xice_agent_attach_recv (lagent, ls_id, XICE_COMPONENT_TYPE_RTP,
      cb_xice_recv,
      GUINT_TO_POINTER (1));
  xice_agent_attach_recv (ragent, rs_id, XICE_COMPONENT_TYPE_RTP,
      cb_xice_recv,
      GUINT_TO_POINTER (2));

  /* step: run mainloop until local candidates are ready
   *       (see timer_cb() above) */
  g_debug("test-dribble: Added streams, running mainloop until 'candidate-gathering-done'...");
  uv_run(global_mainloop, UV_RUN_DEFAULT);
  g_assert(global_lagent_gathering_done == TRUE);
  g_assert(global_ragent_gathering_done == TRUE);

  {
      gchar *ufrag = NULL, *password = NULL;
      xice_agent_get_local_credentials(lagent, ls_id, &ufrag, &password);
      xice_agent_set_remote_credentials (ragent,
          rs_id, ufrag, password);
      g_free (ufrag);
      g_free (password);
      xice_agent_get_local_credentials(ragent, rs_id, &ufrag, &password);
      xice_agent_set_remote_credentials (lagent,
          ls_id, ufrag, password);
      g_free (ufrag);
      g_free (password);
  }
  cands = xice_agent_get_local_candidates (ragent, rs_id, XICE_COMPONENT_TYPE_RTP);
  xice_agent_set_remote_candidates (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, cands);
  for (i = cands; i; i = i->next)
    xice_candidate_free ((XiceCandidate *) i->data);
  g_slist_free (cands);
  cands = xice_agent_get_local_candidates (lagent, ls_id, XICE_COMPONENT_TYPE_RTP);
  xice_agent_set_remote_candidates (ragent, rs_id, XICE_COMPONENT_TYPE_RTP, cands);
  for (i = cands; i; i = i->next)
    xice_candidate_free ((XiceCandidate *) i->data);
  g_slist_free (cands);

  g_debug ("test-dribble: Set properties, next running mainloop until connectivity checks succeed...");

  /* step: run the mainloop until connectivity checks succeed
   *       (see timer_cb() above) */
  uv_run(global_mainloop, UV_RUN_DEFAULT);

  /* note: verify that STUN binding requests were sent */
  g_assert (global_lagent_ibr_received == TRUE);
  g_assert (global_ragent_ibr_received == TRUE);

  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);
  g_assert (global_ragent_state == XICE_COMPONENT_STATE_READY);
  /* note: verify that correct number of local candidates were reported */
  g_assert (global_lagent_cands == 1);
  g_assert (global_ragent_cands == 1);

  g_debug ("test-dribble: agents are ready.. now adding new buggy candidate");

  uv_timer_t quit_timer;
  uv_timer_init(global_mainloop, &quit_timer);
  uv_timer_start(&quit_timer, quit_loop_cb, 500, 0);
  uv_run(global_mainloop, UV_RUN_DEFAULT);

  global_components_ready--;

  cands = xice_agent_get_local_candidates (ragent, rs_id, XICE_COMPONENT_TYPE_RTP);
  xice_address_set_port(&((XiceCandidate *) cands->data)->addr, 80);
  xice_agent_set_remote_candidates (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, cands);
  for (i = cands; i; i = i->next)
    xice_candidate_free ((XiceCandidate *) i->data);
  g_slist_free (cands);

  g_assert (global_lagent_state == XICE_COMPONENT_STATE_CONNECTED);
  uv_run (global_mainloop, UV_RUN_DEFAULT);
  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);

  /*
  g_debug ("test-dribble: buggy candidate worked, testing lower priority cand");

  cands = xice_agent_get_local_candidates (ragent, rs_id, XICE_COMPONENT_TYPE_RTP);
  xice_address_set_port(&((XiceCandidate *) cands->data)->addr, 80);
  ((XiceCandidate *) cands->data)->priority -= 100;
  xice_agent_set_remote_candidates (lagent, ls_id, XICE_COMPONENT_TYPE_RTP, cands);
  for (i = cands; i; i = i->next)
    xice_candidate_free ((XiceCandidate *) i->data);
  g_slist_free (cands);

  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);*/

  /* note: test payload send and receive */
  global_ragent_read = 0;
  g_assert (xice_agent_send (lagent, ls_id, 1, 16, "1234567812345678") == 16);
  uv_run(global_mainloop, UV_RUN_DEFAULT);
  g_assert (global_ragent_read == 16);

  g_debug ("test-dribble: Ran mainloop, removing streams...");

  /* step: clean up resources and exit */

  xice_agent_remove_stream (lagent, ls_id);
  xice_agent_remove_stream (ragent, rs_id);
  priv_print_global_status ();
  g_assert (global_lagent_state == XICE_COMPONENT_STATE_READY);
  g_assert (global_ragent_state >= XICE_COMPONENT_STATE_CONNECTED);
  /* note: verify that correct number of local candidates were reported */
  g_assert (global_lagent_cands == 1);
  g_assert (global_ragent_cands == 1);


  g_object_unref (lagent);
  g_object_unref (ragent);

  //g_main_loop_unref (global_mainloop);
  global_mainloop = NULL;

  //g_source_remove (timer_id);
  uv_unref((uv_handle_t*)&timer_id);
  xice_context_destroy(context);

#ifdef G_OS_WIN32
  WSACleanup();
#endif
  return 0;
}