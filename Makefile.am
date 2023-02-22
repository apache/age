bin_PROGRAMS = graph_protocol

AM_CPPFLAGS = \
-I@PGSQL_LIB_DIR@ \
-I@PGSQL_INCLUDE_DIR@ \
-I@PGSQL_INCLUDE_SERVER_DIR@ \
-I/usr/local/lib \
-I/usr/local/include/libwebsockets \
-Isrc/include

graph_protocol_LDADD = \
-L@PGSQL_LIB_DIR@ \
-L@PGSQL_INCLUDE_DIR@ \
-L@PGSQL_INCLUDE_SERVER_DIR@ \
-L/usr/local/lib -lwebsockets \
-L/usr/local/include/libwebsockets \
-Lsrc/include \
-lpq -lpthread

graph_protocol_SOURCES = \
src/main.c \
\
src/utils/utils.c \
src/utils/config_helper.c \
src/utils/logging.c \
src/utils/process.c \
src/utils/option_helper.c \
\
src/spec/bolt/server_state/v440/server_state.c \
\
src/spec/bolt/handshake/v430/handshake_430.c \
src/spec/bolt/handshake/v430/common/handshake.c \
src/spec/bolt/handshake/handshake_utils.c \
\
src/communication/ag/receiver.c \
src/communication/ag/sender.c \
src/communication/bolt/receiver.c \
src/communication/bolt/sender.c \
src/communication/agbolt_ws_hook.c \
src/communication/socket_manager.c \
\
src/spec/packstream/v100/coretypes/pack_string.c \
src/spec/packstream/v100/coretypes/pack_list.c \
src/spec/packstream/v100/coretypes/pack_dict.c \
src/spec/packstream/v100/coretypes/pack_boolean.c \
src/spec/packstream/v100/coretypes/pack_integer.c \
src/spec/packstream/v100/coretypes/pack_structure.c \
src/spec/packstream/pack_utils.c \
\
src/spec/bolt/structures/bolt_node.c \
src/spec/bolt/message/v440/message_utils.c \
src/spec/bolt/bolt_utils.c \
src/stubs/stubs.c \
\
src/lib/libwebsocket/lws_extensions.c \
src/lib/libwebsocket/lws_main.c \
src/lib/libwebsocket/lws_protocol.c \
src/lib/libwebsocket/lws_utils.c \
src/lib/libwebsocket/lws_vhost.c \
src/lib/libwebsocket/lws_lejp_parser.c \
\
src/lib/libwebsocket/callback/callback_closed.c \
src/lib/libwebsocket/callback/callback_established.c \
src/lib/libwebsocket/callback/callback_init.c \
src/lib/libwebsocket/callback/callback_receive.c \
src/lib/libwebsocket/callback/callback_writable.c\
\
src/spec/bolt/message/v440/summary/failed.c \
src/spec/bolt/message/v440/summary/ignored.c \
src/spec/bolt/message/v440/detailed/record.c \
src/spec/bolt/message/v440/summary/success.c

