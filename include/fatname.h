#ifndef FATNAME_H_
#define FATNAME_H_

#include "str.h"

#define LOWERCASE_ISSUE	  0x01
#define BAD_CHARACTER	  0x02
#define BAD_TERMINATION   0x04
#define NOT_CONVERTED_YET 0x08
#define TOO_MANY_DOTS     0x10

int is_fatname(const char* name);
int fatname_to_name(const char* fatname, char* name);
int name_to_fatname(const char* name, char* fatname);

#endif