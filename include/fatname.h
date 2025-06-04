#ifndef FATNAME_H_
#define FATNAME_H_

#include "str.h"

int fatname_to_name(const char* fatname, char* name);
int name_to_fatname(const char* name, char* fatname);

#endif