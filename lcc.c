#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "stack.h"

#define OP_CAPACITY 1000
#define STR_CAPACITY 1000

#define INT_LEN 8

#define SHELL_GREEN "\033[0;32m"
#define SHELL_RED   "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

/*TODO: comment everything */

enum conditionals{
IF,
ELIF,
ELSE,
NO_CONDITIONAL,
};

/* Structure for organizing comparison operations */
enum cmp_operations{
EQUAL,
NOT_EQUAL,
LESS,
LESS_OR_EQ,
GREATER,
GREATER_OR_EQ,
CMP_OPERATION_ENUM_END,
};

typedef struct{
    int op_enum;
    char* source_name;
    char* asm_name;
}cmp_operation;

const cmp_operation cmp_operation_structs[CMP_OPERATION_ENUM_END] = {
{EQUAL,         "==",   "je"},
{NOT_EQUAL,     "!=",   "jne"},
{LESS,          "<",    "jl"},
{LESS_OR_EQ,    "<=",   "jle"},
{GREATER,       ">",    "jg"},
{GREATER_OR_EQ, ">=",   "jge"},
};

/* Structure for organizing arithmetic operations */
enum arit_operations{
ADD,
SUB,
MOD,
DIV,
MUL,
ARIT_OPERATION_ENUM_END,
};

typedef struct{
    int op_enum;
    char* source_name;
    char* asm_name;
}arit_operation;

const arit_operation arit_operation_structs[CMP_OPERATION_ENUM_END] = {
{ADD,   "+",    "add"},
{SUB,   "-",    "sub"},
{MOD,   "%",    "div"},
{DIV,   "/",    "div"},
{MUL,   "*",    "mul"},
};

enum logical_operations{
AND,
OR,
LOGICAL_OPS_END,
};

typedef struct{
    int op_enum;
    char* source_name;
    char* asm_name;
}logical_operation;

const logical_operation logical_operation_structs[LOGICAL_OPS_END] = {
{AND,   "&&",   "and"},
{OR,    "||",   "or"},
};

typedef struct{
    char* name;
    char* value;
    char* mem_addr_ref;
}int_var;

/* Token for string escape sequences
 * Every character is converted into a token,
 * normal character just have the 'is_char' flag set
 * and are simply inserted */

typedef struct{
    bool is_char;
    char default_char;
    char* expression;
    int expr_len;
    char expandeur;
}str_token;

enum str_tokens{
NEW_LINE,
TAB,
BACKSLASH,
QUOTE,
BRACKET_LEFT,
BRACKET_RIGHT,
STR_TOKEN_END,
};

const str_token token_structs[STR_TOKEN_END] = {
{false, 0, "\",0xa,\"",     7, 'n'},    /* Newline */
{false, 0, "\",0x9,\"",     7, 't'},    /* Tabstop */
{false, 0, "\\",            1, '\\'},   /* The character '\' */
{false, 0, "\",0x22,\"",    8, '\"'},   /* The character '"' */
{false, 0, "\",0x5B,\"",    8, '['},    /* The character '[' */
{false, 0, "\",0x5D,\"",    8, ']'},    /* The character ']' */
};

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...);
void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...);
void compiler_error(char* source_file, int line, char* format, ...);
void compiler_error_core(char* source_file, int line, char* format, va_list format_list);
char* read_source_code(char* filename);
char* generate_nasm(char* source_file_name, char* source_code);
char** sepbyspc(char* src, int* dest_len);
void freewordarr(char** arr, int len);
char* parse_number(char* expression, char* filename, int line, int_var int_vars[], int len_int_vars);
char* parse_string(char* string, char* filename, int line);
bool parse_expression(char* expr, char* target, char** preserve, int preserve_len, FILE* nasm_output, char* filename, int line, int_var int_vars[], int len_int_vars);
void parse_if(char* expr, char* filename, int line, FILE* nasm_output, int_var int_vars[], int len_int_vars, int_stack* end_stack);
cmp_operation parse_comparison(char* expr, char* filename, int line, FILE* nasm_output, int_var int_vars[], int len_int_vars);

/* Classes for asserting something and exiting
 * if something is unwanted; compiler_error just
 * exits no matter what */

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...){
    if(eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(source_file, line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...){
    if(!eval)
        return;

    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(source_file, line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error(char* source_file, int line, char* format, ...){
    va_list format_params;

    va_start(format_params, format);
    compiler_error_core(source_file, line, format, format_params);
    va_end(format_params);

    exit(1);
}

void compiler_error_core(char* source_file, int line, char* format, va_list format_list){
    fprintf(stderr, "%sCompiler Error!\n", SHELL_RED);
    if(line == 0)
        fprintf(stderr, "%s: ", source_file);
    else
        fprintf(stderr, "%s:%d ", source_file, line);

    vfprintf(stderr, format, format_list);

    fprintf(stderr, SHELL_WHITE);
}

/* Read a file into dest, mallocs */
char* read_source_code(char* filename){
    int input_file = open(filename, O_RDONLY);
    compiler_error_on_true(input_file < 0, filename, 0, "Could not open file '%s'\n", filename);

    struct stat file_stat = {0};
    compiler_error_on_true((fstat(input_file, &file_stat)) < 0, filename, 0, "Could not stat file '%s'\n", filename);

    int input_size = file_stat.st_size;

    char* result = mmap(NULL, input_size, PROT_READ, MAP_PRIVATE, input_file, 0);
    compiler_error_on_false(result, filename, 0, "Failed to map file '%s'\n", filename);

    close(input_file);

    return result;
}

/* Generate nasm assembly code from the least source,
 * write it to a file and return the filename */
char* generate_nasm(char* source_file_name, char* source_code){
    char* filename;
    const char* appendix = ".asm";

    FILE* output_file;

    char** lines;
    int lines_len = 0;

    char* source_file_name_cpy;

    /* Stacks for handling variables, conditionals and loops */
    str_stack* strings = str_stack_create(STR_CAPACITY);

    int_stack* end_stack = int_stack_create(OP_CAPACITY);
    int_stack* while_stack = int_stack_create(OP_CAPACITY);
    int_stack* real_end_stack = int_stack_create(OP_CAPACITY);

    int_var int_vars[OP_CAPACITY] = {0};
    int int_var_acc = 0;

    int last_conditional = NO_CONDITIONAL;

    bool require_uprint = false;

    /* Convert X.least to X.asm */
    source_file_name_cpy = malloc((strlen(source_file_name) + 1) * sizeof(char));
    strcpy(source_file_name_cpy, source_file_name);

    strtok(source_file_name_cpy, ".");
    filename = malloc((strlen(source_file_name_cpy) + strlen(appendix) + 1) * sizeof(char));
    sprintf(filename, "%s%s", source_file_name_cpy, appendix);

    free(source_file_name_cpy);

    output_file = fopen(filename, "w");
    compiler_error_on_false(output_file, source_file_name, 0, "Could not open file '%s' for writing\n", filename);

    /* nasm header */
    fprintf(output_file,    ";; Generated by Least Complicated Compiler (lcc)\n"
                            "global _start\n"
                            "section .text\n"
                            "_start:\n");

    /* Count newlines */
    for(int i = 0; i < strlen(source_code); i++)
        if(source_code[i] == '\n')
            lines_len++;

    /* Split file by '\n' chars and load into lines
     *
     * We can't use pure strtok() because multiple '\n's would all be eliminated and
     * we want to count empty lines as well */
    lines = malloc(lines_len * sizeof(char*));
    char* offset = source_code;

    bool found_all = false;
    int accumulator = 0;
    while(!found_all){
        /* Exit if no more newlines are found */
        char* next_offset = index(offset, '\n');
        if(!next_offset){
            found_all = true;
            break;
        }
        /* If line is empty, set line to NULL and continue to next iteration */
        if(next_offset - offset <= 0){
            lines[accumulator++] = NULL;
            offset = next_offset + 1;
            continue;
        }

        /* Ignore spaces at beginning of line */
        char* space_offset = offset;
        for(; space_offset < next_offset; space_offset++){
            if(*space_offset != ' ')
                break;
        }

        lines[accumulator] = malloc(((int)(next_offset - space_offset) + 1) * sizeof(char));
        memcpy(lines[accumulator], space_offset, (int)(next_offset - space_offset));
        lines[accumulator][(int)(next_offset - space_offset)] = '\0';

        offset = next_offset + 1;
        accumulator++;
    }

    /* Parse operations
     * Things like print parse strings here and add them to 'strings' */
    for (int i = 0; i < lines_len; i++){
        /* Any empty line is NULL in the array */
        if(!lines[i])
            continue;

        /* HACK: error out on lines with spaces at the end to avoid segfaults when splitting them */
        compiler_error_on_true(lines[i][strlen(lines[i]) - 1] == ' ', source_file_name, i + 1, "Trailing spaces\n");

        char* instruction_op;
        int instruction_op_len = 0;
        char* line_cpy;
        int line_len = strlen(lines[i]);

        line_cpy = malloc((line_len + 1) * sizeof(char));
        strcpy(line_cpy, lines[i]);

        /* Get instruction by taking all characters up to the first SPC */
        instruction_op = strtok(line_cpy, " ");
        compiler_error_on_false(instruction_op, source_file_name, i + 1, "Could not read instruction\n");
        instruction_op_len = strlen(instruction_op) + 1;

        /* Print string to standard out */
        if(strcmp(instruction_op, "print") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);
            fprintf(output_file,    "\t;; print\n"
                                    "\tmov rax, 1\n"
                                    "\tmov rdi, 1\n"
                                    "\tmov rsi, str%d\n"
                                    "\tmov rdx, str%dLen\n"
                                    "\tsyscall\n", strings->counter, strings->counter);
            str_stack_push(strings, parse_string(rest_of_line, source_file_name, i));
        /* Print integer to standard out */
        }else if(strcmp(instruction_op, "uprint") == 0){
            require_uprint = true;

            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            rest_of_line = parse_number(rest_of_line, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; uprint\n"
                                    "\tmov rax, %s\n"
                                    "\tcall uprint\n", rest_of_line);
        /* Exit with specified exit code */
        }else if(strcmp(instruction_op, "fprint") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            char* parsed = parse_string(rest_of_line, source_file_name, i);

            int parsed_len = strlen(parsed);

            /* Count '[' and ']'
             * parsed[i] / 92 is 0 on '[' and 1 on ']', since they are the characters 91 and 93 */
            int counters[2] = {0};
            for(int i = 0; i < parsed_len; i++)
                counters[parsed[i] / 92] += parsed[i] == '[' || parsed[i] == ']';

            compiler_error_on_false(counters[0] == counters[1], source_file_name, i + 1, "Uneven numbers of '[' and ']' in format string\n");

            char* parsed_cpy = malloc((parsed_len + 1) * sizeof(char));
            strcpy(parsed_cpy, parsed);
            char* tmp_ptr = parsed_cpy;

            for(int j = 0; j < parsed_len; j++){
                if(parsed[j] == '['){
                    compiler_error_on_true(j == parsed_len - 1, source_file_name, i + 1, "Reached end of string after '['\n");
                    compiler_error_on_true(parsed[j+1] == ']', source_file_name, i + 1, "Empty format parameter\n");

                    require_uprint = true;

                    if(j > 0){
                        char* before = strtok(tmp_ptr, "[");
                        tmp_ptr = NULL;
                        compiler_error_on_false(before, source_file_name, i + 1, "String parsing error\n");
                        compiler_error_on_true(index(before, ']'), source_file_name, i + 1, "Unexpected ']' in format string\n");

                        int before_len = strlen(before);

                        char* new_string = malloc((before_len + 1) * sizeof(char));
                        strcpy(new_string, before);
                        fprintf(output_file,    "\t;; print\n"
                                                "\tmov rax, 1\n"
                                                "\tmov rdi, 1\n"
                                                "\tmov rsi, str%d\n"
                                                "\tmov rdx, str%dLen\n"
                                                "\tsyscall\n", strings->counter, strings->counter);

                        str_stack_push(strings, new_string);
                    }else{
                        /* Since we are the first character, we don't need to parse a 'before' the format */
                        tmp_ptr++;
                    }
                    char* inside = strtok(tmp_ptr, "]");
                    tmp_ptr = NULL;

                    compiler_error_on_false(inside, source_file_name, i + 1, "String parsing error\n");
                    compiler_error_on_true(index(inside, '['), source_file_name, i + 1, "Unexpected '[' in format string\n");

                    char* number = parse_number(inside, source_file_name, i + 1, int_vars, int_var_acc);
                    fprintf(output_file,    "\t;; uprint\n"
                                            "\tmov rax, %s\n"
                                            "\tcall uprint\n", number);
                }
            }
            /* If there are characters after the last '[' */
            char* seq = strtok(tmp_ptr, "\0");
            if(seq){
                int seq_len = strlen(seq);

                char* new_string = malloc((seq_len + 1) * sizeof(char));
                strcpy(new_string, seq);
                fprintf(output_file,    "\t;; print\n"
                                        "\tmov rax, 1\n"
                                        "\tmov rdi, 1\n"
                                        "\tmov rsi, str%d\n"
                                        "\tmov rdx, str%dLen\n"
                                        "\tsyscall\n", strings->counter, strings->counter);

                str_stack_push(strings, new_string);
            }

            free(parsed);
            free(parsed_cpy);
        }else if(strcmp(instruction_op, "exit") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            rest_of_line = parse_number(rest_of_line, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; exit\n"
                                    "\tmov rax, 60\n"
                                    "\tmov rdi, %s\n"
                                    "\tsyscall\n", rest_of_line);
        /* Conditional if statement */
        }else if(strcmp(instruction_op, "if") == 0){
            last_conditional = IF;

            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            parse_if(rest_of_line, source_file_name, i, output_file, int_vars, int_var_acc, end_stack);

            int_stack_push(real_end_stack, i);
        }else if(strcmp(instruction_op, "elif") == 0){
            compiler_error_on_false(last_conditional == IF || last_conditional == ELIF || last_conditional == NO_CONDITIONAL, source_file_name, i + 1, "'elif' must follow 'if' or 'elif'\n");

            last_conditional = ELIF;
            compiler_error_on_true(end_stack->counter <= 0, source_file_name, i + 1, "Unexpected 'elif'\n");

            fprintf(output_file,    "\t;; elif\n"
                                    "\tjmp .realend%d\n"
                                    "\t.end%d:\n", int_stack_top(real_end_stack), int_stack_pop(end_stack));

            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            parse_if(rest_of_line, source_file_name, i, output_file, int_vars, int_var_acc, end_stack);
        }else if(strcmp(instruction_op, "else") == 0){
            last_conditional = ELSE;
            compiler_error_on_true(end_stack->counter <= 0, source_file_name, i + 1, "Unexpected 'else'\n");

            fprintf(output_file,    "\t;; else\n"
                                    "\tjmp .realend%d\n"
                                    "\t.end%d:\n", int_stack_top(real_end_stack), int_stack_pop(end_stack));

            int_stack_push(end_stack, i);
        }else if(strcmp(instruction_op, "while") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            fprintf(output_file,    "\t;; while\n"
                                    "\t.whileentry%d:\n", i);

            cmp_operation operator = parse_comparison(rest_of_line, filename, i, output_file, int_vars, int_var_acc);

            fprintf(output_file,    "\t%s .while%d\n"
                                    "\tjmp .end%d\n"
                                    "\t.while%d:\n", operator.asm_name, i, i, i);
            int_stack_push(end_stack, i);
            int_stack_push(while_stack, i);
        /* End blocks */
        }else if(strcmp(instruction_op, "end") == 0){
            last_conditional = NO_CONDITIONAL;

            compiler_error_on_true(line_len > instruction_op_len, source_file_name, i + 1, "Expected end of line after 'end' instruction\n");
            compiler_error_on_true(end_stack->counter <= 0, source_file_name, i + 1, "Unexpected 'end'\n");
            int to_pop = int_stack_pop(end_stack);

            /* If this end ends a while loop: jump back to the top if we reached the line just before this end */
            if(while_stack->counter > 0){
                int possible_while_pop = int_stack_top(while_stack);
                if(to_pop == possible_while_pop){
                    fprintf(output_file,    "\t;; looping back\n"
                                            "\tjmp .whileentry%d\n", int_stack_pop(while_stack));
                }
            }

            fprintf(output_file,    "\t;; end\n"
                                    "\t.realend%d:\n"
                                    "\t.end%d:\n", int_stack_pop(real_end_stack), to_pop);
        /* Declare integer */
        }else if(strcmp(instruction_op, "int") == 0){
            compiler_error_on_true(instruction_op_len >= line_len, filename, i + 1, "No argument for assigning int\n");

            int expr_len;
            char* rest_of_line = strtok(NULL, "\0");
            compiler_error_on_false(rest_of_line, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            char** words = sepbyspc(rest_of_line, &expr_len);

            compiler_error_on_false(words, source_file_name, i, "Could not parse expression\n");
            compiler_error_on_true(expr_len != 2, source_file_name, i, "Did not find two words in expression '%s'\n", rest_of_line);
            int_var new_int;

            /* Parse string that comes after the instruction to number */
            for (int j = 0; j < strlen(words[0]); j++) {
                compiler_error_on_true(words[0][j] >= '0' && words[0][j] <= '9', filename, i + 1, "Character: '%c' is a digit\n", words[0][j]);
            }
            for(int j = 0; j < int_var_acc; j++){
                compiler_error_on_true(strcmp(words[0], int_vars[j].name) == 0, source_file_name, i + 1, "Redifinition of '%s'\n", words[0]);
            }
            new_int.name = malloc((strlen(words[0]) + 1) * sizeof(char));
            strcpy(new_int.name, words[0]);

            for(int j = 0; j < INT_LEN + 1 && j < strlen(words[1]); j++){
                compiler_error_on_true(words[1][j] < '0' || words[1][j] > '9', filename, i + 1, "Character: '%c' is not a digit\n", words[1][j]);
                compiler_error_on_true(j >= INT_LEN, filename, i + 1, "Number too long\n");
            }
            new_int.value = malloc((strlen(words[1]) + 1) * sizeof(char));
            strcpy(new_int.value, words[1]);

            new_int.mem_addr_ref = malloc((strlen(words[0]) + 1 + 2) * sizeof(char));
            sprintf(new_int.mem_addr_ref, "[%s]", words[0]);

            int_vars[int_var_acc++] = new_int;
            freewordarr(words, expr_len);
        /* Set value of integer */
        }else if(strcmp(instruction_op, "set") == 0){
            char* int_to_set = strtok(NULL, " ");
            compiler_error_on_false(int_to_set, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);
            char* expr = strtok(NULL, "\0");
            compiler_error_on_false(expr, source_file_name, i + 1, "Could not parse arguments to function '%s'\n", instruction_op);

            bool found = false;
            for (int i = 0; i < int_var_acc; i++) {
                if(strcmp(int_vars[i].name, int_to_set) == 0){
                    int_to_set = int_vars[i].mem_addr_ref;
                    found = true;
                }
            }
            compiler_error_on_false(found, source_file_name, i + 1, "'%s' is not a defined variable\n", int_to_set);

            parse_expression(expr, "rax", NULL, 0, output_file, source_file_name, i, int_vars, int_var_acc);
            fprintf(output_file,    "\t;; set\n"
                                    "\tmov %s, rax\n", int_to_set);
        }else
            compiler_error(source_file_name, i + 1, "Could not parse operation: '%s'\n", instruction_op);

        free(line_cpy);
    }

    compiler_error_on_true(end_stack->counter != 0, filename, 0, "Not all conditionals resolved\n");

    for(int i = 0; i < lines_len; i++)
        free(lines[i]);

    free(lines);

    /* If not otherwise specified, exit with code 0 */
    fprintf(output_file,    "\t;; exit\n"
                            "\tmov rax, 60\n"
                            "\tmov rdi, 0\n"
                            "\tsyscall\n");

    /* Subroutine for uprint instruction
     * Only printed if uprint is used */
    if(require_uprint){
        fprintf(output_file,"uprint:\n"
                            "\tenter 32,0\n"
                            "\tmov rdi, rbp\n" /* Start at base pointer */
                            "\tmov rcx, 10\n"
                            "\t.div:\n"
                            "\txor rdx, rdx\n" /* Reset to 0 */
                            "\tdiv rcx\n"
                            "\tor dl, '0'\n" /* Add char constant value to 8-bit end of rdx */
                            "\tdec rdi\n"
                            "\tmov [rdi], dl\n" /* Move character to start of string */
                            "\ttest rax, rax\n"
                            "\tjnz .div\n"

                            "\t.pr:\n"
                            "\tmov rsi, rdi\n" /* Points to beginning of string */
                            "\tmov rax, 1\n"
                            "\tmov rdi, 1\n"
                            "\tmov rdx, rbp\n"
                            "\tsub rdx, rsi\n" /* Calculate length of string */
                            "\tsyscall\n"
                            "\tleave\n"
                            "\tret\n");
    }

    fprintf(output_file,    "section .data\n");

    /* Parse strings into nasm strings in data section */
    while(strings->counter > 0){
        char* string = str_stack_pop(strings);
        fprintf(output_file, "\tstr%d: db \"%s\"\n"
               "\tstr%dLen: equ $ - str%d\n",
               strings->counter, string, strings->counter, strings->counter);
        free(string);
    }
    str_stack_free(strings);

    int_stack_free(end_stack);
    int_stack_free(while_stack);
    int_stack_free(real_end_stack);

    /* Parse ints into constants */
    for(int i = 0; i < int_var_acc; i++){
        fprintf(output_file, "\t%s: dq %s\n", int_vars[i].name, int_vars[i].value);
        free(int_vars[i].name);
        free(int_vars[i].value);
        free(int_vars[i].mem_addr_ref);
    }

    fclose(output_file);

    return filename;
}

/* Unite an array of strings into a single string with the elements separated by spaces */
char* unite(char** src, int off, int len){
    char* out;
    int out_len = 0;

    for(int i = off; i < len + off; i++)
        out_len += strlen(src[i]);

    /* Account for spaces */
    out_len += len - 1;

    out = malloc((out_len + 1) * sizeof(char));
    memset(out, '\0', out_len + 1);

    for(int i = off; i < len + off - 1; i++){
        strcat(out, src[i]);
        strcat(out, " ");
    }
    strcat(out, src[len + off - 1]);

    return out;
}

/* Separate src by spaces (gobble up duplicate spaces) into array of strings */
char** sepbyspc(char* src, int* dest_len){
    if(!src)
        return NULL;

    int src_len = strlen(src);

    if(src_len == 0)
        return NULL;

    char* src_cpy = malloc((src_len + 1) * sizeof(char));
    strcpy(src_cpy, src);

    char** dest;
    *dest_len = 0;

    for(int i = 0, repeat = 0; i < src_len; i++){
        if(src_cpy[i] == ' ' && !repeat){
            *dest_len += 1;
            repeat = 1;
        }else if(!(src_cpy[i] == ' ')){
            repeat = 0;
        }
    }
    *dest_len += 1;

    dest = malloc((*dest_len + 1) * sizeof(char*));

    char* src_word;
    char* src_copy_ptr = src_cpy;
    /* control_copy_ptr becomes NULL after first iteration since every call to strtok after the first one
     * which is supposed to operate on the same string has to be with NULL as str. */
    for(int i = 0; i < *dest_len; src_copy_ptr = NULL, i++){
        src_word = strtok(src_copy_ptr, " ");
        int src_word_len = strlen(src_word);

        dest[i] = malloc((src_word_len + 1) * sizeof(char));
        dest[i][src_word_len] = '\0';
        strcpy(dest[i], src_word);
    }

    free(src_cpy);

    return dest;
}

/* Free array generated by, for example, sepbyspc where the array itself is also malloc'd */
void freewordarr(char** arr, int len){
    for(int i = 0; i < len; i++)
        free(arr[i]);

    free(arr);
}

/* Check if expression is either a valid number or a variable (return nasm memory reference if so) */
char* parse_number(char* expression, char* filename, int line, int_var int_vars[], int len_int_vars){
    for (int i = 0; i < len_int_vars; i++) {
        if(strcmp(int_vars[i].name, expression) == 0)
            return int_vars[i].mem_addr_ref;
    }
    for(int i = 0; i < INT_LEN + 1 && i < strlen(expression); i++){
        compiler_error_on_true(expression[i] < '0' || expression[i] > '9', filename, line + 1,
                                "Unknown variable or number constant: '%s'\n", expression);
        compiler_error_on_true(i >= INT_LEN, filename, line + 1, "Number too long\n");
    }

    return expression;
}

/* Check validity of string and insert escape sequences */
char* parse_string(char* string, char* filename, int line){
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
                    compiler_error_on_false((j+1) < string_len, filename, line + 1, "Reached end of line while parsing first '\"' in string\n");

                acc++;
                compiler_error_on_true(acc > 2, filename, line + 1, "Found more than two '\"' while parsing string\n");
                if(acc == 2)
                    compiler_error_on_false((j+1) == string_len, filename, line + 1, "Expected end of line after string\n");
                break;
            case '\\':
                j++;
                compiler_error_on_true(j >= string_len - 1, filename, line + 1, "Reached end of line while trying to parse escape sequence\n");
                bool found = false;
                for(int i = 0; i < STR_TOKEN_END; i++){
                    if(string[j] == token_structs[i].expandeur){
                        tokens[tokens_len++] = token_structs[i];
                        found = true;
                    }
                }
                compiler_error_on_false(found, filename, line + 1, "Failed to parse escape sequence '\\%c'\n", string[j]);
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
        compiler_error_on_true(string[j] != '"' && acc == 0, filename, line + 1,
                                "First '\"' not at beginning of parsed sequence, instead found '%c'\n", string[j]);
    }
    compiler_error_on_false(acc == 2, filename, line + 1, "Did not find two '\"' while parsing string\n");

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

/* Convert arithmetic expression into assembly and print to nasm_output
 * Result is moved into target register */
bool parse_expression(char* expr, char* target, char** preserve, int preserve_len, FILE* nasm_output, char* filename, int line, int_var int_vars[], int len_int_vars){
    arit_operation operator;
    int expr_len = 0;
    char** expr_arr = sepbyspc(expr, &expr_len);

    if(preserve_len != 0){
        for(int i = 0; i < preserve_len; i++)
            fprintf(nasm_output,    "\t;; preserving %s\n"
                                    "\tpush %s\n", preserve[i], preserve[i]);
    }

    compiler_error_on_false(expr_arr, filename, line + 1, "Could not parse expression\n");
    switch(expr_len){
        case 1:
        {
            char* number = parse_number(expr_arr[0], filename, line, int_vars, len_int_vars);
            fprintf(nasm_output, "\tmov %s, %s\n", target, number);
            freewordarr(expr_arr, expr_len);
            break;
        }
        case 3:
        {
            bool found_operator = false;
            for(int j = 0; j < ARIT_OPERATION_ENUM_END; j++){
                if(strcmp(expr_arr[1], arit_operation_structs[j].source_name) == 0){
                    operator = arit_operation_structs[j];
                    found_operator = true;
                }
            }
            compiler_error_on_false(found_operator, filename, line + 1, "Could not parse operation '%s'\n", expr_arr[1]);

            char* numbers[3] = {0};

            /* Check if numbers are valid */
            for (int j = 0; j < 3; j++) {
                if(j == 1)
                    continue;

                numbers[j] = parse_number(expr_arr[j], filename, line, int_vars, len_int_vars);
            }

            switch(operator.op_enum){
                case DIV:
                case MUL:
                case MOD:
                    fprintf(nasm_output,    "\t;; %s\n"
                                            "\tmov rdx, 0\n"
                                            "\tmov rax, %s\n"
                                            "\tmov rcx, %s\n"
                                            "\t%s rcx\n", operator.source_name, numbers[0], numbers[2], operator.asm_name);
                    /* If taking modulus: get remainder instead of quotient */
                    if(operator.op_enum == MOD && (strcmp(target, "rdx") != 0))
                        fprintf(nasm_output,"\tmov rax, rdx\n");
                    else if(strcmp(target, "rax") != 0)
                        fprintf(nasm_output,"\tmov %s, rax\n", target);
                    break;
                default:
                    fprintf(nasm_output,    "\t;; %s\n"
                                            "\tmov rax, %s\n"
                                            "\tmov rbx, %s\n"
                                            "\t%s rax, rbx\n", operator.source_name, numbers[0], numbers[2], operator.asm_name);
                    if(strcmp(target, "rax") != 0)
                        fprintf(nasm_output,"\tmov %s, rax\n", target);
                    break;
            }
            freewordarr(expr_arr, expr_len);
            break;
        }
        default:
            compiler_error(filename, line + 1, "Unexpected number of elements in expression '%s'\n", expr);
    }

    if(preserve_len != 0){
        for(int i = preserve_len - 1; i >= 0; i--)
            fprintf(nasm_output, "\tpop %s\n", preserve[i]);
    }

    return true;
}

void parse_if(char* expr, char* filename, int line, FILE* nasm_output, int_var int_vars[], int len_int_vars, int_stack* end_stack){
    int_stack* indices = int_stack_create(OP_CAPACITY);
    str_stack* logicals = str_stack_create(STR_CAPACITY);

    int words_len = 0;
    char** words = sepbyspc(expr, &words_len);

    /* Count logical operators */
    for(int i = 0; i < words_len; i++){
        for(int j = 0; j < LOGICAL_OPS_END; j++){
            if(strcmp(words[i], logical_operation_structs[j].source_name) == 0){
                int_stack_push(indices, i);
                str_stack_push(logicals, logical_operation_structs[j].asm_name);
            }
        }
    }

    // Concatenate comparisons into nasm code with logical operators governing when we goto .ifX
    if(indices->counter > 0){
        int comparison_len = indices->counter + 1;
        char* comparisons[comparison_len];

        int off = 0;

        for(int i = 0, acc = 0; i < words_len; i++){
            if(i == int_stack_bottom(indices)){
                int len = int_stack_pull(indices);
                comparisons[acc++] = unite(words, off, len - off);
                off = len + 1;
            }
        }
        comparisons[comparison_len - 1] = unite(words, off, words_len - off);

        for(int i = 0; i < comparison_len; i++){
            char* logical = str_stack_pull(logicals);
            cmp_operation operator = parse_comparison(comparisons[i], filename, line, nasm_output, int_vars, len_int_vars);
            if(logical){
                fprintf(nasm_output,    "\t;; %s\n", logical);
                if(strcmp(logical, "and") == 0){
                    fprintf(nasm_output,    "\t%s .%s%d%d\n"
                                            "\tjmp .end%d\n"
                                            "\t.%s%d%d:\n", operator.asm_name, logical, line, i, line, logical, line, i);
                }else if(strcmp(logical, "or") == 0){
                    fprintf(nasm_output,    "\t%s .if%d\n"
                                            "\tjmp .%s%d%d\n"
                                            "\t.%s%d%d:\n", operator.asm_name, line, logical, line, i, logical, line, i);
                }
            }else{
                fprintf(nasm_output,    "\t;; if\n"
                                        "\t%s .if%d\n"
                                        "\tjmp .end%d\n"
                                        "\t.if%d:\n", operator.asm_name, line, line, line);
            }
        }

        for(int i = 0; i < comparison_len; i++)
            free(comparisons[i]);
    }else{
        cmp_operation operator = parse_comparison(expr, filename, line, nasm_output, int_vars, len_int_vars);
        fprintf(nasm_output,    "\t;; if\n"
                                "\t%s .if%d\n"
                                "\tjmp .end%d\n"
                                "\t.if%d:\n", operator.asm_name, line, line, line);
    }

    int_stack_push(end_stack, line);

    int_stack_free(indices);
    str_stack_free(logicals);
    freewordarr(words, words_len);
}

cmp_operation parse_comparison(char* expr, char* filename, int line, FILE* nasm_output, int_var int_vars[], int len_int_vars){
    cmp_operation operator;

    int words_len = 0;
    char** words = sepbyspc(expr, &words_len);

    /* If only one number: if it's one: true, else: false */
    if(words_len == 1){
        char* number = parse_number(words[0], filename, line, int_vars, len_int_vars);
        fprintf(nasm_output,    "\t;; checking if number is one\n"
                                "\tmov rax, %s\n"
                                "\tcmp rax, 1\n", number);
        freewordarr(words, words_len);
        return cmp_operation_structs[EQUAL];
    }

    compiler_error_on_false(words, filename, line + 1, "Could not parse expression\n");

    bool found_operator = false;
    int operator_index = 0;
    for(int j = 0; j < words_len; j++){
        for(int k = 0; k < CMP_OPERATION_ENUM_END; k++){
            if(strcmp(words[j], cmp_operation_structs[k].source_name) == 0){
                compiler_error_on_true(found_operator, filename, line + 1, "Found more than one operator in expression in expression '%s'\n", expr);
                operator = cmp_operation_structs[k];
                operator_index = j;
                found_operator = true;
            }
        }
    }
    compiler_error_on_false(found_operator, filename, line, "Could not parse operation '%s'\n", words[1]);
    compiler_error_on_true(operator_index == words_len, filename, line, "Operator is last operand in operation '%s'\n", words[1]);

    /* Join the two sides of the operation, pass them to the expression parser and then compare */
    char* sides[2] = {0};
    sides[0] = unite(words, 0, operator_index);
    sides[1] = unite(words, operator_index + 1, words_len - operator_index - 1);

    parse_expression(sides[0], "rax", NULL, 0, nasm_output, filename, line, int_vars, len_int_vars);
    char* preserve[1] = {"rax"};
    parse_expression(sides[1], "rbx", preserve, 1, nasm_output, filename, line, int_vars, len_int_vars);

    fprintf(nasm_output,    "\t;; %s\n"
                            "\tcmp rax, rbx\n", operator.source_name);

    freewordarr(words, words_len);
    for (int j = 0; j < 2; j++)
        free(sides[j]);

    return operator;
}

int main(int argc, char *argv[]) {
    bool run_after_compile = false;
    /* Handle command line input with getopt */
    int flag;
    while ((flag = getopt(argc, argv, "r")) != -1){
        switch (flag){
        case 'r':
            run_after_compile = true;
            break;
        case '?':
        default:
            return 1;
        }
    }

    const char* nasm_cmd_base = "nasm -g -felf64 -o ";

    const char* ld_cmd_base = "ld -o ";

    compiler_error_on_false(argc >= 2, "Initialization", 0, "No input file provided\n");

    printf("[INFO] Input file: %s%s%s\n", SHELL_GREEN, argv[argc - 1], SHELL_WHITE);
    char* input_source = read_source_code(argv[argc - 1]);

    char* nasm_filename = generate_nasm(argv[argc - 1], input_source);
    printf("[INFO] Generating nasm source to %s%s%s\n", SHELL_GREEN, nasm_filename, SHELL_WHITE);

    char* source_filename_cpy = malloc((strlen(argv[argc - 1]) + 1) * sizeof(char));
    strcpy(source_filename_cpy, argv[argc - 1]);

    strtok(source_filename_cpy, ".");

    char* object_filename = malloc((strlen(source_filename_cpy) + strlen(".o") + 1) * sizeof(char));
    sprintf(object_filename, "%s%s", source_filename_cpy, ".o");

    printf("[CMD] %s%s%s%s %s%s%s\n", nasm_cmd_base,
           SHELL_RED, object_filename, SHELL_WHITE,
           SHELL_GREEN, nasm_filename, SHELL_WHITE);

    if(fork() == 0){
        execlp("nasm", "nasm", "-g", "-felf64", "-o", object_filename, nasm_filename, NULL);
    }else{
        wait(NULL);
    }

    printf("[CMD] %s%s%s%s %s%s%s\n", ld_cmd_base,
           SHELL_RED, source_filename_cpy, SHELL_WHITE,
           SHELL_GREEN, object_filename, SHELL_WHITE);

    if(fork() == 0){
        execlp("ld", "ld", "-o", source_filename_cpy, object_filename, NULL);
    }else{
        wait(NULL);
    }

    if(run_after_compile){
        const char* execute_cmd_base = "./";
        char* execute_local_file = malloc(
            (strlen(execute_cmd_base) + strlen(source_filename_cpy) + 1)
            * sizeof(char));
        sprintf(execute_local_file, "%s%s", execute_cmd_base, source_filename_cpy);

        printf("[CMD] %s%s%s%s\n", execute_cmd_base,
            SHELL_GREEN, source_filename_cpy, SHELL_WHITE);
        if(fork() == 0){
            execlp(execute_local_file, execute_local_file, NULL);
        }else{
            wait(NULL);
        }

        free(execute_local_file);
    }

    free(object_filename);
    free(nasm_filename);
    free(source_filename_cpy);

    return 0;
}
