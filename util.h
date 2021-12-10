#ifndef UTIL_H_
#define UTIL_H_

#include <stdbool.h>

#include "dictionary.h"

struct compile_info{
    bool defined_vars[NODE_ARR_SZ];

    int body_id;

    int var_id;
    char* known_vars[NODE_ARR_SZ];

    int string_id;
    char* known_strings[NODE_ARR_SZ];

    bool req_libs[LIB_ENUM_END];
};

char* unite(char** src, int off, int len);
char** sepbyspc(char* src, int* dest_len);
void freewordarr(char** arr, int len);
char** spltlns(char* str, int *len);
struct compile_info* create_c_info();
char* read_source_code(char* filename);

#endif // UTIL_H_
