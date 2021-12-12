#ifndef LSTRING_H_
#define LSTRING_H_

#include <stdbool.h>

#include "dictionary.h"

#define NODE_ARR_SZ 1024

typedef struct lstring_s lstring;

#include "lexer.h"

typedef struct{
    bool is_char;
    char default_char;
    const char* expression;
    int expr_len;
    char expandeur;
    char the_char;
}str_token;

enum str_tokens{
NEW_LINE,
TAB,
BACKSLASH,
QUOTE,
SINGLE_QUOTE,
BRACKET_LEFT,
BRACKET_RIGHT,
STR_TOKEN_END,
};

struct lstring_s{
    token* ts[NODE_ARR_SZ];
    int ts_sz;
};

extern const str_token token_structs[STR_TOKEN_END];

lstring* parse_string(char* string, int line);

#endif // LSTRING_H_
