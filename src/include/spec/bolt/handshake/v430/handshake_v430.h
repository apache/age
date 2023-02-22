


#ifndef SPEC_BOLT_HANDSHAKE_H
#define SPEC_BOLT_HANDSHAKE_H


#define SERVER_VERSION_MAJOR (0x04)
#define SERVER_VERSION_MINOR (0x04)
#define SERVER_VERSION_HOTFIX (0x02)

void simple_handshake(int listen_fd, int new_socket,
                      struct sockaddr_in address);


#define BOLT_CONN_IDENT_SIZE (4)
#define BOLT_NEGO_VERSION_SIZE (4)
#define BOLT_NEGO_VERSION_CANDIDATE_LIST_SIZE (4)

extern const char BOLT_CONN_IDENT_BYTEARRAY[BOLT_CONN_IDENT_SIZE];

typedef struct {
    char version_candidates[4][4];
    struct sockaddr_in *client_address;
} client_version_info;


typedef struct {
    char bolt_ident[4];
    char avail_versions[4][4];
} bolt_client_identification;

typedef struct {
    char avail_version[4];
} bolt_server_identification;

#endif


