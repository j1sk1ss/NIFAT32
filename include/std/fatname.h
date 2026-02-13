/*
License:
    MIT License

    Copyright (c) 2025 Nikolay 

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Description:
    This file contains main tools for working with fat name style 8.3

Dependencies:
    - str.h - String functions.
*/

#ifndef FATNAME_H_
#define FATNAME_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/str.h>

#define PATH_SPLITTER '/'

/*
Convert fatname 8.3 to default name and ext.
Example: NAME    EXT to name.ext
Params:
- fatname - Source fatname full name.
- name - Destination buffer where will be placed name.

Return 1.
*/
int fatname_to_name(const char* fatname, char* name);

/*
Convert default name and ext to fatname 8.3.
Example: name.ext to NAME    EXT
Params:
- name - Source full name.
- fatname - Destination buffer where will be placed fatname.

Return 1.
*/
int name_to_fatname(const char* name, char* fatname);

/*
Convert path to fatname 8.3.
Example: root/tdir/dir2/name.ext to ROOT/TDIR/DIR2/NAME    EXT
Params:
- path - Source path.
- fatnames - Destination buffer where will be placed converted path.

Return 1.
*/
int path_to_fatnames(const char* path, char* fatnames);

/*
Convert path to 8.3 in one memory.
Example: root/tdir/dir2/name.ext to ROOT/TDIR/DIR2/NAME    EXT
Params:
- path - Source path.

Return 1.
*/
int path_to_83(char* path);

/*
Extract file name from path. 
Example: root/tdir/file.txt, name=file.txt
         Note: Same for 8.3 path
Params:
- path - Source path.
- name - Filename buffer for saving.

Return 1.
*/
int extract_name(const char* path, char* name);

/*
Unpack 8.3 name to name and extention.
Example: NAME    EXT, name=NAME, ext=EXT
Params:
- name83 - 8.3 name.
- name - Buffer for name.
- ext - Buffer for extention.

Return 1.
*/
int unpack_83_name(const char* name83, char* name, char* ext);

#ifdef __cplusplus
}
#endif
#endif