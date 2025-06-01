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
            str_strncpy(name, "..", 2);
            return 1;
        }

        str_strncpy(name, ".", 1);
        return 1;
    }

    unsigned short counter = 0;
    for (counter = 0; counter < 8; counter++) {
        if (fatname[counter] == 0x20) {
            name[counter] = '.';
            break;
        }

        name[counter] = fatname[counter];
    }

    if (counter == 8) {
        name[counter] = '.';
    }

    unsigned short sec_counter = 8;
    for (sec_counter = 8; sec_counter < 11; sec_counter++) {
        ++counter;
        if (fatname[sec_counter] == 0x20 || fatname[sec_counter] == 0x20) {
            if (sec_counter == 8) {
                counter -= 2;
            }

            break;
        }
        
        name[counter] = fatname[sec_counter];		
    }

    ++counter;
    while (counter < 12) {
        name[counter] = ' ';
        ++counter;
    }

    name[12] = '\0';
    return 1;
}

int name_to_fatname(const char* name, char* fatname) {
    int has_ext            = 0;
    unsigned short dot_pos = 0;
    unsigned int counter   = 0;

    while (counter++ <= 8) {
        if (name[counter] == '.' || name[counter] == '\0') {
            if (name[counter] == '.') has_ext = 1;
            dot_pos = counter;
            break;
        }
        else {
            fatname[counter] = name[counter];
        }
    }

    if (counter > 9) {
        counter = 8;
        dot_pos = 8;
    }
    
    unsigned short extCount = 8;
    while (extCount < 11) {
        if (name[counter] && has_ext) fatname[extCount] = name[counter];
        else fatname[extCount] = ' ';

        counter++;
        extCount++;
    }

    counter = dot_pos;
    while (counter < 8) {
        fatname[counter++] = ' ';
    }

    return 1;
}