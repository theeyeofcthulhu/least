#ifndef LEXER_H_
#define LEXER_H_

typedef struct token_s token;

#include "dictionary.h"
#include "lstring.h"

struct token_s{
    enum{KEYWORD, OPERATOR, COMPARATOR, LOGICAL, STRING, LSTRING, CHAR, NUMBER, VAR, SEP, EOL} type;
    union{
        ekeyword key;
        earit_operation arit_operator;
        ecmp_operation cmp_operator;
        elogical_operation log_operator;
        char* string;
        lstring* lstring;
        char the_char;
        int num;
        char* name;
    }value;
    int line;
};

token** lex_source(char* source, int* lex_len);
void free_tokens(token** ts, int len);
bool has_next_arg(token** ts, int* len);

token* make_key(ekeyword key, int line);
token* make_string(char* string, int line);
token* make_num(int value, int line);
token* make_cmp(ecmp_operation cmp, int line);
token* make_arit(earit_operation arit, int line);
token* make_log(elogical_operation log, int line);
token* make_var(char* name, int line);
token* make_eol(int line);
token* make_sep(int line);

#endif // LEXER_H_
