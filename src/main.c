#include <getopt.h>

#include "utils/option_helper.h"
#include "utils/config_helper.h"
#include "communication/socket_manager.h"
#include "libwebsocket/lws.h"

static struct option long_opts[] = {
        {"debug",       no_argument,       NULL, 'd'},
        {"config-file", required_argument, NULL, 'f'},
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'v'},
        {NULL, 0,                          NULL, 0}
};


struct garph_protocol_conf garph_protocol_conf;

int main(int argc, char **argv) {
    int opt;
    int debug_level = 0;
    int *long_opt_index = NULL;

    char *target;

    if (argc == 1) { goto TEST; }
    if (argc == 2) { goto TEMP; }

    while ((opt = getopt_long(argc, argv, "df:hv", long_opts, long_opt_index)) != -1) {
        switch (opt) {
            case 'd':
                debug_level = 1;
                break;
            case 'f':
                if (!optarg) {
                    usage();
                    exit(1);
                }
            TEMP:
                read_from_conf(argv[1]);
                ws_server(garph_protocol_conf.bolt_port);
                exit(0);
            case 'h':
                usage();
                exit(0);
            case 'v':
                show_version();
                exit(0);
            default:
                if (!optarg) {
                    usage();
                    exit(1);
                }
        }
    }

    free_configuration(garph_protocol_conf);


    TEST:
    target = "{\"title\": \"The Matrix\", \"tagline\": \"Welcome to the Real World\", \"released\": 1999}";
    target = "{\"ID\":null,\"name\":\"Doe\",\"first-name\":\"John\",\"age\":25,\"hobbies\":[\"reading\",\"cinema\",{\"sports\":[\"volley-ball\",\"snowboard\"]}],\"address\":{}}";
    lws_lejp_parser(target);

    return 0;
}


