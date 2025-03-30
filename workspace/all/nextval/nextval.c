#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

void printUsage()
{
    printf("NextUI nextval\n"
           "[read]\n"
           "  nextval key\n\n"
           "[write]\n"
           "  nextval key vale\n");
    CFG_print();
}

int main(int argc, char *argv[])
{
    CFG_init(NULL);

    if (argc <= 1) {
        printUsage();
        return EXIT_SUCCESS;
    }

    // get
    // not really maintainable this way, this should really all be c++ with proper type deduction
    // instead of naive copy paste
    if (argc == 2) {
        char setting_value[MAX_PATH];
        CFG_get(argv[1], setting_value);
        if(strlen(setting_value))
            printf("%s\n", setting_value);
        else
            printf("unknown key: %s", argv[1]);
    }
    // set
    else if(argc == 3) {

    }
    else {
        printf("Error: Invalid argument '%s'\n", argv[1]);
        printUsage();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}