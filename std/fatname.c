#include "../include/fatname.h"

int is_fatname(const char* name) {
    short result = 0;
    unsigned short iterator = 0;
    for (iterator = 0; iterator < 11; iterator++) {
        if (name[iterator] < 0x20 && name[iterator] != 0x05) {
            result = result | BAD_CHARACTER;
        }
        
        switch (name[iterator]) {
            case 0x2E: {
                if ((result & NOT_CONVERTED_YET) == NOT_CONVERTED_YET) result |= TOO_MANY_DOTS;
                result ^= NOT_CONVERTED_YET;
                break;
            }

            case 0x22:
            case 0x2A:
            case 0x2B:
            case 0x2C:
            case 0x2F:
            case 0x3A:
            case 0x3B:
            case 0x3C:
            case 0x3D:
            case 0x3E:
            case 0x3F:
            case 0x5B:
            case 0x5C:
            case 0x5D:
            case 0x7C: result = result | BAD_CHARACTER; break;
            default: break;
        }

        if (name[iterator] >= 'a' && name[iterator] <= 'z') {
            result = result | LOWERCASE_ISSUE;
        }
    }

    return result;
}

int fatname_to_name(const char* fatname, char* name) {
    if (fatname[0] == '.') {
        if (fatname[1] == '.') {
            str_strncpy(name, "..", 3);
            return 1;
        }
        str_strncpy(name, ".", 2);
        return 1;
    }

    int i, j = 0;
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

    name[j] = '\0';
    return 1;
}

int name_to_fatname(const char* name, char* fatname) {
    int i = 0, j = 0;
    while (name[i] != '\0' && name[i] != '.' && j < 8) {
        fatname[j++] = name[i++];
    }
    
    while (j < 8) fatname[j++] = ' ';
    if (name[i] == '.') i++;

    int ext = 0;
    while (name[i] != '\0' && ext < 3) {
        fatname[j++] = name[i++];
        ext++;
    }

    while (j < 11) fatname[j++] = ' ';
    str_uppercase(fatname);
    return 1;
}
