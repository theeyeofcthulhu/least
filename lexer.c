#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"
#include "dictionary.h"
#include "lstring.h"
#include "util.h"
#include "error.h"
#include "lstring.h"

ekeyword strkeytoekey(char* word){
    if(strcmp(word, "print") == 0)
        return K_PRINT;
    else if(strcmp(word, "exit") == 0)
        return K_EXIT;
    else if(strcmp(word, "if") == 0)
        return K_IF;
    else if(strcmp(word, "elif") == 0)
        return K_ELIF;
    else if(strcmp(word, "else") == 0)
        return K_ELSE;
    else if(strcmp(word, "while") == 0)
        return K_WHILE;
    else if(strcmp(word, "end") == 0)
        return K_END;
    else if(strcmp(word, "int") == 0)
        return K_INT;
    else if(strcmp(word, "str") == 0)
        return K_STR;
    else if(strcmp(word, "read") == 0)
        return K_READ;
    else if(strcmp(word, "set") == 0)
        return K_SET;
    else if(strcmp(word, "putchar") == 0)
        return K_PUTCHAR;
    else
        return K_NOKEY;
}

/* Structure for organizing comparison operations */
ecmp_operation strcmptoecmp(char* cmp){
    if(strcmp(cmp, "==") == 0)
        return EQUAL;
    else if(strcmp(cmp, "!=") == 0)
        return NOT_EQUAL;
    else if(strcmp(cmp, "<") == 0)
        return LESS;
    else if(strcmp(cmp, "<=") == 0)
        return LESS_OR_EQ;
    else if(strcmp(cmp, ">") == 0)
        return GREATER;
    else if(strcmp(cmp, ">=") == 0)
        return GREATER_OR_EQ;
    else
        return CMP_OPERATION_ENUM_END;
}

/* Structure for organizing arithmetic operations */
earit_operation strarittoearit(char* arit){
    if(strcmp(arit, "+") == 0)
        return ADD;
    else if(strcmp(arit, "-") == 0)
        return SUB;
    else if(strcmp(arit, "%") == 0)
        return MOD;
    else if(strcmp(arit, "/") == 0)
        return DIV;
    else if(strcmp(arit, "*") == 0)
        return MUL;
    else
        return ARIT_OPERATION_ENUM_END;
}

/* Structure for organizing arithmetic operations */
elogical_operation strlogtoelog(char* log){
    if(strcmp(log, "&&") == 0)
        return AND;
    else if(strcmp(log, "||") == 0)
        return OR;
    else
        return LOGICAL_OPS_END;
}

void checkbanned(char* s, int line){
    for (size_t i = 0; i < strlen(s); i++)
        compiler_error_on_true((isdigit(s[i]) || !isascii(s[i]) || ispunct(s[i])), line + 1, "Invalid character in variable name: '%s'\n", s);
}

token* make_key(ekeyword key, int line){
    token* res = malloc(sizeof(token));
    res->type = KEYWORD;
    res->value.key = key;
    res->line = line;

    return res;
}

token* make_string(char* string, int line){
    token* res = malloc(sizeof(token));
    res->type = STRING;
    res->value.string = string;
    res->line = line;

    return res;
}

token* make_lstring(lstring* string, int line){
    token* res = malloc(sizeof(token));
    res->type = LSTRING;
    res->value.lstring = string;
    res->line = line;

    return res;
}

token* make_num(int value, int line){
    token* res = malloc(sizeof(token));
    res->type = NUMBER;
    res->value.num = value;
    res->line = line;

    return res;
}

token* make_cmp(ecmp_operation cmp, int line){
    token* res = malloc(sizeof(token));
    res->type = COMPARATOR;
    res->value.cmp_operator = cmp;
    res->line = line;

    return res;
}

token* make_arit(earit_operation arit, int line){
    token* res = malloc(sizeof(token));
    res->type = OPERATOR;
    res->value.arit_operator = arit;
    res->line = line;

    return res;
}

token* make_log(elogical_operation log, int line){
    token* res = malloc(sizeof(token));
    res->type = LOGICAL;
    res->value.log_operator = log;
    res->line = line;

    return res;
}

token* make_var(char* name, int line){
    token* res = malloc(sizeof(token));
    res->type = VAR;
    res->value.name = name;
    res->line = line;

    return res;
}

token* make_eol(int line){
    token* res = malloc(sizeof(token));
    res->type = EOL;
    res->line = line;

    return res;
}

token* make_sep(int line){
    token* res = malloc(sizeof(token));
    res->type = SEP;
    res->line = line;

    return res;
}

void free_tokens(token** ts, int len){
    for(int i = 0; i < len; i++)
        free(ts[i]);
    free(ts);
}

bool has_next_arg(token** ts, int* len){
    *len = 0;

    while((ts[(*len)]->type != SEP) && (ts[(*len)]->type != EOL))
        *len += 1;

    return ts[(*len)]->type == SEP;
}

#define MAX_TOKENS 2048
token** lex_source(char* source, int* lex_len){
    token** tokens = malloc(MAX_TOKENS * sizeof(token*));
    *lex_len = 0;

    int lines_len;
    char** lines = spltlns(source, &lines_len);

    for(int i = 0; i < lines_len; i++){
        if(!lines[i])
            continue;

        int words_len;
        char** words = sepbyspc(lines[i], &words_len);
        compiler_error_on_false(words, i + 1, "Could not split line into words\n");

        for(int j = 0; j < words_len; j++){
            if(words[j][0] == '\"'){
                //TODO: Strings with multiple spaces
                int start = j;

                bool found_end = false;
                size_t k = 1;
                while(!found_end){
                    //FIXME: invalid read of size 1
                    if(words[j][k] == '\"' && words[j][k - 1] != '\\'){
                        compiler_error_on_false(k == strlen(words[j]) - 1, i + 1, "Quote not end of word '%s'\n", words[j]);
                        found_end = true;
                    }
                    if(k++ > strlen(words[j])){
                        compiler_error_on_false(j++ < words_len - 1, i + 1, "Could not find end of string in line '%s'\n", lines[i]);
                        k = 0;
                    }
                }
                char* string_to_parse = unite(words, start, j);

                lstring* parsed = parse_string(string_to_parse, i);

                free(string_to_parse);

                tokens[(*lex_len)++] = make_lstring(parsed, i);

                continue;
            }else if(isdigit(words[j][0])){
                for(size_t k = 0; k < strlen(words[j]); k++)
                    compiler_error_on_false(isdigit(words[j][k]), i + 1, "Expected digit, got: '%c' in number '%s'\n", words[j][k], words[j]);

                tokens[(*lex_len)++] = make_num(strtol(words[j], NULL, 10), i);

                continue;
            }

            ecmp_operation cmp = strcmptoecmp(words[j]);
            if(cmp != CMP_OPERATION_ENUM_END){
                tokens[(*lex_len)++] = make_cmp(cmp, i);
                continue;
            }

            earit_operation arit = strarittoearit(words[j]);
            if(arit != ARIT_OPERATION_ENUM_END){
                tokens[(*lex_len)++] = make_arit(arit, i);
                continue;
            }

            elogical_operation log = strlogtoelog(words[j]);
            if(log != LOGICAL_OPS_END){
                tokens[(*lex_len)++] = make_log(log, i);
                continue;
            }

            ekeyword key = strkeytoekey(words[j]);
            if(key != K_NOKEY){
                tokens[(*lex_len)++] = make_key(key, i);
                continue;
            }

            if(strcmp(words[j], ";") == 0){
                tokens[(*lex_len)++] = make_sep(i);
                continue;
            }

            char* var = malloc((strlen(words[j]) + 1) * sizeof(char));
            strcpy(var, words[j]);
            checkbanned(var, i);

            tokens[(*lex_len)++] = make_var(var, i);
        }
        tokens[(*lex_len)++] = make_eol(i);

        freewordarr(words, words_len);
    }
    freewordarr(lines, lines_len);

    tokens = realloc(tokens, *lex_len * sizeof(token*));

    return tokens;
}
