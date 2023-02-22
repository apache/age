

#include "utils/config_helper.h"

extern struct garph_protocol_conf garph_protocol_conf;

bool bool_conf_parser(char *str) {
    char *token;
    token = strtok(str, ":");
    token = strtok(NULL, ":");

    if (strlen(token) == 4)
        return true;
    else
        return false;
}

int64 long_conf_parser(char *str) {
    char *token;
    token = strtok(str, ":");
    token = strtok(NULL, ":");
    long l = strtol(token, NULL, 10);
    return l;
}

char *str_conf_parser(char *str) {
    char *token;
    token = strtok(str, ":");
    token = strtok(NULL, ":");

    return token;
}


char *str_connstr_parser(char *str) {
    char *token;
    token = strtok(str, ":");
    token = strtok(NULL, ":");
    return token;
}


void free_configuration(struct garph_protocol_conf s) {

    free(s.socket_dir);
    free(s.pid_file_name);
    free(s.conn_string);
    free(s.graph_path);
}


int32 read_from_conf(char *conf_abs_path) {

    FILE *f;
    char ch[200];
    int i = 0;

    char *tofree;
    f = fopen(conf_abs_path, "r");
    if (f != NULL) {
        char *tmp = malloc(300);
        tofree = tmp;

        while (fgets(ch, sizeof(ch), f) != NULL) {
            switch (i) {
                case 0:
                    garph_protocol_conf.log_debug = bool_conf_parser(ch);
                    break;
                case 1:
                    garph_protocol_conf.bolt_port = long_conf_parser(ch);
                    break;
                case 2:
                    tmp = str_conf_parser(ch);
                    garph_protocol_conf.socket_dir = malloc(strlen(tmp) + 1);
                    memcpy(garph_protocol_conf.socket_dir, tmp, strlen(tmp) + 1);
                    garph_protocol_conf.socket_dir[strlen(tmp) + 1] = '\0';
                    break;
                case 3:
                    tmp = str_conf_parser(ch);
                    garph_protocol_conf.pid_file_name = malloc(strlen(tmp) + 1);
                    memcpy(garph_protocol_conf.pid_file_name, tmp, strlen(tmp) + 1);
                    garph_protocol_conf.pid_file_name[strlen(tmp) + 1] = '\0';
                    break;
                case 4:
                    tmp = str_connstr_parser(ch);
                    garph_protocol_conf.conn_string = malloc(strlen(tmp) + 1);
                    memcpy(garph_protocol_conf.conn_string, tmp, strlen(tmp) + 1);
                    garph_protocol_conf.conn_string[strlen(tmp) + 1] = '\0';
                    break;
                case 5:
                    tmp = str_conf_parser(ch);
                    garph_protocol_conf.graph_path = malloc(strlen(tmp) + 1);
                    memcpy(garph_protocol_conf.graph_path, tmp, strlen(tmp) + 1);
                    garph_protocol_conf.graph_path[strlen(tmp) + 1] = '\0';
                    break;
                default:
                    break;
            }
            i++;
        }

        free(tofree);
    } else {
        perror("configuration file not exists.");
        return 1;
    }
    fclose(f);
    return 0;
}
