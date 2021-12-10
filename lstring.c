#include <string.h>
#include <stdlib.h>

#include "error.h"
#include "lstring.h"

const str_token token_structs[STR_TOKEN_END] = {
{false, 0, "\",0xa,\"",     7, 'n',     '\n'},      /* Newline */
{false, 0, "\",0x9,\"",     7, 't',     '\t'},      /* Tabstop */
{false, 0, "\\",            1, '\\',    '\\'},      /* The character '\' */
{false, 0, "\",0x22,\"",    8, '\"',    '\"'},      /* The character '"' */
{false, 0, "\",0x27,\"",    8, '\'',    '\''},      /* The character "'" */
{false, 0, "\",0x5B,\"",    8, '[',     '['},       /* The character '[' */
{false, 0, "\",0x5D,\"",    8, ']',     ']'},       /* The character ']' */
};

/* Check validity of string and insert escape sequences */
char* parse_string(char* string, int line){
    char* result;
    int result_len = 0;

    int string_len = strlen(string);

    int acc = 0;

    str_token tokens[string_len];
    int tokens_len = 0;

    /* Parse string that comes after the instruction */
    for (int j = 0; j < string_len; j++) {
        switch(string[j]){
            case '"':
                if(acc == 0)
                    compiler_error_on_false((j+1) < string_len, line + 1, "Reached end of line while parsing first '\"' in string\n");

                acc++;
                compiler_error_on_true(acc > 2, line + 1, "Found more than two '\"' while parsing string\n");
                if(acc == 2)
                    compiler_error_on_false((j+1) == string_len, line + 1, "Expected end of line after string\n");
                break;
            case '\\':
                j++;
                compiler_error_on_true(j >= string_len - 1, line + 1, "Reached end of line while trying to parse escape sequence\n");
                bool found = false;
                for(int i = 0; i < STR_TOKEN_END; i++){
                    if(string[j] == token_structs[i].expandeur){
                        tokens[tokens_len++] = token_structs[i];
                        found = true;
                    }
                }
                compiler_error_on_false(found, line + 1, "Failed to parse escape sequence '\\%c'\n", string[j]);
                break;
            default:
            {
                str_token token;
                token.is_char = true;
                token.default_char = string[j];
                token.expr_len = 1;
                tokens[tokens_len++] = token;
                break;
            }
        }
        compiler_error_on_true(string[j] != '"' && acc == 0, line + 1,
                                "First '\"' not at beginning of parsed sequence, instead found '%c'\n", string[j]);
    }
    compiler_error_on_false(acc == 2, line + 1, "Did not find two '\"' while parsing string\n");

    for (int i = 0; i < tokens_len; i++)
        result_len += tokens[i].expr_len;

    result = malloc((result_len + 1) * sizeof(char));
    memset(result, '\0', result_len + 1);
    for (int i = 0, j = 0; i < tokens_len; i++) {
        if(tokens[i].is_char)
            result[j] = tokens[i].default_char;
        else
            strcat(result, tokens[i].expression);
        j += tokens[i].expr_len;
    }

    return result;
}
