#include "lavender.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    
    bool usingMain = false;
    //parse command line arguments
    for(int i = 1; i < argc; i++) {
        if(usingMain) {
            //arguments NYI
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
