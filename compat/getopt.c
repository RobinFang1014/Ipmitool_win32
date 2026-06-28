#include "msvc_compat.h"

char *optarg = NULL;
int optind = 1;
int opterr = 1;
int optopt = 0;

int getopt(int argc, char * const argv[], const char *optstring)
{
    static const char *next = NULL;
    const char *optdecl;
    char c;

    optarg = NULL;

    if (next == NULL || *next == '\0') {
        if (optind >= argc || argv[optind] == NULL) {
            return -1;
        }
        if (argv[optind][0] != '-' || argv[optind][1] == '\0') {
            return -1;
        }
        if (strcmp(argv[optind], "--") == 0) {
            optind++;
            return -1;
        }
        next = argv[optind] + 1;
        optind++;
    }

    c = *next++;
    optopt = c;
    optdecl = strchr(optstring, c);
    if (optdecl == NULL || c == ':') {
        if (opterr) {
            fprintf(stderr, "Unknown option -%c\n", c);
        }
        return '?';
    }

    if (optdecl[1] == ':') {
        if (*next != '\0') {
            optarg = (char *)next;
            next = NULL;
        } else if (optind < argc) {
            optarg = argv[optind++];
            next = NULL;
        } else {
            if (opterr) {
                fprintf(stderr, "Option -%c requires an argument\n", c);
            }
            return (optstring[0] == ':') ? ':' : '?';
        }
    }

    return c;
}

