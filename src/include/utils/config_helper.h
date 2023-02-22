

#ifndef AGBOLT_CONFIG_HELPER_H
#define AGBOLT_CONFIG_HELPER_H

#include "graph_protocol_stddef.h"

struct garph_protocol_conf {
    bool log_debug;
    uint32 bolt_port;
    char *socket_dir;
    char *pid_file_name;
    char *conn_string;
    char *graph_path;
};

bool bool_conf_parser(char *str);

int64 long_conf_parser(char *str);

char *str_conf_parser(char *str);

char *str_connstr_parser(char *str);


void free_configuration(struct garph_protocol_conf s);

#endif
