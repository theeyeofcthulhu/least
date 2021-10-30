#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#define OP_CAPACITY 1000
#define STR_CAPACITY 1000

#define INT_LEN 8

#define SHELL_GREEN "\033[0;32m"
#define SHELL_RED   "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

//TODO: comment everything

// Structure for organizing comparison operations
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

// Structure for organizing arithmetic operations
enum arit_operations{
ADD,
SUB,
MOD,
DIV,
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
{MOD,   "%",    ""},
{DIV,   "/",    ""},
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
STR_TOKEN_END,
};

const str_token token_structs[STR_TOKEN_END] = {
{false, 0, "\",0xa,\"",     7, 'n'},    // Newline
{false, 0, "    ",          4, 't'},    // Tabstop
{false, 0, "\\",            1, '\\'},   // The character '\'
{false, 0, "\",0x22,\"",    8, '\"'},   // The character '"'
};

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...);
void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...);
void compiler_error(char* source_file, int line, char* format, ...);
char* read_source_code(char* filename);
char* generate_nasm(char* source_file_name, char* source_code);
char** sepbyspc(char* src, int* dest_len);
void freewordarr(char** arr, int len);
char* parse_number(char* expression, char* filename, int line, int_var int_vars[], int len_int_vars);
char* parse_string(char* string, char* filename, int line);
bool parse_expression(char* expr, char* target, FILE* nasm_output, char* filename, int line, int_var int_vars[], int len_int_vars);

/* Classes for asserting something and exiting
 * if something is unwanted; compiler_error just
 * exits no matter what */

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...){
    if(eval)
        return;

    va_list format_params;

    printf("%sCompiler Error!\n", SHELL_RED);
    if(line == 0)
        printf("%s: ", source_file);
    else
        printf("%s:%d ", source_file, line);

    va_start(format_params, format);
    vprintf(format, format_params);
    va_end(format_params);

    printf(SHELL_WHITE);

    exit(1);
}

void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...){
    if(!eval)
        return;

    va_list format_params;

    printf("%sCompiler Error!\n", SHELL_RED);
    if(line == 0)
        printf("%s: ", source_file);
    else
        printf("%s:%d ", source_file, line);

    va_start(format_params, format);
    vprintf(format, format_params);
    va_end(format_params);

    printf(SHELL_WHITE);

    exit(1);
}

void compiler_error(char* source_file, int line, char* format, ...){
    va_list format_params;

    printf("%sCompiler Error!\n", SHELL_RED);
    if(line == 0)
        printf("%s: ", source_file);
    else
        printf("%s:%d ", source_file, line);

    va_start(format_params, format);
    vprintf(format, format_params);
    va_end(format_params);

    printf(SHELL_WHITE);

    exit(1);
}

/* Read a file into dest, mallocs */
char* read_source_code(char* filename){
    FILE* input_file;
    int input_size;
    char* result;

    input_file = fopen(filename, "r");
    compiler_error_on_true(input_file == NULL, filename, 0, "Could not open file '%s'\n", filename);

    fseek(input_file, 0, SEEK_END);
    input_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);

    result = malloc(input_size + 1);
    fread(result, 1, input_size, input_file);
    result[input_size] = '\0';

    fclose(input_file);

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

    // Stacks for handling variables, conditionals and loops
    char* strings[STR_CAPACITY] = {0};
    int strings_len = 0;

    int end_stack[OP_CAPACITY] = {0};
    int end_stack_acc = 0;

    int_var int_vars[OP_CAPACITY] = {0};
    int int_var_acc = 0;

    bool require_uprint = false;

    source_file_name_cpy = malloc((strlen(source_file_name) + 1) * sizeof(char));
    strcpy(source_file_name_cpy, source_file_name);

    strtok(source_file_name_cpy, ".");
    filename = malloc((strlen(source_file_name_cpy) + strlen(appendix) + 1) * sizeof(char));
    sprintf(filename, "%s%s", source_file_name_cpy, appendix);

    free(source_file_name_cpy);

    output_file = fopen(filename, "w");
    compiler_error_on_true(output_file == NULL, source_file_name, 0, "Could not open file '%s' for writing\n", filename);

    fprintf(output_file, "global _start\n");
    fprintf(output_file, "section .text\n");

    fprintf(output_file, "_start:\n");

    for(int i = 0; i < strlen(source_code); i++)
        if(source_code[i] == '\n')
            lines_len++;

    // Split file by '\n' chars and load into lines
    lines = malloc(lines_len * sizeof(char*));
    char* offset = source_code;

    bool found_all = false;
    int accumulator = 0;
    while(!found_all){
        char* next_offset = index(offset, '\n');
        if(!next_offset){
            found_all = true;
            break;
        }
        if(next_offset - offset <= 0){
            lines[accumulator++] = NULL;
            offset = next_offset + 1;
            continue;
        }

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

    // Parse operations
    // Things like print parse strings here and add them to 'strings'
    for (int i = 0; i < lines_len; i++){
        if(!lines[i])
            continue;

        char* instruction_op;
        int instruction_op_len = 0;
        char* line_cpy;
        int line_len = strlen(lines[i]);

        line_cpy = malloc((line_len + 1) * sizeof(char));
        strcpy(line_cpy, lines[i]);

        // Get instruction by taking all characters up to the first SPC
        instruction_op = strtok(line_cpy, " ");
        compiler_error_on_true(instruction_op == NULL, source_file_name, i + 1, "Could not read instruction\n");
        instruction_op_len = strlen(instruction_op) + 1;

        // Print string to standard out
        if(strcmp(instruction_op, "print") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            strings[strings_len] = parse_string(rest_of_line, source_file_name, i);
            fprintf(output_file,    "\t;; print\n"
                                    "\tmov rax, 1\n"
                                    "\tmov rdi, 1\n"
                                    "\tmov rsi, str%d\n"
                                    "\tmov rdx, str%dLen\n"
                                    "\tsyscall\n", strings_len, strings_len);
            strings_len++;
        // Print integer to standard out
        }else if(strcmp(instruction_op, "uprint") == 0){
            require_uprint = true;

            char* rest_of_line = strtok(NULL, "\0");
            rest_of_line = parse_number(rest_of_line, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; uprint\n"
                                    "\tmov rax, %s\n"
                                    "\tcall uprint\n", rest_of_line);
        // Exit with specified exit code
        }else if(strcmp(instruction_op, "exit") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            rest_of_line = parse_number(rest_of_line, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; exit\n"
                                    "\tmov rax, 60\n"
                                    "\tmov rdi, %s\n"
                                    "\tsyscall\n", rest_of_line);
        // Conditional if statement
        }else if(strcmp(instruction_op, "if") == 0){
            cmp_operation operator;

            char* rest_of_line = strtok(NULL, "\0");
            int words_len = 0;
            char** words = sepbyspc(rest_of_line, &words_len);

            compiler_error_on_false(words, filename, i + 1, "Could not parse expression\n");

            bool found_operator = false;
            int operator_index = 0;
            for(int j = 0; j < words_len; j++){
                for(int k = 0; k < CMP_OPERATION_ENUM_END; k++){
                    if(strcmp(words[j], cmp_operation_structs[k].source_name) == 0){
                        compiler_error_on_true(found_operator, filename, i + 1, "Found more than one operator in expression in expression '%s'\n", rest_of_line);
                        operator = cmp_operation_structs[k];
                        operator_index = j;
                        found_operator = true;
                    }
                }
            }
            compiler_error_on_false(found_operator, source_file_name, i, "Could not parse operation '%s'\n", words[1]);
            compiler_error_on_true(operator_index == words_len, source_file_name, i, "Operator is last operand in operation '%s'\n", words[1]);

            // Join the two sides of the operation, pass them to the expression parser and then compare
            char* sides[2] = {0};
            int sizes[2] = {0};
            bool init[2] = {0};
            for (int j = 0; j < words_len; j++) {
                if(j == operator_index)
                    continue;
                int addressing = j < operator_index ? 0 : 1;
                sizes[addressing] += strlen(words[j]) + 1;
            }
            for (int j = 0; j < 2; j++)
                sides[j] = malloc((sizes[j]) * sizeof(char*));
            for (int j = 0; j < words_len; j++) {
                if(j == operator_index)
                    continue;
                int addressing = j < operator_index ? 0 : 1;
                if(init[addressing]){
                    char* copy = malloc((strlen(sides[addressing]) + 1) * sizeof(char));
                    strcpy(copy, sides[addressing]);
                    sprintf(sides[addressing], "%s%s ", copy, words[j]);
                    free(copy);
                }else{
                    sprintf(sides[addressing], "%s ", words[j]);
                    init[addressing] = true;
                }
            }
            for (int j = 0; j < 2; j++)
                sides[j][sizes[j] - 1] = '\0';
            parse_expression(sides[0], "eax", output_file, source_file_name, i, int_vars, int_var_acc);
            parse_expression(sides[1], "ebx", output_file, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; if\n"
                                    "\tcmp eax, ebx\n"
                                    "\t%s .if%d\n"
                                    "\tjmp .end%d\n"
                                    "\t.if%d:\n", operator.asm_name, i, i, i);
            end_stack[end_stack_acc++] = i;

            freewordarr(words, words_len);
        // End blocks
        }else if(strcmp(instruction_op, "end") == 0){
            compiler_error_on_true(line_len > instruction_op_len, source_file_name, i + 1, "Expected end of line after 'end' instruction\n");
            compiler_error_on_true(end_stack_acc <= 0, source_file_name, i + 1, "Unexpected 'end'\n");

            fprintf(output_file,    "\t;; end\n"
                                    "\t.end%d:\n", end_stack[--end_stack_acc]);
        // Declare integer
        }else if(strcmp(instruction_op, "int") == 0){
            compiler_error_on_true(instruction_op_len >= line_len, filename, i + 1, "No argument for assigning int\n");

            int expr_len;
            char* rest_of_line = strtok(NULL, "\0");
            char** words = sepbyspc(rest_of_line, &expr_len);

            compiler_error_on_false(words, source_file_name, i, "Could not parse expression\n");
            compiler_error_on_true(expr_len != 2, source_file_name, i, "Did not find two words in expression '%s'\n", rest_of_line);
            int_var new_int;

            // Parse string that comes after the instruction to number
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
        // Set value of integer
        }else if(strcmp(instruction_op, "set") == 0){
            char* int_to_set = strtok(NULL, " ");
            char* expr = strtok(NULL, "\0");

            bool found = false;
            for (int i = 0; i < int_var_acc; i++) {
                if(strcmp(int_vars[i].name, int_to_set) == 0){
                    int_to_set = int_vars[i].mem_addr_ref;
                    found = true;
                }
            }
            compiler_error_on_false(found, source_file_name, i + 1, "'%s' is not a defined variable\n", int_to_set);

            parse_expression(expr, "eax", output_file, source_file_name, i, int_vars, int_var_acc);
            fprintf(output_file,    "\t;; set\n"
                                "\tmov %s, eax\n", int_to_set);
        }else
            compiler_error(source_file_name, i + 1, "Could not parse operation: '%s'\n", instruction_op);

        free(line_cpy);
    }

    compiler_error_on_true(end_stack_acc != 0, filename, 0, "Not all conditionals resolved\n");

    for(int i = 0; i < lines_len; i++)
        free(lines[i]);

    free(lines);

    // If not otherwise specified, exit with code 0
    fprintf(output_file,    "\t;; exit\n"
                            "\tmov rax, 60\n"
                            "\tmov rdi, 0\n"
                            "\tsyscall\n");

    // Subroutine for uprint instruction
    // Only printed if uprint is used
    if(require_uprint){
        fprintf(output_file,";; divide rax by 10\n"
                            "divten:\n"
                            "\tmov rdx, 0\n"
                            "\tmov rcx, 10\n"
                            "\tdiv rcx\n"
                            "\tret\n");
        fprintf(output_file,";; print char in rsi onto screen\n"
                            "putchar:\n"
                            "\tmov rax, 1\n"
                            "\tmov rdi, 1\n"
                            "\tmov rdx, 1\n"
                            "\tsyscall\n"
                            "\tret\n");
        fprintf(output_file,"uprint:\n"
                            "\tmov r8, 0\n"
                            "\t.div:\n"
                            "\tcall divten\n"
                            "\tadd rdx, 0x30\n"
                            "\tpush rdx\n"
                            "\tinc r8\n"
                            "\tcmp rax, 0\n"
                            "\tjne .div\n"

                            "\t.pr:\n"
                            "\tmov rsi, rsp\n"
                            "\tcall putchar\n"
                            "\tpop rax\n"
                            "\tdec r8\n"
                            "\tjne .pr\n"
                            "\tret\n");
    }

    fprintf(output_file,    "section .data\n");

    // Parse strings into nasm strings in data section
    for(int i = 0; i < strings_len; i++){
        if(!strings[i])
            continue;
        fprintf(output_file, "\tstr%d: db \"%s\"\n"
               "\tstr%dLen: equ $ - str%d\n",
               i, strings[i], i, i);
        free(strings[i]);
    }

    // Parse ints into constants
    for(int i = 0; i < int_var_acc; i++){
        fprintf(output_file, "\t%s: dq %s\n", int_vars[i].name, int_vars[i].value);
        free(int_vars[i].name);
        free(int_vars[i].value);
        free(int_vars[i].mem_addr_ref);
    }

    fclose(output_file);

    return filename;
}

// Separate src by spaces (gobble up duplicate spaces) into array of strings
char** sepbyspc(char* src, int* dest_len){
    //rest  of line
    int src_len = strlen(src);
    if(!(src_len > 0))
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

    // Split file by '\n' chars and load into lines
    dest = malloc((*dest_len + 1) * sizeof(char*));

    char* src_word;
    char* src_copy_ptr = src_cpy;
    // control_copy_ptr becomes NULL after first iteration since every call to strtok after the first one
    // has to be with NULL as str.
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

// Free array generated by, for example, sepbyspc
void freewordarr(char** arr, int len){
    for(int i = 0; i < len; i++)
        free(arr[i]);

    free(arr);
}

// Check if expression is either a valid number or a variable (return nasm memory reference if so)
char* parse_number(char* expression, char* filename, int line, int_var int_vars[], int len_int_vars){
    for (int i = 0; i < len_int_vars; i++) {
        if(strcmp(int_vars[i].name, expression) == 0)
            return int_vars[i].mem_addr_ref;
    }
    for(int i = 0; i < INT_LEN + 1 && i < strlen(expression); i++){
        compiler_error_on_true(expression[i] < '0' || expression[i] > '9', filename, line + 1,
                                "Unknown word: '%s'\n", expression);
        compiler_error_on_true(i >= INT_LEN, filename, line + 1, "Number too long\n");
    }

    return expression;
}

// Check validity of string and insert escape sequences
char* parse_string(char* string, char* filename, int line){
    char* result;
    int result_len = 0;

    int string_len = strlen(string);

    int acc = 0;

    str_token tokens[string_len];
    int tokens_len = 0;

    // Parse string that comes after the instruction
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
    result[result_len] = '\0';
    for (int i = 0, j = 0; i < tokens_len; i++) {
        if(tokens[i].is_char)
            result[j] = tokens[i].default_char;
        else
            memcpy(result + j, tokens[i].expression, tokens[i].expr_len);
        j += tokens[i].expr_len;
    }

    return result;
}

// Convert arithmetic expression into assembly and print to nasm_output
// Result is moved into target register
bool parse_expression(char* expr, char* target, FILE* nasm_output, char* filename, int line, int_var int_vars[], int len_int_vars){
    arit_operation operator;
    int expr_len = 0;
    char** expr_arr = sepbyspc(expr, &expr_len);

    compiler_error_on_false(expr_arr, filename, line + 1, "Could not parse expression\n");
    switch(expr_len){
        case 1:
        {
            char* number = parse_number(expr_arr[0], filename, line, int_vars, len_int_vars);
            fprintf(nasm_output, "\tmov %s, %s\n", target, number);
            freewordarr(expr_arr, expr_len);
            return true;
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

            // Check if numbers are valid
            for (int j = 0; j < 3; j++) {
                if(j == 1)
                    continue;

                numbers[j] = parse_number(expr_arr[j], filename, line, int_vars, len_int_vars);
            }

            if(operator.op_enum == MOD || operator.op_enum == DIV){
                fprintf(nasm_output,    "\t;; mod\n"
                                        "\tmov edx, 0\n"
                                        "\tmov eax, %s\n"
                                        "\tmov ecx, %s\n"
                                        "\tdiv ecx\n", numbers[0], numbers[2]);
                // If taking modulus: get remainder instead of quotient
                if(operator.op_enum == MOD && (strcmp(target, "edx") != 0))
                    fprintf(nasm_output,    "\tmov eax, edx\n");
                else if(strcmp(target, "eax") != 0)
                    fprintf(nasm_output,"\tmov %s, eax\n", target);
            }else{
                fprintf(nasm_output,    "\t;; %s\n"
                                        "\tmov eax, %s\n"
                                        "\tmov ebx, %s\n"
                                        "\t%s eax, ebx\n", operator.asm_name, numbers[0], numbers[2], operator.asm_name);
                if(strcmp(target, "eax") != 0)
                    fprintf(nasm_output,"\tmov %s, eax\n", target);
            }
            freewordarr(expr_arr, expr_len);
            return true;
        }
        default:
            compiler_error(filename, line + 1, "Unexpected number of elements in expression '%s'\n", expr);
    }

    return true;
}

int main(int argc, char *argv[]) {
    bool run_after_compile = false;
	// Handle command line input with getopt
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

    const char* nasm_cmd_base = "nasm -felf64 -o ";
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

    char* nasm_cmd = malloc((strlen(nasm_cmd_base) + strlen(object_filename) + 1 + strlen(nasm_filename) + 1) * sizeof(char));
    sprintf(nasm_cmd, "%s%s %s", nasm_cmd_base, object_filename, nasm_filename);

    printf("[CMD] %s%s%s%s %s%s%s\n", nasm_cmd_base,
           SHELL_RED, object_filename, SHELL_WHITE,
           SHELL_GREEN, nasm_filename, SHELL_WHITE);
    system(nasm_cmd);
    free(nasm_cmd);

    char* ld_cmd = malloc((strlen(ld_cmd_base) + strlen(source_filename_cpy) + 1 + strlen(object_filename) + 1) * sizeof(char));
    sprintf(ld_cmd, "%s%s %s", ld_cmd_base, source_filename_cpy, object_filename);

    printf("[CMD] %s%s%s%s %s%s%s\n", ld_cmd_base,

           SHELL_RED, source_filename_cpy, SHELL_WHITE,
           SHELL_GREEN, object_filename, SHELL_WHITE);
    system(ld_cmd);
    free(ld_cmd);

    if(run_after_compile){
        const char* execute_cmd_base = "./";
        char* execute_cmd = malloc((strlen(execute_cmd_base) + strlen(source_filename_cpy) + 1) * sizeof(char));
        sprintf(execute_cmd, "%s%s", execute_cmd_base, source_filename_cpy);

        printf("[CMD] %s%s%s%s\n", execute_cmd_base,
            SHELL_GREEN, source_filename_cpy, SHELL_WHITE);
        system(execute_cmd);
        free(execute_cmd);
    }

    free(input_source);
    free(object_filename);
    free(nasm_filename);
    free(source_filename_cpy);

    return 0;
}
