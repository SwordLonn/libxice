/*
 * This file is part of the Xice GLib ICE library.
 *
 * Unit test for ICE full-mode related features.
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
#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#include <uv.h>

static uv_loop_t *error_loop;

gint global_lagent_cands = 0;
gint global_ragent_cands = 0;

gint global_lagent_buffers = 0;
gint global_ragent_buffers = 0;

void timer_cb (uv_timer_t *handle)
{
  g_debug ("ERROR: test has got stuck, aborting...");
  exit (-1);
}

static void
mainloop_thread (void* arg)
{
  uv_loop_t *loop = arg;

  g_usleep (100000);
  uv_run(loop, UV_RUN_DEFAULT);

  return NULL;
}


static void
cb_new_selected_pair(XiceAgent *agent,
    guint stream_id,
    guint component_id,
    gchar *lfoundation,
    gchar* rfoundation,
    gpointer data)
{
  g_debug ("test-thread:%s: %p", G_STRFUNC, data);

  if (GPOINTER_TO_UINT (data) == 1)
    g_atomic_int_inc (&global_lagent_cands);
  else if (GPOINTER_TO_UINT (data) == 2)
    g_atomic_int_inc (&global_ragent_cands);
}


static void cb_candidate_gathering_done(XiceAgent *agent, guint stream_id, gpointer data)
{
  XiceAgent *other = g_object_get_data (G_OBJECT (agent), "other-agent");
  gchar *ufrag = NULL, *password = NULL;
  GSList *cands, *i;
  guint id, other_id;
  gpointer tmp;

  g_debug ("test-thread:%s", G_STRFUNC);

  tmp = g_object_get_data (G_OBJECT (agent), "id");
  id = GPOINTER_TO_UINT (tmp);
  tmp = g_object_get_data (G_OBJECT (other), "id");
  other_id = GPOINTER_TO_UINT (tmp);

  xice_agent_get_local_credentials(agent, id, &ufrag, &password);
  xice_agent_set_remote_credentials (other,
      other_id, ufrag, password);
  g_free (ufrag);
  g_free (password);

  cands = xice_agent_get_local_candidates(agent, id, 1);
  g_assert (cands != NULL); 

  xice_agent_set_remote_candidates (other, other_id, 1, cands);

  for (i = cands; i; i = i->next)
    xice_candidate_free ((XiceCandidate *) i->data);
  g_slist_free (cands);
}



static void cb_xice_recv (XiceAgent *agent, guint stream_id, guint component_id, guint len, gchar *buf, gpointer user_data)
{
  gchar data[10];
  gint *count = NULL;

  if (GPOINTER_TO_UINT (user_data) == 1)
    count = &global_lagent_buffers;
  else if (GPOINTER_TO_UINT (user_data) == 2)
    count = &global_ragent_buffers;
  else
    g_error ("Invalid agent ?");

  if (*count == -1)
    return;

  g_assert (len == 10);

  memset (data, *count+'1', 10);

  g_assert (memcmp (buf, data, 10) == 0);

  (*count)++;

  if (*count == 10)
    *count = -1;

  if (global_ragent_buffers == -1 && global_lagent_buffers == -1) {
    uv_stop(error_loop);
    uv_loop_close(error_loop);
  }
}


static void cb_component_state_changed (XiceAgent *agent,
    guint stream_id,
    guint component_id,
    guint state,
    gpointer user_data)
{
  int i;
  gchar data[10];

  if (state != XICE_COMPONENT_STATE_READY)
    return;

  for (i=0; i<10; i++)
    {
      memset (data, i+'1', 10);

      xice_agent_send (agent, stream_id, component_id, 10, data);
    }
}

int main(void)
{
	XiceAgent *lagent, *ragent;      /* agent's L and R */
	XiceAddress baseaddr;
	const char *stun_server = NULL, *stun_server_port = NULL;

	guint ls_id, rs_id;
	
#ifdef G_OS_WIN32
	WSADATA w;
	WSAStartup(0x0202, &w);
#endif
	g_type_init();
#if !GLIB_CHECK_VERSION(2,31,8)
	g_thread_init(NULL);
#endif

	/* step: create the agents L and R */
	uv_loop_t *ldmainloop = malloc(sizeof(uv_loop_t));
	uv_loop_init(ldmainloop);
	XiceContext* lcontext = xice_context_create("libuv", (gpointer)ldmainloop);
	lagent = xice_agent_new(lcontext, XICE_COMPATIBILITY_MSN);
	uv_thread_t ldthread;

	uv_loop_t *rdmainloop = malloc(sizeof(uv_loop_t));
	uv_loop_init(rdmainloop);
	XiceContext* rcontext = xice_context_create("libuv", (gpointer)rdmainloop);
	ragent = xice_agent_new(rcontext, XICE_COMPATIBILITY_MSN);
	uv_thread_t rdthread;

	g_object_set_data(G_OBJECT(lagent), "other-agent", ragent);
	g_object_set_data(G_OBJECT(ragent), "other-agent", lagent);

	g_object_set(G_OBJECT(lagent), "controlling-mode", TRUE, NULL);
	g_object_set(G_OBJECT(ragent), "controlling-mode", FALSE, NULL);

	

	/* step: specify which local interface to use */
	if (!xice_address_set_from_string(&baseaddr, "127.0.0.1"))
		g_assert_not_reached();
	xice_agent_add_local_address(lagent, &baseaddr);
	xice_agent_add_local_address(ragent, &baseaddr);

	g_signal_connect(G_OBJECT(lagent), "candidate-gathering-done",
		G_CALLBACK(cb_candidate_gathering_done), GUINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(ragent), "candidate-gathering-done",
		G_CALLBACK(cb_candidate_gathering_done), GUINT_TO_POINTER(2));
	g_signal_connect(G_OBJECT(lagent), "component-state-changed",
		G_CALLBACK(cb_component_state_changed), GUINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(ragent), "component-state-changed",
		G_CALLBACK(cb_component_state_changed), GUINT_TO_POINTER(2));
	g_signal_connect(G_OBJECT(lagent), "new-selected-pair",
		G_CALLBACK(cb_new_selected_pair), GUINT_TO_POINTER(1));
	g_signal_connect(G_OBJECT(ragent), "new-selected-pair",
		G_CALLBACK(cb_new_selected_pair), GUINT_TO_POINTER(2));

	stun_server = getenv("XICE_STUN_SERVER");
	stun_server_port = getenv("XICE_STUN_SERVER_PORT");
	if (stun_server) {
		g_object_set(G_OBJECT(lagent), "stun-server", stun_server, NULL);
		g_object_set(G_OBJECT(lagent), "stun-server-port", atoi(stun_server_port), NULL);
		g_object_set(G_OBJECT(ragent), "stun-server", stun_server, NULL);
		g_object_set(G_OBJECT(ragent), "stun-server-port", atoi(stun_server_port), NULL);
	}

	/* step: test setter/getter functions for properties */
	{
		guint max_checks = 0;
		gchar *string = NULL;
		guint port = 0;
		gboolean mode = FALSE;
		g_object_get(G_OBJECT(lagent), "stun-server", &string, NULL);
		g_assert(stun_server == NULL || strcmp(string, stun_server) == 0);
		g_free(string);
		g_object_get(G_OBJECT(lagent), "stun-server-port", &port, NULL);
		g_assert(stun_server_port == NULL || port == (guint)atoi(stun_server_port));
		g_object_get(G_OBJECT(lagent), "controlling-mode", &mode, NULL);
		g_assert(mode == TRUE);
		g_object_set(G_OBJECT(lagent), "max-connectivity-checks", 300, NULL);
		g_object_get(G_OBJECT(lagent), "max-connectivity-checks", &max_checks, NULL);
		g_assert(max_checks == 300);
	}

	/* step: run test the first time */
	g_debug("test-thread: TEST STARTS / running test for the 1st time");

	ls_id = xice_agent_add_stream(lagent, 2);
	rs_id = xice_agent_add_stream(ragent, 2);
	g_assert(ls_id > 0);
	g_assert(rs_id > 0);

	g_object_set_data(G_OBJECT(lagent), "id", GUINT_TO_POINTER(ls_id));
	g_object_set_data(G_OBJECT(ragent), "id", GUINT_TO_POINTER(rs_id));

	xice_agent_gather_candidates(lagent, ls_id);
	xice_agent_gather_candidates(ragent, rs_id);

	xice_agent_attach_recv(lagent, ls_id, 1, cb_xice_recv,
		GUINT_TO_POINTER(1));
	xice_agent_attach_recv(ragent, rs_id, 1, cb_xice_recv,
		GUINT_TO_POINTER(2));

	uv_thread_create(&ldthread, mainloop_thread, ldmainloop);
	uv_thread_create(&rdthread, mainloop_thread, rdmainloop);

  /* step: add a timer to catch state changes triggered by signals */
  error_loop = (uv_loop_t*)malloc(sizeof(uv_loop_t));
  uv_loop_init(error_loop);
  uv_timer_t timer_req;
  uv_timer_init(error_loop, &timer_req);
  uv_timer_start(&timer_req, timer_cb, 30000, 0); // ?

  /* Run loop for error timer */
  uv_run(error_loop, UV_RUN_DEFAULT);

  uv_stop(ldmainloop);
  uv_stop(rdmainloop);
  uv_loop_close(ldmainloop);
  uv_loop_close(rdmainloop);

	uv_thread_join(&ldthread);
	uv_thread_join(&rdthread);

	/* note: verify that correct number of local candidates were reported */
	g_assert(global_lagent_cands == 1);
	g_assert(global_ragent_cands == 1);

	g_object_unref(lagent);
	g_object_unref(ragent);

#ifdef G_OS_WIN32
	WSACleanup();
#endif
	return 0;
}