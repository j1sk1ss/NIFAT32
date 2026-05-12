#include <std/fatname.h>

int nft32_fatname_to_name(const char* fatname, char* name) {
    if (fatname[0] == '.') {
        if (fatname[1] == '.') {
            nft32_str_strncpy(name, "..", 3);
            return 1;
        }

        nft32_str_strncpy(name, ".", 2);
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

int nft32_name_to_fatname(const char* name, char* fatname) {
    int i = 0, j = 0;
    if (name[0] == '.') {
        if (name[1] == '.') {
            nft32_str_strncpy(fatname, "..", 3);
            i = j = 2;
        }
        else {
            nft32_str_strncpy(fatname, ".", 2);
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
    nft32_str_uppercase(fatname);
    fatname[j] = 0;
    return 1;
}

int nft32_path_to_fatnames(const char* path, char* fatnames) {
    nft32_str_strcpy(fatnames, path);

    int i;
    for (i = nft32_str_strlen(path); path[i] != PATH_SPLITTER && i >= 0; i--);
    i++;
    
    nft32_name_to_fatname(path + i, fatnames + i);
    nft32_str_uppercase(fatnames);
    return 1;
}

int nft32_path_to_83(char* path) {
    int i = nft32_str_strlen(path) - 1;
    while (i > 0 && path[i] != PATH_SPLITTER) i--;
    if (path[i] == PATH_SPLITTER) i++;
    char tmp[16] = { 0 };
    nft32_name_to_fatname(path + i, tmp);
    nft32_str_memcpy(path + i, tmp, 11);
    nft32_str_uppercase(path);
    return 1;
}

int nft32_extract_name(const char* path, char* name) {
    int i;
    for (i = nft32_str_strlen(path); path[i] != PATH_SPLITTER && i > 0; i--);
    nft32_str_strcpy(name, path + i);
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
