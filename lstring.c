#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "lstring.h"
#include "ast.h"

const str_token token_structs[STR_TOKEN_END] = {
{false, 0, "\",0xa,\"",     7, 'n',     '\n'},      /* Newline */
{false, 0, "\",0x9,\"",     7, 't',     '\t'},      /* Tabstop */
{false, 0, "\\",            1, '\\',    '\\'},      /* The character '\' */
{false, 0, "\",0x22,\"",    8, '\"',    '\"'},      /* The character '"' */
{false, 0, "\",0x27,\"",    8, '\'',    '\''},      /* The character "'" */
{false, 0, "\",0x5B,\"",    8, '[',     '['},       /* The character '[' */
{false, 0, "\",0x5D,\"",    8, ']',     ']'},       /* The character ']' */
};

char* str_tokens_to_str(str_token* ts, int ts_sz){
    char* result;
    int result_len = 0;

    for (int i = 0; i < ts_sz; i++)
        result_len += ts[i].expr_len;

    result = malloc((result_len + 1) * sizeof(char));
    memset(result, '\0', result_len + 1);
    for (int i = 0, j = 0; i < ts_sz; i++) {
        if(ts[i].is_char)
            result[j] = ts[i].default_char;
        else
            strcat(result, ts[i].expression);
        j += ts[i].expr_len;
    }

    return result;
}

/* Check validity of string and insert escape sequences */
lstring* parse_string(char* string, int line){
    lstring* res = malloc(sizeof(lstring));
    memset(res->ts, 0, NODE_ARR_SZ * sizeof(token*));
    res->ts_sz = 0;

    int string_len = strlen(string);

    int acc = 0;

    str_token* tokens = malloc(string_len * sizeof(str_token));
    int tokens_len = 0;

    /* Parse string that comes after the instruction */
    for (int i = 0; i < string_len; i++) {
        switch(string[i]){
            case '"':
                if(acc == 0)
                    compiler_error_on_false((i+1) < string_len, line + 1, "Reached end of line while parsing first '\"' in string\n");

                acc++;
                compiler_error_on_true(acc > 2, line + 1, "Found more than two '\"' while parsing string\n");
                if(acc == 2)
                    compiler_error_on_false((i+1) == string_len, line + 1, "Expected end of string after second quotation\n");
                break;
            case '\\':
                i++;
                compiler_error_on_true(i >= string_len - 1, line + 1, "Reached end of line while trying to parse escape sequence\n");
                bool found = false;
                for(int j = 0; j < STR_TOKEN_END && !found; j++){
                    if(string[i] == token_structs[j].expandeur){
                        tokens[tokens_len++] = token_structs[j];
                        found = true;
                    }
                }
                compiler_error_on_false(found, line + 1, "Failed to parse escape sequence '\\%c'\n", string[i]);
                break;
            /* We found a format parameter: parse the tokens in it */
            case '[':
            {
                char* part = str_tokens_to_str(tokens, tokens_len);
                if(strcmp(part, "") != 0)
                    res->ts[res->ts_sz++] = make_string(part, line + 1);

                tokens_len = 0;

                char* inside = string + i + 1;
                char* next_bracket = strchr(inside, ']');

                compiler_error_on_false(next_bracket, line + 1, "'[' without closing ']'\n");

                *next_bracket = '\0'; /* Make inside a NULL-terminated string */

                compiler_error_on_true(strchr(inside, '['), line + 1, "Found '[' inside format argument");

                int parsed_len;
                token** parsed_inside = lex_source(inside, &parsed_len);

                compiler_error_on_false(parsed_inside, line + 1, "Could not parse format parameter to tokens\n");
                compiler_error_on_true(parsed_len <= 0 || parsed_len >= 3, line + 1, "No or more than one tokens in format parameter\n");

                for(int j = 0; j < parsed_len - 1; j++){
                    compiler_error_on_false(parsed_inside[j]->type == OPERATOR ||
                                            parsed_inside[j]->type == VAR ||
                                            parsed_inside[j]->type == NUMBER, line + 1,
                                            "Only variables, numbers and operators are allowed inside a format parameter\n");
                    res->ts[res->ts_sz++] = parsed_inside[j];
                }

                i = next_bracket - string;

                break;
            }
            case ']':
                compiler_error(line + 1, "Unexpected closing ']'\n");
                break;
            default:
            {
                str_token n_token;
                n_token.is_char = true;
                n_token.default_char = string[i];
                n_token.expr_len = 1;
                tokens[tokens_len++] = n_token;
                break;
            }
        }
        compiler_error_on_true(string[i] != '"' && acc == 0, line + 1,
                                "First '\"' not at beginning of parsed sequence, instead found '%c'\n", string[i]);
    }
    compiler_error_on_false(acc == 2, line + 1, "Did not find two '\"' while parsing string\n");

    char* part = str_tokens_to_str(tokens, tokens_len);
    if(strcmp(part, "") != 0)
        res->ts[res->ts_sz++] = make_string(part, line + 1);

    free(tokens);

    return res;
}
