#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>

#define OP_CAPACITY 1000
#define STR_CAPACITY 1000

#define SHELL_GREEN "\033[0;32m"
#define SHELL_RED   "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

enum operands{
PRINT,
EXIT,
NOOP,
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

    assert(input_file != NULL);

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

    int operands[OP_CAPACITY] = {0};
    int operands_len = 0;

    char* strings[STR_CAPACITY] = {0};

    source_file_name_cpy = malloc((strlen(source_file_name) + 1) * sizeof(char));
    strcpy(source_file_name_cpy, source_file_name);

    strtok(source_file_name_cpy, ".");
    filename = malloc((strlen(source_file_name_cpy) + strlen(appendix) + 1) * sizeof(char));
    sprintf(filename, "%s%s", source_file_name_cpy, appendix);

    free(source_file_name_cpy);

    output_file = fopen(filename, "w");
    assert(output_file != NULL);

    fprintf(output_file, "global _start\n");
    fprintf(output_file, "section .text\n");
    fprintf(output_file, "_start:\n");

    for(int i = 0; i < strlen(source_code); i++)
        if(source_code[i] == '\n')
            lines_len++;

    lines = malloc(lines_len * sizeof(char*));
    char* tmp_ptr = source_code, *line_ptr;
    for(int i = 0; (line_ptr = strtok(tmp_ptr, "\n")) != NULL; tmp_ptr = NULL, i++){
        lines[i] = malloc((strlen(line_ptr) + 1) * sizeof(char));
        strcpy(lines[i], line_ptr);
    }

    for (int i = 0; i < lines_len; i++){
        char* instruction_op;
        int instruction_op_len = 0;
        char* instruction_cpy;
        int instruction_len = strlen(lines[i]);

        instruction_cpy = malloc((instruction_len + 1) * sizeof(char));
        strcpy(instruction_cpy, lines[i]);

        instruction_op = strtok(instruction_cpy, " ");
        assert(instruction_op != NULL);
        instruction_op_len = strlen(instruction_op) + 1;

        printf("OP: %s\n", instruction_op);

        if(strcmp(instruction_op, "print") == 0){
            int string_boundaries[2] = {0};
            int string_len = 0;

            strcpy(instruction_cpy, lines[i]);

            int acc = 0;
            for (int j = instruction_op_len; j < instruction_len; j++) {
                if(instruction_cpy[j] == '"'){
                    // On first quotes: assert that the string goes on longer
                    if(acc == 0){
                        compiler_error_on_false((j+1) < instruction_len, source_file_name, i + 1, "Reached end of line while parsing first '\"' in string\n");
                    }
                    string_boundaries[acc++] = j;
                    // Assert that quotes are only two
                    compiler_error_on_true(acc > 2, source_file_name, i + 1, "Found more than two '\"' while parsing string\n");
                    if(acc == 2){
                        // On second quotes: assert that line has ended
                        compiler_error_on_false((j+1) == instruction_len, source_file_name, i + 1, "Expected end of line after string\n");
                        break;
                    }
                }
                compiler_error_on_true(instruction_cpy[j] != '"' && acc == 0, source_file_name, i + 1,
                                        "First '\"' not at beginning of parsed sequence, instead found '%c'\n", instruction_cpy[j]);
            }
            // Assert that two quotes were found
            compiler_error_on_false(acc == 2, source_file_name, i + 1, "Did not find two '\"' while parsing string\n");

            operands[operands_len] = PRINT;

            string_len = string_boundaries[1] - string_boundaries[0] - 1;
            compiler_error_on_false(string_len > 0, source_file_name, i + 1, "Refusing to print empty string\n");
            strings[operands_len] = malloc((string_len + 1) * sizeof(char));
            memcpy(strings[operands_len], instruction_cpy + string_boundaries[0] + 1, string_len);
            strings[operands_len][string_len] = '\0';
            operands_len++;
        }else if(strcmp(instruction_op, "exit") == 0){
            operands[operands_len++] = EXIT;
        }else
            compiler_error(source_file_name, i + 1, "Could not parse operation: '%s'\n", instruction_op);

        free(instruction_cpy);
    }

    for(int i = 0; i < lines_len; i++){
        free(lines[i]);
    }
    free(lines);

    for (int i = 0; i < operands_len; i++){
        if(operands[i] == PRINT){
            fprintf(output_file,    "\t;; print\n"
                                    "\tmov rax, 1\n"
                                    "\tmov rdi, 1\n"
                                    "\tmov rsi, str%d\n"
                                    "\tmov rdx, str%dLen\n"
                                    "\tsyscall\n", i, i);
        }else if(operands[i] == EXIT){
            fprintf(output_file,    "\t;; exit\n"
                                    "\tmov rax, 60\n"
                                    "\tmov rdi, 0\n"
                                    "\tsyscall\n");
        }
    }

    fprintf(output_file,    "\t;; exit\n"
                            "\tmov rax, 60\n"
                            "\tmov rdi, 0\n"
                            "\tsyscall\n");

    fprintf(output_file,    "section .rodata\n");

    for(int i = 0; i < operands_len; i++){
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
    char* nasm_file;
    const char* nasm_cmd_base = "nasm -felf64 -o ";
    char* nasm_cmd;
    const char* ld_cmd_base = "ld -o ";
    char* ld_cmd;
    char* execute_cmd;
    const char* execute_cmd_base = "./";
    char* object_filename;
    char* executable_filename;
    char* source_file_name_cpy;

    assert(argc >= 2);

    printf("[INFO] Input file: %s%s%s\n", SHELL_GREEN, argv[1], SHELL_WHITE);
    input_source = read_source_code(argv[1]);

    nasm_file = generate_nasm(argv[1], input_source);
    printf("[INFO] Generating nasm source to %s%s%s\n", SHELL_GREEN, nasm_file, SHELL_WHITE);

    source_file_name_cpy = malloc((strlen(argv[1]) + 1) * sizeof(char));
    strcpy(source_file_name_cpy, argv[1]);
    strtok(source_file_name_cpy, ".");

    executable_filename = source_file_name_cpy;

    object_filename = malloc((strlen(source_file_name_cpy) + strlen(".o") + 1) * sizeof(char));
    sprintf(object_filename, "%s%s", source_file_name_cpy, ".o");

    nasm_cmd = malloc((strlen(nasm_cmd_base) + strlen(object_filename) + 1 + strlen(nasm_file) + 1) * sizeof(char));
    sprintf(nasm_cmd, "%s%s %s", nasm_cmd_base, object_filename, nasm_file);

    printf("[CMD] %s%s%s%s %s%s%s\n", nasm_cmd_base,
           SHELL_RED, object_filename, SHELL_WHITE,
           SHELL_GREEN, nasm_file, SHELL_WHITE);
    system(nasm_cmd);
    free(nasm_cmd);

    ld_cmd = malloc((strlen(ld_cmd_base) + strlen(executable_filename) + 1 + strlen(object_filename) + 1) * sizeof(char));
    sprintf(ld_cmd, "%s%s %s", ld_cmd_base, executable_filename, object_filename);

    printf("[CMD] %s%s%s%s %s%s%s\n", ld_cmd_base,
           SHELL_RED, executable_filename, SHELL_WHITE,
           SHELL_GREEN, object_filename, SHELL_WHITE);
    system(ld_cmd);
    free(ld_cmd);

    execute_cmd = malloc((strlen(execute_cmd_base) + strlen(executable_filename) + 1) * sizeof(char));
    sprintf(execute_cmd, "%s%s", execute_cmd_base, executable_filename);

    printf("[CMD] %s%s%s%s\n", execute_cmd_base,
           SHELL_GREEN, executable_filename, SHELL_WHITE);
    system(execute_cmd);
    free(execute_cmd);

    free(input_source);
    free(object_filename);
    free(nasm_file);
    free(source_file_name_cpy);

    return 0;
}
