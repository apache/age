/*
 * lws_protocol.c 
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * IDENTIFICATION
 *	  src/lib/libwebsocket/lws_protocol.c
 */

#include "libwebsocket/lws.h"
#include "interface/pgwired_facade.h"
#include "communication/agbolt_ws_hook.h"
#include "libwebsocket/callback/lws_callback.h"

#include "utils/config_helper.h"


static struct pg_conn *conn;
int in_transaction = 0;
extern int interrupted;

extern struct garph_protocol_conf garph_protocol_conf;


int
callback_minimal_server_echo(struct lws *wsi, enum lws_callback_reasons reason,
                             void *user, void *in, size_t len) {

    /* This will be different for every connected peer */
    struct per_session_data *pss =
            (struct per_session_data *) user;

    struct per_vhost_storage *vhd = (struct per_vhost_storage *)
            lws_protocol_vh_priv_get(lws_get_vhost(wsi),
                                     lws_get_protocol(wsi));

    const struct msg *pmsg;
    struct msg amsg;
    int writable_m;
    int receive_n;
    int flags;

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            switch (lws_callback_init(wsi, vhd, in)) {
                case CALLBACK_INIT_VHD_IS_FALSE:
                    return -1;
                case CALLBACK_SUCCESS:
                    break;
            }

            conn = get_postgres_connection(garph_protocol_conf.conn_string);
            break;
        case LWS_CALLBACK_ESTABLISHED:
            switch (lws_callback_established(pss)) {
                case CALLBACK_ESTABLISHED_RING_IS_FALSE:
                    return -1;
                case CALLBACK_SUCCESS:
                    break;
            }

            break;
        case LWS_CALLBACK_SERVER_WRITEABLE:
            lwsl_user("LWS_CALLBACK_SERVER_WRITEABLE\n");
            if (pss->write_consume_pending) {
                /* perform the deferred fifo consume */
                lws_ring_consume_single_tail(pss->ring, &pss->tail, 1);
                pss->write_consume_pending = 0;
            }
            pmsg = lws_ring_get_element(pss->ring, &pss->tail);
            if (!pmsg) {
                lwsl_user(" (nothing in ring)\n");
                break;
            }

            struct msg *resp;
            resp = agbolt_ws_hook(conn, pmsg);
            assert(resp);

            flags = lws_write_ws_flags(resp->binary,
                                       resp->first, resp->final);

            /* notice we allowed for LWS_PRE in the payload already */
            writable_m = lws_write(
                    wsi, ((uint8 *) resp->payload)
//                               + LWS_PRE
                    , resp->len, (enum lws_write_protocol) flags);

            if (writable_m < (int) resp->len) {
                lwsl_err("ERROR %d writing to ws socket\n", writable_m);
                return -1;
            }

            if (resp->len) { ;
                puts((const char *) resp->payload);
                lwsl_hexdump_notice(resp->payload, resp->len);
            }

            lwsl_user(" wrote %d: flags: 0x%x first: %d final %d\n",
                      writable_m, flags, resp->first, resp->final);

            /*
             * Workaround deferred deflate in pmd extension by only
             * consuming the fifo entry when we are certain it has been
             * fully deflated at the next WRITABLE callback.  You only need
             * this if you're using pmd.
             */

            pss->write_consume_pending = 1;
            lws_callback_on_writable(wsi);

            if (pss->flow_controlled &&
                (int) lws_ring_get_count_free_elements(pss->ring) > RING_DEPTH - 5) {
                lws_rx_flow_control(wsi, 1);
                pss->flow_controlled = 0;
            }

            if ((*vhd->options & 1) && resp && resp->final)
                pss->completed = 1;
            break;

        case LWS_CALLBACK_RECEIVE:


            lwsl_user("LWS_CALLBACK_RECEIVE: %4d (rpp %5d, first %d, "
                      "last %d, bin %d, msglen %d (+ %d = %d))\n",
                      (int) len, (int) lws_remaining_packet_payload(wsi),
                      lws_is_first_fragment(wsi),
                      lws_is_final_fragment(wsi),
                      lws_frame_is_binary(wsi), pss->msglen, (int) len,
                      (int) pss->msglen + (int) len);
            if (len) { ;
                puts((const char *) in);
                lwsl_hexdump_notice(in, len);
            }
            (amsg).first = (char) lws_is_first_fragment(wsi);
            (amsg).final = (char) lws_is_final_fragment(wsi);
            (amsg).binary = (char) lws_frame_is_binary(wsi);

            receive_n = (int) lws_ring_get_count_free_elements(pss->ring);

            if (!receive_n) {
                lwsl_user("dropping!\n");
                return CALLBACK_BREAK;
            }
            if ((amsg).final)
                pss->msglen = 0;
            else
                pss->msglen += (uint32_t) len;

            (amsg).len = len;
            (amsg).payload = malloc(len); // LWS_PRE eleminated

            if (!(amsg).payload) {
                lwsl_user("OOM: dropping\n");
                return CALLBACK_BREAK;

            }

            switch (len) {
                case 75: // begin
                case 63: // begin
                case 65: // begin
                    lwsl_user("begin detected, return 0.\n");
                    in_transaction = 1;
                    return 0; // don't write
                default:
                    break;
            }

            memcpy((char *) (amsg).payload, in, len); // LWS_PRE eleminated
            if (!lws_ring_insert(pss->ring, &amsg, 1)) {
                minimal_destroy_message(&amsg);
                lwsl_user("dropping!\n");
                return CALLBACK_BREAK;

            }

            lws_callback_on_writable(wsi);
            if (receive_n < 3 && !pss->flow_controlled) {
                pss->flow_controlled = 1;
                lws_rx_flow_control(wsi, 0);
            }

            break;
        case LWS_CALLBACK_CLOSED:

            lwsl_user("LWS_CALLBACK_CLOSED\n");
            lws_ring_destroy(pss->ring);

            if (*vhd->options & 1) {
                if (!*vhd->interrupted)
                    *vhd->interrupted = 1 + pss->completed;
                lws_cancel_service(lws_get_context(wsi));
            }
            break;
        default:
            break;
    }

    return 0;
}


