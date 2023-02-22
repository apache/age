

#ifndef SPEC_PACKSTREAM_H
#define SPEC_PACKSTREAM_H

#include "graph_protocol_stddef.h"

#define STRING8_SIZE 1

char *write_tiny_boolean(bool b);

char *write_tiny_string(char str[]);
char *write_string8(char str[]);
char *write_string16(char str[]);
char *write_tiny_int(int integer);

char *write_int16(int16 i);

char *write_tiny_list_marker(int element_count);
char *write_list8_marker(int element_count);
char *write_tiny_dict_marker(int element_count);
char *write_tiny_struct_marker(size_t element_count);

#endif