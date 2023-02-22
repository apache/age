
#include "utils/option_helper.h"


void usage(void) {
    fprintf(stdout, "%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
    fprintf(stdout, "\t-h,\t--help          Print this help.\n");
    fprintf(stdout, "\t-f,\t--config        Target configuration file, need filepath.\n");
    fprintf(stdout, "\t-v,\t--version       Print version string.\n");
    fprintf(stdout, "\t-d,\t--debug         Operating with debug mode.\n");
}

void show_version(void) {
    fprintf(stdout, "%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}



