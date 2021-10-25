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

enum operands{
PRINT,
EXIT,
NOOP,
};

enum cmp_operations{
EQUAL,
};

typedef struct{
    char* source_name;
    int enum_name;
    char* asm_name;
}operation;

operation equals = {
"==",
EQUAL,
"je",
};

void compiler_error_on_false(bool eval, char* source_file, int line, char* format, ...);
void compiler_error_on_true(bool eval, char* source_file, int line, char* format, ...);
void compiler_error(char* source_file, int line, char* format, ...);
char* read_source_code(char* filename);
char* generate_nasm(char* source_file_name, char* source_code);
void process_line_to_nasm(char* line, FILE* nasm_file);

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
            int string_boundaries[2] = {0};
            int string_len = 0;
            int acc = 0;

            // Copy line back into line_cpy, was destroyed with strtok()
            strcpy(line_cpy, lines[i]);

            // Parse string that comes after the instruction
            for (int j = instruction_op_len; j < line_len; j++) {
                if(line_cpy[j] == '"'){
                    if(acc == 0)
                        compiler_error_on_false((j+1) < line_len, source_file_name, i + 1, "Reached end of line while parsing first '\"' in string\n");

                    string_boundaries[acc++] = j;
                    compiler_error_on_true(acc > 2, source_file_name, i + 1, "Found more than two '\"' while parsing string\n");
                    if(acc == 2){
                        compiler_error_on_false((j+1) == line_len, source_file_name, i + 1, "Expected end of line after string\n");
                        break;
                    }
                }
                compiler_error_on_true(line_cpy[j] != '"' && acc == 0, source_file_name, i + 1,
                                        "First '\"' not at beginning of parsed sequence, instead found '%c'\n", line_cpy[j]);
            }
            compiler_error_on_false(acc == 2, source_file_name, i + 1, "Did not find two '\"' while parsing string\n");

            string_len = string_boundaries[1] - string_boundaries[0] - 1;
            compiler_error_on_false(string_len > 0, source_file_name, i + 1, "Refusing to print empty string\n");

            strings_len = i + 1;
            strings[i] = malloc((string_len + 1) * sizeof(char));
            memcpy(strings[i], line_cpy + string_boundaries[0] + 1, string_len);
            strings[i][string_len] = '\0';

            fprintf(output_file,    "\t;; print\n"
                                    "\tmov rax, 1\n"
                                    "\tmov rdi, 1\n"
                                    "\tmov rsi, str%d\n"
                                    "\tmov rdx, str%dLen\n"
                                    "\tsyscall\n", i, i);
        }else if(strcmp(instruction_op, "exit") == 0){
            char code[INT_LEN] = {0};
            int acc = 0;

            strcpy(line_cpy, lines[i]);

            compiler_error_on_true(instruction_op_len >= line_len, filename, i, "No argument for function '%s'\n", instruction_op);
            // Parse string that comes after the instruction to number
            for (int j = instruction_op_len; j < line_len; j++) {
                compiler_error_on_true(line_cpy[j] < '0' || line_cpy[j] > '9', filename, i, "Character: '%c' is not a digit\n", line_cpy[j]);
                compiler_error_on_true(acc >= INT_LEN, filename, i, "Number too long\n");
                code[acc++] = line_cpy[j];
            }

            fprintf(output_file,    "\t;; exit\n"
                                    "\tmov rax, 60\n"
                                    "\tmov rdi, %s\n"
                                    "\tsyscall\n", code);
        }else if(strcmp(instruction_op, "if") == 0){
            operation operator;
            char* words[3] = {0};

            for (int i = 0; i < 3; i++) {
                words[i] = strtok(NULL, " ");
                compiler_error_on_true(words[i] == NULL, source_file_name, i + 1, "Could not parse three words after if\n");
            }
            compiler_error_on_true(strtok(NULL, " "), source_file_name, i + 1, "Found more than three words after if\n");

            // Parse operator
            if(strcmp(words[1], "==") == 0)
                operator = equals;
            else
                compiler_error(source_file_name, i + 1, "Could not parse operator: '%s'\n", words[1]);

            // Check if numbers are valid
            for (int i = 0; i < 3; i++) {
                if(i == 1)
                    continue;

                for(int j = 0; j < INT_LEN + 1 && j < strlen(words[i]); j++){
                    compiler_error_on_true(words[i][j] < '0' || words[i][j] > '9', filename, i, "Character: '%c' is not a digit\n", words[i][j]);
                    compiler_error_on_true(j >= INT_LEN, filename, i, "Number too long\n");
                }
            }

            fprintf(output_file,    "\t;; if\n"
                                    "\tmov rax, %s\n"
                                    "\tcmp rax, %s\n"
                                    "\t%s .if%d\n"
                                    "\tjmp .end%d\n"
                                    "\t.if%d:\n", words[0], words[2], operator.asm_name, i, i, i);
            end_stack[end_stack_acc++] = i;

        }else if(strcmp(instruction_op, "end") == 0){
            compiler_error_on_true(line_len > instruction_op_len, source_file_name, i + 1, "Expected end of line after 'end' instruction\n");
            compiler_error_on_true(end_stack_acc <= 0, source_file_name, i + 1, "Unexpected 'end'\n");

            fprintf(output_file,    "\t;; end\n"
                                    "\t.end%d:\n", end_stack[--end_stack_acc]);
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

    fprintf(output_file,    "section .rodata\n");

    // Parse strings into nasm strings in data section
    for(int i = 0; i < strings_len; i++){
        if(!strings[i])
            continue;
        fprintf(output_file, "\tstr%d: db \"%s\", 10\n"
               "\tstr%dLen: equ $ - str%d\n",
               i, strings[i], i, i);
        free(strings[i]);
    }

    fclose(output_file);

    return filename;
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
