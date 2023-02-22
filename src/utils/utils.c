

#include "utils/utils.h"
#include "utils/config_helper.h"


size_t concaternating_byte_array(char **dest, size_t dest_size, char *src, size_t src_size) {

    char *dest_backup;
    if (*dest == NULL || dest_size == 0) {
        *dest = malloc(src_size);
        memcpy(*dest, src, src_size);
    } else {
        dest_backup = (char *) malloc(dest_size);
        memcpy(dest_backup, *dest, dest_size);
//        *dest = realloc((char *) *dest, dest_size + src_size+1);
        free(*dest);

        *dest = malloc(dest_size + src_size);
        memcpy(*dest, dest_backup, dest_size);
        memcpy(*dest + dest_size, src, src_size);

        free(dest_backup);
    }
    return dest_size + src_size;
}

char *replace_double_quote_to_single_quote(char **dest, char *src, size_t src_size) {
    for (int i = 0; i < src_size; ++i) {
        if (src[i] == '\"') {
            src[i] = '\'';
        }
    }
    return *dest;
}

char *concaternating_default_graphpath(char *graphpath, char *query) {

    printf("%s", graphpath);

    char *f = "set graph_path='%s'; %s;";

    char *query_with_graphpath = malloc((strlen(f) - 4) + strlen(graphpath) + strlen(query) + 1);
    sprintf(query_with_graphpath, f, graphpath, query);

    return query_with_graphpath;

}

