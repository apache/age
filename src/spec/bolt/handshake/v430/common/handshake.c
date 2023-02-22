


#include "graph_protocol_stddef.h"

#include "spec/bolt/handshake/handshake.h"
#include "spec/bolt/handshake/v430/handshake_v430.h"

const char BOLT_CONN_IDENT_BYTEARRAY[BOLT_CONN_IDENT_SIZE] = {0x60, 0x60, 0xB0, 0x17};

bool is_bolt_conn(const char recv[]) {
    for (int i = 0; i < BOLT_CONN_IDENT_SIZE; ++i) {
        if (recv[i] != BOLT_CONN_IDENT_BYTEARRAY[i]) {
            return false;
        }
    }
    return true;
}


void copy_client_info(char recv[]) {

}


bolt_server_identification
bolt_version_negotiation(char recv[]) {
    char server_available_ver[4] = {0x00, SERVER_VERSION_HOTFIX, SERVER_VERSION_MINOR, SERVER_VERSION_MAJOR};
    bolt_server_identification bolt_server_ident;
    memcpy(bolt_server_ident.avail_version, server_available_ver, 4);
    return bolt_server_ident;
}


void bolt_handshake(int *conn, char recv[]) {
    int i = 0;

    if (is_bolt_conn(recv)) {
        i += BOLT_CONN_IDENT_SIZE;
        copy_client_info(recv);
        bolt_version_negotiation(&recv[i]);
    } else {
        perror("not a bolt connection. socket distruct");
    }
}