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

enum cmp_operations{
EQUAL,
NOT_EQUAL,
LESS,
LESS_OR_EQ,
GREATER,
GREATER_OR_EQ,
OPERATION_ENUM_END,
};

typedef struct{
    char* source_name;
    char* asm_name;
}operation;

const operation operation_structs[OPERATION_ENUM_END] = {
{"==",   "je"},
{"!=",   "jne"},
{"<",    "jl"},
{"<=",   "jle"},
{">",    "jg"},
{">=",   "jge"},
};

typedef struct{
    char* name;
    char* value;
    char* mem_addr_ref;
}int_var;

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
{false, 0, "\",0xa,\"", 7, 'n'},
{false, 0, "    ", 4, 't'},
{false, 0, "\\", 1, '\\'},
{false, 0, "\",0x22,\"", 8, '\"'},
};

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...);
void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...);
void compiler_error(char* source_file, int line, char* format, ...);
char* read_source_code(char* filename);
char* generate_nasm(char* source_file_name, char* source_code);
char* parse_number(char* expression, char* filename, int line, int_var int_vars[], int len_int_vars);
char* parse_string(char* string, char* filename, int line);

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

    char* strings[STR_CAPACITY] = {0};
    int strings_len;

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

        instruction_op = strtok(line_cpy, " ");
        compiler_error_on_true(instruction_op == NULL, source_file_name, i + 1, "Could not read instruction\n");
        instruction_op_len = strlen(instruction_op) + 1;

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
        }else if(strcmp(instruction_op, "uprint") == 0){
            require_uprint = true;

            char* rest_of_line = strtok(NULL, "\0");
            rest_of_line = parse_number(rest_of_line, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; uprint\n"
                                    "\tmov rax, %s\n"
                                    "\tcall uprint\n", rest_of_line);
        }else if(strcmp(instruction_op, "exit") == 0){
            char* rest_of_line = strtok(NULL, "\0");
            rest_of_line = parse_number(rest_of_line, source_file_name, i, int_vars, int_var_acc);

            fprintf(output_file,    "\t;; exit\n"
                                    "\tmov rax, 60\n"
                                    "\tmov rdi, %s\n"
                                    "\tsyscall\n", rest_of_line);
        }else if(strcmp(instruction_op, "if") == 0){
            operation operator;
            char* words[3] = {0};

            for (int i = 0; i < 3; i++) {
                words[i] = strtok(NULL, " ");
                compiler_error_on_true(words[i] == NULL, source_file_name, i + 1, "Could not parse three words after if\n");
            }
            compiler_error_on_true(strtok(NULL, " "), source_file_name, i + 1, "Found more than three words after if\n");

            for(int j = 0; j < OPERATION_ENUM_END; j++){
                if(strcmp(words[1], operation_structs[j].source_name) == 0)
                    operator = operation_structs[j];
            }

            // Check if numbers are valid
            for (int j = 0; j < 3; j++) {
                if(j == 1)
                    continue;

                words[j] = parse_number(words[j], source_file_name, i, int_vars, int_var_acc);
            }

            fprintf(output_file,    "\t;; if\n"
                                    "\tmov eax, %s\n"
                                    "\tmov ebx, %s\n"
                                    "\tcmp eax, ebx\n"
                                    "\t%s .if%d\n"
                                    "\tjmp .end%d\n"
                                    "\t.if%d:\n", words[0], words[2], operator.asm_name, i, i, i);
            end_stack[end_stack_acc++] = i;

        }else if(strcmp(instruction_op, "end") == 0){
            compiler_error_on_true(line_len > instruction_op_len, source_file_name, i + 1, "Expected end of line after 'end' instruction\n");
            compiler_error_on_true(end_stack_acc <= 0, source_file_name, i + 1, "Unexpected 'end'\n");

            fprintf(output_file,    "\t;; end\n"
                                    "\t.end%d:\n", end_stack[--end_stack_acc]);
        }else if(strcmp(instruction_op, "int") == 0){
            compiler_error_on_true(instruction_op_len >= line_len, filename, i + 1, "No argument for assigning int\n");

            char* words[2] = {0};
            int_var new_int;

            for (int i = 0; i < 2; i++) {
                words[i] = strtok(NULL, " ");
                compiler_error_on_true(words[i] == NULL, source_file_name, i + 1, "Could not parse two words after if\n");
            }
            compiler_error_on_true(strtok(NULL, " "), source_file_name, i + 1, "Found more than two words after if\n");

            // Parse string that comes after the instruction to number
            for (int j = 0; j < strlen(words[0]); j++) {
                compiler_error_on_true(words[0][j] >= '0' && words[0][j] <= '9', filename, i + 1, "Character: '%c' is a digit\n", words[0][j]);
            }
            for(int j = 0; j < int_var_acc; j++){
                compiler_error_on_true(strcmp(words[0], int_vars[j].name) == 0, source_file_name, i + 1, "Redifinition of '%s'\n", words[0]);
            }
            new_int.name = malloc((strlen(words[0]) + 1 + 2) * sizeof(char));
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
        }else
            compiler_error(source_file_name, i + 1, "Could not parse operation: '%s'\n", instruction_op);

        free(line_cpy);
    }

    compiler_error_on_true(end_stack_acc != 0, filename, 0, "Not all conditionals resolved\n");

    for(int i = 0; i < lines_len; i++)
        free(lines[i]);

    free(lines);

    fprintf(output_file,    "\t;; exit\n"
                            "\tmov rax, 60\n"
                            "\tmov rdi, 0\n"
                            "\tsyscall\n");

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

int main(int argc, char *argv[]) {
    char* input_source;
    char* nasm_filename;
    const char* nasm_cmd_base = "nasm -felf64 -o ";
    char* nasm_cmd;
    const char* ld_cmd_base = "ld -o ";
    char* ld_cmd;
    char* execute_cmd;
    const char* execute_cmd_base = "./";
    char* object_filename;
    char* source_filename_cpy;

    compiler_error_on_false(argc >= 2, "Initialization", 0, "No input file provided\n");

    printf("[INFO] Input file: %s%s%s\n", SHELL_GREEN, argv[1], SHELL_WHITE);
    input_source = read_source_code(argv[1]);

    nasm_filename = generate_nasm(argv[1], input_source);
    printf("[INFO] Generating nasm source to %s%s%s\n", SHELL_GREEN, nasm_filename, SHELL_WHITE);

    source_filename_cpy = malloc((strlen(argv[1]) + 1) * sizeof(char));
    strcpy(source_filename_cpy, argv[1]);
    strtok(source_filename_cpy, ".");

    object_filename = malloc((strlen(source_filename_cpy) + strlen(".o") + 1) * sizeof(char));
    sprintf(object_filename, "%s%s", source_filename_cpy, ".o");

    nasm_cmd = malloc((strlen(nasm_cmd_base) + strlen(object_filename) + 1 + strlen(nasm_filename) + 1) * sizeof(char));
    sprintf(nasm_cmd, "%s%s %s", nasm_cmd_base, object_filename, nasm_filename);

    printf("[CMD] %s%s%s%s %s%s%s\n", nasm_cmd_base,
           SHELL_RED, object_filename, SHELL_WHITE,
           SHELL_GREEN, nasm_filename, SHELL_WHITE);
    system(nasm_cmd);
    free(nasm_cmd);

    ld_cmd = malloc((strlen(ld_cmd_base) + strlen(source_filename_cpy) + 1 + strlen(object_filename) + 1) * sizeof(char));
    sprintf(ld_cmd, "%s%s %s", ld_cmd_base, source_filename_cpy, object_filename);

    printf("[CMD] %s%s%s%s %s%s%s\n", ld_cmd_base,
           SHELL_RED, source_filename_cpy, SHELL_WHITE,
           SHELL_GREEN, object_filename, SHELL_WHITE);
    system(ld_cmd);
    free(ld_cmd);

    execute_cmd = malloc((strlen(execute_cmd_base) + strlen(source_filename_cpy) + 1) * sizeof(char));
    sprintf(execute_cmd, "%s%s", execute_cmd_base, source_filename_cpy);

    printf("[CMD] %s%s%s%s\n", execute_cmd_base,
           SHELL_GREEN, source_filename_cpy, SHELL_WHITE);
    system(execute_cmd);
    free(execute_cmd);

    free(input_source);
    free(object_filename);
    free(nasm_filename);
    free(source_filename_cpy);

    return 0;
}
