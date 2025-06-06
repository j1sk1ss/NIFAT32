#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SAVE_OPTION		"-o"
#define SOURCE_OPTION	"-s"
#define OUTPUT_SIZE     "--volume-size" /* In MB */
#define SPC             "--spc"         /* Sectors per cluster */
#define FAT_COUNT_OPT   "--fc"
#define BS_BACKUPS_OPT  "--bsbc"        /* Bootsector backups count */

typedef struct {
    char* save_path;
    char* source_path;
    int v_size;
    int spc;
    int fc;
    int bsbc;
} opt_t;

int process_input(int argc, char* argv[], opt_t* opt);

#endif