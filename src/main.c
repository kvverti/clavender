#include "lavender.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

int main(int argc, char* argv[]) {

    bool usingMain = false;
    //parse command line arguments
    for(int i = 1; i < argc; i++) {
        if(usingMain) {
            lv_mainArgs.args = &argv[i];
            lv_mainArgs.count = argc - i;
            break;
        } else if(strcmp(argv[i], "-version") == 0) {
            puts("C Lavender version 1.0");
            return 0;
        } else if(strcmp(argv[i], "-fp") == 0) {
            //-fp takes one argument
            if(i == (argc - 1)) {
                puts("-fp takes one argument");
                exit(1);
            }
            i++;
            lv_filepath = argv[i];
        } else if(strcmp(argv[i], "-debug") == 0) {
            lv_debug = true;
        } else if(strcmp(argv[i], "-maxStackSize") == 0) {
            //-maxStackSize takes one argument
            if(i == (argc - 1)) {
                puts("-maxStackSize takes one argument");
                exit(1);
            }
            i++;
            char* end;
            size_t size = strtoul(argv[i], &end, 10);
            assert(end);
            //get the unit
            // K - kibibyte
            // M - mebibyte
            // G - gibibyte
            size_t multiplier;
            switch(*end) {
                case 'K':
                    multiplier = 1024;
                    break;
                case 'M':
                    multiplier = 1024 * 1024;
                    break;
                case 'G':
                    multiplier = 1024 * 1024 * 1024;
                    break;
                default:
                    //the whole string was not converted
                    printf("Argument %s must be a nonnegative integer with suffix K, M, or G\n",
                        argv[i]);
                    exit(1);
            }
            //in number of TextBufferObj
            lv_maxStackSize = size * multiplier / sizeof(TextBufferObj);
        } else if(strcmp(argv[i], "-help") == 0) {
            puts(
                "Usage: lavender [options] [main file] [args]\n"
                "where 'options' is one of:\n"
                "         -fp <directory> : Sets the filepath. The filepath is where\n"
                "                           Lavender looks for user defined files.\n"
                "                  -debug : Enables debug logging.\n"
                "    -maxStackSize <size> : Sets the maximum size of the Lavender stack\n"
                "                           in kibibytes (K), mebibiyes (M), or gibibytes (G).\n"
                "                -version : Print version information and exit.\n"
                "                   -help : Print this information and exit."
            );
            return 0;
        } else if(strncmp(argv[i], "-", 1) == 0) {
            printf("Argument %s not recognized\n", argv[i]);
            exit(1);
        } else {
            //set the main file to run
            lv_mainFile = argv[i];
            usingMain = true;
        }
    }
    lv_run();
    return 0;
}
