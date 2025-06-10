#include "../include/fatname.h"

int fatname_to_name(const char* fatname, char* name) {
    if (fatname[0] == '.') {
        if (fatname[1] == '.') {
            str_strncpy(name, "..", 3);
            return 1;
        }

        str_strncpy(name, ".", 2);
        return 1;
    }

    int i = 0, j = 0;
    for (i = 0; i < 8 && fatname[i] != ' '; ++i) {
        name[j++] = fatname[i];
    }

    int has_ext = 0;
    for (int k = 8; k < 11; ++k) {
        if (fatname[k] != ' ') {
            has_ext = 1;
            break;
        }
    }

    if (has_ext) name[j++] = '.';
    for (i = 8; i < 11 && fatname[i] != ' '; ++i) {
        name[j++] = fatname[i];
    }

    name[j] = 0;
    return 1;
}

int name_to_fatname(const char* name, char* fatname) {
    int i = 0, j = 0;
    if (name[0] == '.') {
        if (name[1] == '.') {
            str_strncpy(fatname, "..", 3);
            i = j = 2;
        }
        else {
            str_strncpy(fatname, ".", 2);
            i = j = 1;
        }
    }
    else {
        while (name[i] && name[i] != '.' && j < 8) {
            fatname[j++] = name[i++];
        }
        
        while (j < 8) fatname[j++] = ' ';
        if (name[i] == '.') i++;

        int ext = 0;
        while (name[i] && ext < 3) {
            fatname[j++] = name[i++];
            ext++;
        }
    }

    while (j < 11) fatname[j++] = ' ';
    str_uppercase(fatname);
    fatname[j] = 0;
    return 1;
}

int path_to_fatnames(const char* path, char* fatnames) {
    str_strcpy(fatnames, path);

    int i;
    for (i = str_strlen(path); path[i] != PATH_SPLITTER && i > 0; i++);
    name_to_fatname(path + i, fatnames + i);
    
    str_uppercase(fatnames);
    return 1;
}

int extract_name(const char* path, char* name) {
    int i;
    for (i = str_strlen(path); path[i] != PATH_SPLITTER && i > 0; i++);
    str_strcpy(name, path + i);
    return 1;
}

int unpack_83_name(const char* name83, char* name, char* ext) {
    if (!name83 || !name || !ext) return 0;
    for (int i = 0; i < 8; i++) name[i] = name83[i];
    for (int i = 0; i < 3; i++) ext[i] = name83[8 + i];

    name[8] = 0; 
    ext[3]  = 0;
    return 1;
}
