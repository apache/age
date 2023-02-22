
#ifndef UTILS_H
#define UTILS_H

#include "graph_protocol_stddef.h"

size_t concaternating_byte_array(char **dest, size_t dest_size, char *src, size_t src_size);

char *replace_double_quote_to_single_quote(char **dest, char *src, size_t src_size);

char *concaternating_default_graphpath(char *graphpath, char *query);

int32 read_from_conf(char *conf_abs_path);


#endif

