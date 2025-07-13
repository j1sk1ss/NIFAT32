#include "../include/options.h"

int process_input(int argc, char* argv[], opt_t* opt) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], SOURCE_OPTION)) {
            if (i + 1 < argc) opt->source_path = argv[++i];
            else {
                fprintf(stderr, "Error: Source path required after %s\n", SOURCE_OPTION);
                return 0;
            }
        } 
        else if (!strcmp(argv[i], SAVE_OPTION)) {
            if (i + 1 < argc) opt->save_path = argv[++i];
            else {
                fprintf(stderr, "Error: Output path required after %s\n", SAVE_OPTION);
                return 0;
            }
        }
        else if (!strcmp(argv[i], OUTPUT_SIZE)) {
            if (i + 1 < argc) opt->v_size = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Output size required after %s\n", OUTPUT_SIZE);
                return 0;
            }
        }
        else if (!strcmp(argv[i], SPC)) {
            if (i + 1 < argc) opt->spc = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Sectors count required after %s\n", SPC);
                return 0;
            }
        }
        else if (!strcmp(argv[i], FAT_COUNT_OPT)) {
            if (i + 1 < argc) opt->fc = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: Fat count required after %s\n", FAT_COUNT_OPT);
                return 0;
            }
        }
        else if (!strcmp(argv[i], BS_BACKUPS_OPT)) {
            if (i + 1 < argc) opt->bsbc = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: bs backups count required after %s\n", BS_BACKUPS_OPT);
                return 0;
            }
        }
        else if (!strcmp(argv[i], JOURNALS_BACKUPS_OPT)) {
            if (i + 1 < argc) opt->jc = atoi(argv[++i]);
            else {
                fprintf(stderr, "Error: journals count required after %s\n", JOURNALS_BACKUPS_OPT);
                return 0;
            }
        }
    }

    return 1;
}
