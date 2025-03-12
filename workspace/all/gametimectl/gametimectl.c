// heavily modified from the Onion original: https://github.com/OnionUI/Onion/blob/main/src/playActivity/playActivity.c
#include <stdio.h>

#include "defines.h"
#include "api.h"
#include "utils.h"

#include <sqlite3.h>
#include <gametimedb.h>

void printUsage()
{
    printf("Usage: gametimectl list             -> List all play activities\n"
           "       gametimectl start [rom_path] -> Launch the counter for this rom\n"
           "       gametimectl resume           -> Resume the last rom as a new play activity\n"
           "       gametimectl stop [rom_path]  -> Stop the counter for this rom\n"
           "       gametimectl stop_all         -> Stop the counter for all roms\n");
}

int main(int argc, char *argv[])
{
    if (argc <= 1) {
        printUsage();
        return EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "start") == 0) {
            if (i + 1 < argc) {
                LOG_info("Start tracking: %s\n", argv[i+1]);
                play_activity_start(argv[++i]);
            }
            else {
                printf("Error: Missing rom_path argument\n");
                printUsage();
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "resume") == 0) {
            LOG_info("Resuming tracking for last game\n");
            play_activity_resume();
        }
        else if (strcmp(argv[i], "stop") == 0) {
            if (i + 1 < argc) {
                LOG_info("Stop tracking: %s\n", argv[i+1]);
                play_activity_stop(argv[++i]);
            }
            else {
                printf("Error: Missing rom_path argument\n");
                printUsage();
                return EXIT_FAILURE;
            }
        }
        else if (strcmp(argv[i], "stop_all") == 0) {
            LOG_info("Stopping tracking for all games\n");
            play_activity_stop_all();
        }
        else if (strcmp(argv[i], "list") == 0) {
            play_activity_list_all();
        }
        else {
            printf("Error: Invalid argument '%s'\n", argv[1]);
            printUsage();
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}