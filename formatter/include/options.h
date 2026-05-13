/*
License:
    MIT License. See LICENSE file in project root.
    Copyright (c) 2025 Nikolay

Description:
    Command-line option definitions and parser interface for the formatter.

Dependencies:
    - stdio.h - Standard I/O declarations.
    - string.h - String utilities.
    - stdlib.h - Standard library utilities.
*/

#ifndef OPTIONS_H_
#define OPTIONS_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define SAVE_OPTION          "-o"
#define SOURCE_OPTION        "-s"
#define OUTPUT_SIZE          "--volume-size" /* In MB */
#define SPC                  "--spc"         /* Sectors per cluster */
#define FAT_COUNT_OPT        "--fc"
#define BS_BACKUPS_OPT       "--bsbc"        /* Bootsector backups count */
#define B_BS_OPT             "--b-bsbc"      /* [DEBUG] Wrong bootsectos */
#define JOURNALS_BACKUPS_OPT "--jc"

typedef struct {
    char* save_path;
    char* source_path;
    int   v_size; // virtual disk size in mb
    int   spc;    // sector per cluster
    int   fc;     // fats count
    int   bsbc;   // bootsector count
    int   b_bsbc; // [DEBUG] Wrong boot sectors
    int   jc;     // journals count
    int   ec;     // errors count (error storage)
} opt_t;

int process_input(int argc, char* argv[], opt_t* opt);

#endif
