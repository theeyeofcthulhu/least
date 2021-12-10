#ifndef LEXER_H_
#define LEXER_H_

#include "dictionary.h"

typedef struct{
    enum{KEYWORD, OPERATOR, COMPARATOR, LOGICAL, STRING, CHAR, NUMBER, VAR, EOL} type;
    union{
        ekeyword key;
        earit_operation arit_operator;
        ecmp_operation cmp_operator;
        elogical_operation log_operator;
        char* string;
        char the_char;
        int num;
        char* name;
    }value;
    int line;
}token;

token** lex_source(char* source, int* lex_len);
void free_tokens(token** ts, int len);

#endif // LEXER_H_
