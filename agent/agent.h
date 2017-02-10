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

#ifndef _AGENT_H
#define _AGENT_H

/**
 * SECTION:agent
 * @short_description:  ICE agent API implementation
 * @see_also: #XiceCandidate
 * @see_also: #XiceAddress
 * @include: agent.h
 * @stability: Stable
 *
 * The #XiceAgent is your main object when using libxice.
 * It is the agent that will take care of everything relating to ICE.
 * It will take care of discovering your local candidates and do
 *  connectivity checks to create a stream of data between you and your peer.
 *
 <example>
   <title>Simple example on how to use libxice</title>
   <programlisting>
   guint stream_id;
   gchar buffer[] = "hello world!";
   GSList *lcands = NULL;

   // Create a xice agent
   XiceAgent *agent = xice_agent_new (NULL, XICE_COMPATIBILITY_RFC5245);

   // Connect the signals
   g_signal_connect (G_OBJECT (agent), "candidate-gathering-done",
                     G_CALLBACK (cb_candidate_gathering_done), NULL);
   g_signal_connect (G_OBJECT (agent), "component-state-changed",
                     G_CALLBACK (cb_component_state_changed), NULL);
   g_signal_connect (G_OBJECT (agent), "new-selected-pair",
                     G_CALLBACK (cb_new_selected_pair), NULL);

   // Create a new stream with one component and start gathering candidates
   stream_id = xice_agent_add_stream (agent, 1);
   xice_agent_gather_candidates (agent, stream_id);

   // Attach to the component to receive the data
   xice_agent_attach_recv (agent, stream_id, 1, NULL,
                          cb_xice_recv, NULL);

   // ... Wait until the signal candidate-gathering-done is fired ...
   lcands = xice_agent_get_local_candidates(agent, stream_id, 1);

   // ... Send local candidates to the peer and set the peer's remote candidates
   xice_agent_set_remote_candidates (agent, stream_id, 1, rcands);

   // ... Wait until the signal new-selected-pair is fired ...
   // Send our message!
   xice_agent_send (agent, stream_id, 1, sizeof(buffer), buffer);

   // Anything received will be received through the cb_xice_recv callback

   // Destroy the object
   g_object_unref(agent);

   </programlisting>
 </example>
 *
 * Refer to the examples in the examples/ subdirectory of the libxice source for
 * complete examples.
 *
 */


#include <glib-object.h>
/**
 * XiceAgent:
 *
 * The #XiceAgent is the main GObject of the libxice library and represents
 * the ICE agent.
 */
typedef struct _XiceAgent XiceAgent;

#include "address.h"
#include "candidate.h"
#include "debug.h"
#include "contexts/xicecontext.h"

G_BEGIN_DECLS

#define XICE_TYPE_AGENT xice_agent_get_type()

#define XICE_AGENT(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  XICE_TYPE_AGENT, XiceAgent))

#define XICE_AGENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  XICE_TYPE_AGENT, XiceAgentClass))

#define XICE_IS_AGENT(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  XICE_TYPE_AGENT))

#define XICE_IS_AGENT_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  XICE_TYPE_AGENT))

#define XICE_AGENT_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  XICE_TYPE_AGENT, XiceAgentClass))

typedef struct _XiceAgentClass XiceAgentClass;

struct _XiceAgentClass
{
  GObjectClass parent_class;
};


GType xice_agent_get_type (void);


/**
 * XICE_AGENT_MAX_REMOTE_CANDIDATES:
 *
 * A hard limit for the number of remote candidates. This
 * limit is enforced to protect against malevolent remote
 * clients.
 */
#define XICE_AGENT_MAX_REMOTE_CANDIDATES    25

/**
 * XiceComponentState:
 * @XICE_COMPONENT_STATE_DISCONNECTED: No activity scheduled
 * @XICE_COMPONENT_STATE_GATHERING: Gathering local candidates
 * @XICE_COMPONENT_STATE_CONNECTING: Establishing connectivity
 * @XICE_COMPONENT_STATE_CONNECTED: At least one working candidate pair
 * @XICE_COMPONENT_STATE_READY: ICE concluded, candidate pair selection
 * is now final
 * @XICE_COMPONENT_STATE_FAILED: Connectivity checks have been completed,
 * but connectivity was not established
 * @XICE_COMPONENT_STATE_LAST: Dummy state
 *
 * An enum representing the state of a component.
 * <para> See also: #XiceAgent::component-state-changed </para>
 */
typedef enum
{
  XICE_COMPONENT_STATE_DISCONNECTED,
  XICE_COMPONENT_STATE_GATHERING,
  XICE_COMPONENT_STATE_CONNECTING,
  XICE_COMPONENT_STATE_CONNECTED,
  XICE_COMPONENT_STATE_READY,
  XICE_COMPONENT_STATE_FAILED,
  XICE_COMPONENT_STATE_LAST
} XiceComponentState;


/**
 * XiceComponentType:
 * @XICE_COMPONENT_TYPE_RTP: RTP Component type
 * @XICE_COMPONENT_TYPE_RTCP: RTCP Component type
 *
 * Convenience enum representing the type of a component for use as the
 * component_id for RTP/RTCP usages.
 <example>
   <title>Example of use.</title>
   <programlisting>
   xice_agent_send (agent, stream_id, XICE_COMPONENT_TYPE_RTP, len, buf);
   </programlisting>
  </example>
 */
typedef enum
{
  XICE_COMPONENT_TYPE_RTP = 1,
  XICE_COMPONENT_TYPE_RTCP = 2
} XiceComponentType;


/**
 * XiceCompatibility:
 * @XICE_COMPATIBILITY_RFC5245: Use compatibility with the RFC5245 ICE specs
 * @XICE_COMPATIBILITY_GOOGLE: Use compatibility for Google Talk specs
 * @XICE_COMPATIBILITY_MSN: Use compatibility for MSN Messenger specs
 * @XICE_COMPATIBILITY_WLM2009: Use compatibility with Windows Live Messenger
 * 2009
 * @XICE_COMPATIBILITY_OC2007: Use compatibility with Microsoft Office Communicator 2007
 * @XICE_COMPATIBILITY_OC2007R2: Use compatibility with Microsoft Office Communicator 2007 R2
 * @XICE_COMPATIBILITY_DRAFT19: Use compatibility for ICE Draft 19 specs
 * @XICE_COMPATIBILITY_LAST: Dummy last compatibility mode
 *
 * An enum to specify which compatible specifications the #XiceAgent should use.
 * Use with xice_agent_new()
 *
 * <warning>@XICE_COMPATIBILITY_DRAFT19 is deprecated and should not be used
 * in newly-written code. It is kept for compatibility reasons and
 * represents the same compatibility as @XICE_COMPATIBILITY_RFC5245 </warning>
 */
typedef enum
{
  XICE_COMPATIBILITY_RFC5245 = 0,
  XICE_COMPATIBILITY_GOOGLE,
  XICE_COMPATIBILITY_MSN,
  XICE_COMPATIBILITY_WLM2009,
  XICE_COMPATIBILITY_OC2007,
  XICE_COMPATIBILITY_OC2007R2,
  XICE_COMPATIBILITY_DRAFT19 = XICE_COMPATIBILITY_RFC5245,
  XICE_COMPATIBILITY_LAST = XICE_COMPATIBILITY_OC2007R2,
} XiceCompatibility;

/**
 * XiceProxyType:
 * @XICE_PROXY_TYPE_NONE: Do not use a proxy
 * @XICE_PROXY_TYPE_SOCKS5: Use a SOCKS5 proxy
 * @XICE_PROXY_TYPE_HTTP: Use an HTTP proxy
 * @XICE_PROXY_TYPE_LAST: Dummy last proxy type
 *
 * An enum to specify which proxy type to use for relaying.
 * Note that the proxies will only be used with TCP TURN relaying.
 * <para> See also: #XiceAgent:proxy-type </para>
 *
 * Since: 0.0.4
 */
typedef enum
{
  XICE_PROXY_TYPE_NONE = 0,
  XICE_PROXY_TYPE_SOCKS5,
  XICE_PROXY_TYPE_HTTP,
  XICE_PROXY_TYPE_LAST = XICE_PROXY_TYPE_HTTP,
} XiceProxyType;


/**
 * XiceAgentRecvFunc:
 * @agent: The #XiceAgent Object
 * @stream_id: The id of the stream
 * @component_id: The id of the component of the stream
 *        which received the data
 * @len: The length of the data
 * @buf: The buffer containing the data received
 * @user_data: The user data set in xice_agent_attach_recv()
 *
 * Callback function when data is received on a component
 *
 */
typedef void (*XiceAgentRecvFunc) (
  XiceAgent *agent, guint stream_id, guint component_id, guint len,
  gchar *buf, gpointer user_data);


/**
 * xice_agent_new:
 * @ctx: The Glib Mainloop Context to use for timers
 * @compat: The compatibility mode of the agent
 *
 * Create a new #XiceAgent.
 * The returned object must be freed with g_object_unref()
 *
 * Returns: The new agent GObject
 */
XiceAgent *
xice_agent_new (XiceContext *ctx, XiceCompatibility compat);


/**
 * xice_agent_new_reliable:
 * @ctx: The Glib Mainloop Context to use for timers
 * @compat: The compatibility mode of the agent
 *
 * Create a new #XiceAgent in reliable mode, which uses #PseudoTcpSocket to
 * assure reliability of the messages.
 * The returned object must be freed with g_object_unref()
 * <para> See also: #XiceAgent::reliable-transport-writable </para>
 *
 * Since: 0.0.11
 *
 * Returns: The new agent GObject
 */
XiceAgent *
xice_agent_new_reliable (XiceContext *ctx, XiceCompatibility compat);

/**
 * xice_agent_add_local_address:
 * @agent: The #XiceAgent Object
 * @addr: The address to listen to
 * If the port is 0, then a random port will be chosen by the system
 *
 * Add a local address from which to derive local host candidates for
 * candidate gathering.
 * <para>
 * Since 0.0.5, if this method is not called, libxice will automatically
 * discover the local addresses available
 * </para>
 *
 * See also: xice_agent_gather_candidates()
 * Returns: %TRUE on success, %FALSE on fatal (memory allocation) errors
 */
gboolean
xice_agent_add_local_address (XiceAgent *agent, XiceAddress *addr);


/**
 * xice_agent_add_stream:
 * @agent: The #XiceAgent Object
 * @n_components: The number of components to add to the stream
 *
 * Adds a data stream to @agent containing @n_components components.
 *
 * Returns: The ID of the new stream, 0 on failure
 **/
guint
xice_agent_add_stream (
  XiceAgent *agent,
  guint n_components);

/**
 * xice_agent_remove_stream:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream to remove
 *
 * Remove and free a previously created data stream from @agent
 *
 **/
void
xice_agent_remove_stream (
  XiceAgent *agent,
  guint stream_id);


/**
 * xice_agent_set_port_range:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 * @min_port: The minimum port to use
 * @max_port: The maximum port to use
 *
 * Sets a preferred port range for allocating host candidates.
 * <para>
 * If a local host candidate cannot be created on that port
 * range, then the xice_agent_gather_candidates() call will fail.
 * </para>
 * <para>
 * This MUST be called before xice_agent_gather_candidates()
 * </para>
 *
 */
void
xice_agent_set_port_range (
    XiceAgent *agent,
    guint stream_id,
    guint component_id,
    guint min_port,
    guint max_port);

/**
 * xice_agent_set_relay_info:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 * @server_ip: The IP address of the TURN server
 * @server_port: The port of the TURN server
 * @username: The TURN username to use for the allocate
 * @password: The TURN password to use for the allocate
 * @type: The type of relay to use
 *
 * Sets the settings for using a relay server during the candidate discovery.
 *
 * Returns: %TRUE if the TURN settings were accepted.
 * %FALSE if the address was invalid.
 */
gboolean xice_agent_set_relay_info(
    XiceAgent *agent,
    guint stream_id,
    guint component_id,
    const gchar *server_ip,
    guint server_port,
    const gchar *username,
    const gchar *password,
    XiceRelayType type);

/**
 * xice_agent_gather_candidates:
 * @agent: The #XiceAgent Object
 * @stream_id: The id of the stream to start
 *
 * Start the candidate gathering process.
 * Once done, #XiceAgent::candidate-gathering-done is called for the stream
 *
 * <para>See also: xice_agent_add_local_address()</para>
 * <para>See also: xice_agent_set_port_range()</para>
 *
 * Returns: %FALSE if the stream id is invalid or if a host candidate couldn't be allocated
 * on the requested interfaces/ports.
 *
 <note>
   <para>
    Local addresses can be previously set with xice_agent_add_local_address()
  </para>
  <para>
    Since 0.0.5, If no local address was previously added, then the xice agent
    will automatically detect the local address using
    xice_interfaces_get_local_ips()
   </para>
 </note>
 */
gboolean
xice_agent_gather_candidates (
  XiceAgent *agent,
  guint stream_id);

/**
 * xice_agent_set_remote_credentials:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @ufrag: NULL-terminated string containing an ICE username fragment
 *    (length must be between 22 and 256 chars)
 * @pwd: NULL-terminated string containing an ICE password
 *    (length must be between 4 and 256 chars)
 *
 * Sets the remote credentials for stream @stream_id.
 *
 <note>
   <para>
     Stream credentials do not override per-candidate credentials if set
   </para>
 </note>
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
xice_agent_set_remote_credentials (
  XiceAgent *agent,
  guint stream_id,
  const gchar *ufrag, const gchar *pwd);



/**
 * xice_agent_get_local_credentials:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @ufrag: a pointer to a NULL-terminated string containing
 * an ICE username fragment [OUT].
 * This string must be freed with g_free()
 * @pwd: a pointer to a NULL-terminated string containing an ICE
 * password [OUT]
 * This string must be freed with g_free()
 *
 * Gets the local credentials for stream @stream_id.
 *
 * Returns: %TRUE on success, %FALSE on error.
 */
gboolean
xice_agent_get_local_credentials (
  XiceAgent *agent,
  guint stream_id,
  gchar **ufrag, gchar **pwd);

/**
 * xice_agent_set_remote_candidates:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream the candidates are for
 * @component_id: The ID of the component the candidates are for
 * @candidates: a #GSList of #XiceCandidate items describing each candidate to add
 *
 * Sets, adds or updates the remote candidates for a component of a stream.
 *
 <note>
   <para>
    XICE_AGENT_MAX_REMOTE_CANDIDATES is the absolute maximum limit
    for remote candidates.
   </para>
   <para>
   You must first call xice_agent_gather_candidates() and wait for the
   #XiceAgent::candidate-gathering-done signale before
   calling xice_agent_set_remote_candidates()
   </para>
   <para>
    Since 0.1.3, there is no need to wait for the candidate-gathering-done signal.
    Remote candidates can be set even while gathering local candidates.
    Newly discovered local candidates will automatically be paired with
    existing remote candidates.
   </para>
 </note>
 *
 * Returns: The number of candidates added, negative on errors (memory
 * allocation error or invalid component)
 **/
int
xice_agent_set_remote_candidates (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  const GSList *candidates);


/**
 * xice_agent_send:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream to send to
 * @component_id: The ID of the component to send to
 * @len: The length of the buffer to send
 * @buf: The buffer of data to send
 *
 * Sends a data payload over a stream's component.
 *
 <note>
   <para>
     Component state MUST be XICE_COMPONENT_STATE_READY, or as a special case,
     in any state if component was in READY state before and was then restarted
   </para>
   <para>
   In reliable mode, the -1 error value means either that you are not yet
   connected or that the send buffer is full (equivalent to EWOULDBLOCK).
   In both cases, you simply need to wait for the
   #XiceAgent::reliable-transport-writable signal to be fired before resending
   the data.
   </para>
   <para>
   In non-reliable mode, it will virtually never happen with UDP sockets, but
   it might happen if the active candidate is a TURN-TCP connection that got
   disconnected.
   </para>
   <para>
   In both reliable and non-reliable mode, a -1 error code could also mean that
   the stream_id and/or component_id are invalid.
   </para>
</note>
 *
 * Returns: The number of bytes sent, or negative error code
 */
gint
xice_agent_send (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  guint len,
  const gchar *buf);

/**
 * xice_agent_get_local_candidates:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 *
 * Retreive from the agent the list of all local candidates
 * for a stream's component
 *
 <note>
   <para>
     The caller owns the returned GSList as well as the candidates contained
     within it.
     To get full results, the client should wait for the
     #XiceAgent::candidate-gathering-done signal.
   </para>
 </note>
 *
 * Returns: a #GSList of #XiceCandidate objects representing
 * the local candidates of @agent
 **/
GSList *
xice_agent_get_local_candidates (
  XiceAgent *agent,
  guint stream_id,
  guint component_id);


/**
 * xice_agent_get_remote_candidates:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 *
 * Get a list of the remote candidates set on a stream's component
 *
 <note>
   <para>
     The caller owns the returned GSList but not the candidates
     contained within it.
   </para>
   <para>
     The list of remote candidates can change during processing.
     The client should register for the #XiceAgent::new-remote-candidate signal
     to get notified of new remote candidates.
   </para>
 </note>
 *
 * Returns: a #GSList of #XiceCandidates objects representing
 * the remote candidates set on the @agent
 **/
GSList *
xice_agent_get_remote_candidates (
  XiceAgent *agent,
  guint stream_id,
  guint component_id);

/**
 * xice_agent_restart:
 * @agent: The #XiceAgent Object
 *
 * Restarts the session as defined in ICE draft 19. This function
 * needs to be called both when initiating (ICE spec section 9.1.1.1.
 * "ICE Restarts"), as well as when reacting (spec section 9.2.1.1.
 * "Detecting ICE Restart") to a restart.
 *
 * Returns: %TRUE on success %FALSE on error
 **/
gboolean
xice_agent_restart (
  XiceAgent *agent);


/**
 * xice_agent_attach_recv:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of stream
 * @component_id: The ID of the component
 * @ctx: The Glib Mainloop Context to use for listening on the component
 * @func: The callback function to be called when data is received on
 * the stream's component
 * @data: user data associated with the callback
 *
 * Attaches the stream's component's sockets to the Glib Mainloop Context in
 * order to be notified whenever data becomes available for a component.
 *
 * Returns: %TRUE on success, %FALSE if the stream or component IDs are invalid.
 */
gboolean
xice_agent_attach_recv (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceAgentRecvFunc func,
  gpointer data);


/**
 * xice_agent_set_selected_pair:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 * @lfoundation: The local foundation of the candidate to use
 * @rfoundation: The remote foundation of the candidate to use
 *
 * Sets the selected candidate pair for media transmission
 * for a given stream's component. Calling this function will
 * disable all further ICE processing (connection check,
 * state machine updates, etc). Note that keepalives will
 * continue to be sent.
 *
 * Returns: %TRUE on success, %FALSE if the candidate pair cannot be found
 */
gboolean
xice_agent_set_selected_pair (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  const gchar *lfoundation,
  const gchar *rfoundation);

/**
 * xice_agent_get_selected_pair:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 * @local: The local selected candidate
 * @remote: The remote selected candidate
 *
 * Retreive the selected candidate pair for media transmission
 * for a given stream's component.
 *
 * Returns: %TRUE on success, %FALSE if there is no selected candidate pair
 */
gboolean
xice_agent_get_selected_pair (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceCandidate **local,
  XiceCandidate **remote);

/**
 * xice_agent_set_selected_remote_candidate:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 * @candidate: The #XiceCandidate to select
 *
 * Sets the selected remote candidate for media transmission
 * for a given stream's component. This is used to force the selection of
 * a specific remote candidate even when connectivity checks are failing
 * (e.g. non-ICE compatible candidates).
 * Calling this function will disable all further ICE processing
 * (connection check, state machine updates, etc). Note that keepalives will
 * continue to be sent.
 *
 * Returns: %TRUE on success, %FALSE on failure
 */
gboolean
xice_agent_set_selected_remote_candidate (
  XiceAgent *agent,
  guint stream_id,
  guint component_id,
  XiceCandidate *candidate);


/**
 * xice_agent_set_stream_tos:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @tos: The ToS to set
 *
 * Sets the IP_TOS and/or IPV6_TCLASS field on the stream's sockets' options
 *
 * Since: 0.0.9
 */
void xice_agent_set_stream_tos (
  XiceAgent *agent,
  guint stream_id,
  gint tos);



/**
 * xice_agent_set_software:
 * @agent: The #XiceAgent Object
 * @software: The value of the SOFTWARE attribute to add.
 *
 * This function will set the value of the SOFTWARE attribute to be added to
 * STUN requests, responses and error responses sent during connectivity checks.
 * <para>
 * The SOFTWARE attribute will only be added in the #XICE_COMPATIBILITY_RFC5245
 * and #XICE_COMPATIBILITY_WLM2009 compatibility modes.
 *
 * </para>
 * <note>
     <para>
       The @software argument will be appended with the libxice version before
       being sent.
     </para>
     <para>
       The @software argument must be in UTF-8 encoding and only the first
       128 characters will be sent.
     </para>
   </note>
 *
 * Since: 0.0.10
 *
 */
void xice_agent_set_software (
    XiceAgent *agent,
    const gchar *software);

/**
 * xice_agent_set_stream_name:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream to change
 * @name: The new name of the stream or %NULL
 *
 * This function will assign a unique name to a stream.
 * This is only useful when parsing and generating an SDP of the candidates.
 *
 * <para>See also: xice_agent_generate_local_sdp()</para>
 * <para>See also: xice_agent_parse_remote_sdp()</para>
 * <para>See also: xice_agent_get_stream_name()</para>
 *
 * Returns: %TRUE if the name has been set. %FALSE in case of error
 * (invalid stream or duplicate name).
 * Since: 0.1.4
 */
gboolean xice_agent_set_stream_name (
    XiceAgent *agent,
    guint stream_id,
    const gchar *name);

/**
 * xice_agent_get_stream_name:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream to change
 *
 * This function will return the name assigned to a stream.

 * <para>See also: xice_agent_set_stream_name()</para>
 *
 * Returns: The name of the stream. The name is only valid while the stream
 * exists or until it changes through a call to xice_agent_set_stream_name().
 *
 *
 * Since: 0.1.4
 */
const gchar *xice_agent_get_stream_name (
    XiceAgent *agent,
    guint stream_id);

/**
 * xice_agent_get_default_local_candidate:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @component_id: The ID of the component
 *
 * This helper function will return the recommended default candidate to be
 * used for non-ICE compatible clients. This will usually be the candidate
 * with the lowest priority, since it will be the longest path but the one with
 * the most chances of success.
 * <note>
     <para>
     This function is only useful in order to manually generate the
     local SDP
     </para>
 * </note>
 *
 * Returns: The candidate to be used as the default candidate, or %NULL in case
 * of error. Must be freed with xice_candidate_free() once done.
 *
 */
XiceCandidate *
xice_agent_get_default_local_candidate (
    XiceAgent *agent,
    guint stream_id,
    guint component_id);

/**
 * xice_agent_generate_local_sdp:
 * @agent: The #XiceAgent Object
 *
 * Generate an SDP string containing the local candidates and credentials for
 * all streams and components in the agent.
 *
 <note>
   <para>
     The SDP will not contain any codec lines and the 'm' line will not list
     any payload types.
   </para>
   <para>
    It is highly recommended to set names on the streams prior to calling this
    function. Unnamed streams will show up as '-' in the 'm' line, but the SDP
    will not be parseable with xice_agent_parse_remote_sdp() if a stream is
    unnamed.
   </para>
   <para>
     The default candidate in the SDP will be selected based on the lowest
     priority candidate for the first component.
   </para>
 </note>
 *
 * <para>See also: xice_agent_set_stream_name() </para>
 * <para>See also: xice_agent_parse_remote_sdp() </para>
 * <para>See also: xice_agent_generate_local_stream_sdp() </para>
 * <para>See also: xice_agent_generate_local_candidate_sdp() </para>
 * <para>See also: xice_agent_get_default_local_candidate() </para>
 *
 * Returns: A string representing the local SDP. Must be freed with g_free()
 * once done.
 *
 * Since: 0.1.4
 **/
gchar *
xice_agent_generate_local_sdp (
  XiceAgent *agent);

/**
 * xice_agent_generate_local_stream_sdp:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream
 * @include_non_ice: Whether or not to include non ICE specific lines
 * (m=, c= and a=rtcp: lines)
 *
 * Generate an SDP string containing the local candidates and credentials
 * for a stream.
 *
 <note>
   <para>
     The SDP will not contain any codec lines and the 'm' line will not list
     any payload types.
   </para>
   <para>
    It is highly recommended to set the name of the stream prior to calling this
    function. Unnamed streams will show up as '-' in the 'm' line.
   </para>
   <para>
     The default candidate in the SDP will be selected based on the lowest
     priority candidate.
   </para>
 </note>
 *
 * <para>See also: xice_agent_set_stream_name() </para>
 * <para>See also: xice_agent_parse_remote_stream_sdp() </para>
 * <para>See also: xice_agent_generate_local_sdp() </para>
 * <para>See also: xice_agent_generate_local_candidate_sdp() </para>
 * <para>See also: xice_agent_get_default_local_candidate() </para>
 *
 * Returns: A string representing the local SDP for the stream. Must be freed
 * with g_free() once done.
 *
 * Since: 0.1.4
 **/
gchar *
xice_agent_generate_local_stream_sdp (
    XiceAgent *agent,
    guint stream_id,
    gboolean include_non_ice);

/**
 * xice_agent_generate_local_candidate_sdp:
 * @agent: The #XiceAgent Object
 * @candidate: The candidate to generate
 *
 * Generate an SDP string representing a local candidate.
 *
 * <para>See also: xice_agent_parse_remote_candidate_sdp() </para>
 * <para>See also: xice_agent_generate_local_sdp() </para>
 * <para>See also: xice_agent_generate_local_stream_sdp() </para>
 *
 * Returns: A string representing the SDP for the candidate. Must be freed
 * with g_free() once done.
 *
 * Since: 0.1.4
 **/
gchar *
xice_agent_generate_local_candidate_sdp (
    XiceAgent *agent,
    XiceCandidate *candidate);

/**
 * xice_agent_parse_remote_sdp:
 * @agent: The #XiceAgent Object
 * @sdp: The remote SDP to parse
 *
 * Parse an SDP string and extracts candidates and credentials from it and sets
 * them on the agent.
 *
 <note>
   <para>
    This function will return an error if a stream has not been assigned a name
    with xice_agent_set_stream_name() as it becomes troublesome to assign the
    streams from the agent to the streams in the SDP.
   </para>
 </note>
 *
 *
 * <para>See also: xice_agent_set_stream_name() </para>
 * <para>See also: xice_agent_generate_local_sdp() </para>
 * <para>See also: xice_agent_parse_remote_stream_sdp() </para>
 * <para>See also: xice_agent_parse_remote_candidate_sdp() </para>
 *
 * Returns: The number of candidates added, negative on errors
 *
 * Since: 0.1.4
 **/
int
xice_agent_parse_remote_sdp (
    XiceAgent *agent,
    const gchar *sdp);


/**
 * xice_agent_parse_remote_stream_sdp:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream to parse
 * @sdp: The remote SDP to parse
 * @ufrag: Pointer to store the ice ufrag if non %NULL. Must be freed with
 * g_free() after use
 * @pwd: Pointer to store the ice password if non %NULL. Must be freed with
 * g_free() after use
 *
 * Parse an SDP string representing a single stream and extracts candidates
 * and credentials from it.
 *
 * <para>See also: xice_agent_generate_local_stream_sdp() </para>
 * <para>See also: xice_agent_parse_remote_sdp() </para>
 * <para>See also: xice_agent_parse_remote_candidate_sdp() </para>
 *
 * Returns: A #GSList of candidates parsed from the SDP, or %NULL in case of
 * errors
 *
 * Since: 0.1.4
 **/
GSList *
xice_agent_parse_remote_stream_sdp (
    XiceAgent *agent,
    guint stream_id,
    const gchar *sdp,
    gchar **ufrag,
    gchar **pwd);


/**
 * xice_agent_parse_remote_candidate_sdp:
 * @agent: The #XiceAgent Object
 * @stream_id: The ID of the stream the candidate belongs to
 * @sdp: The remote SDP to parse
 *
 * Parse an SDP string and extracts the candidate from it.
 *
 * <para>See also: xice_agent_generate_local_candidate_sdp() </para>
 * <para>See also: xice_agent_parse_remote_sdp() </para>
 * <para>See also: xice_agent_parse_remote_stream_sdp() </para>
 *
 * Returns: The parsed candidate or %NULL if there was an error.
 *
 * Since: 0.1.4
 **/
XiceCandidate *
xice_agent_parse_remote_candidate_sdp (
    XiceAgent *agent,
    guint stream_id,
    const gchar *sdp);

G_END_DECLS

#endif /* _AGENT_H */

