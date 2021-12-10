#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>

#include "util.h"
#include "dictionary.h"
#include "error.h"
#include "lstring.h"
#include "lexer.h"
#include "ast.h"
#include "x86_64.h"

#define SHELL_GREEN "\033[0;32m"
#define SHELL_RED   "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

#define RED(str) SHELL_RED, str, SHELL_WHITE
#define GREEN(str) SHELL_GREEN, str, SHELL_WHITE

int main(int argc, char *argv[]) {
    bool run_after_compile = false;
    /* Handle command line input with getopt */
    int flag;
    while ((flag = getopt(argc, argv, "hr")) != -1){
        switch (flag){
        case 'h':
            printf( "Least Complicated Compiler - lcc\n"
                    "Copyright (C) 2021 - theeyeofcthulhu on GitHub\n\n"
                    "usage: %s [-hr] FILE\n\n"
                    "-h: display this message and exit\n"
                    "-r: run program after compilation\n", argv[0]);
            return 0;
        case 'r':
            run_after_compile = true;
            break;
        case '?':
        default:
            return 1;
        }
    }

    compiler_error_on_false(argc >= 2, 0, "No input file provided\n");

    set_error_file(argv[argc - 1]);

    printf("[INFO] Input file: %s%s%s\n", GREEN(argv[argc - 1]));
    char* input_source = read_source_code(argv[argc - 1]);

    char* filename_no_ext = malloc((strlen(argv[argc - 1]) + 1) * sizeof(char));
    strcpy(filename_no_ext, argv[argc - 1]);

    strtok(filename_no_ext, ".");

    struct compile_info* c_info = create_c_info();

    printf("[INFO] Lexing file: %s%s%s\n", GREEN(argv[argc - 1]));
    int lex_len;
    token** ts = lex_source(input_source, &lex_len);

    tree_node* ast_root = tokens_to_ast(ts, lex_len, c_info);

    char* dot_filename = malloc((strlen(filename_no_ext) + strlen(".dot") + 1) * sizeof(char));
    sprintf(dot_filename, "%s.dot", filename_no_ext);

    printf("[INFO] Generating tree diagram to: %s%s%s\n", GREEN(dot_filename));
    tree_to_dot(ast_root, dot_filename);

    char* svg_filename = malloc((strlen(filename_no_ext) + strlen(".svg") + 1) * sizeof(char));
    sprintf(svg_filename, "%s.svg", filename_no_ext);

    const char* dot_cmd_base = "dot -Tsvg -o ";
    printf("[CMD] %s%s%s%s %s%s%s\n", dot_cmd_base,
           RED(svg_filename),
           GREEN(dot_filename));

    if(fork() == 0)
        execlp("dot", "dot", "-Tsvg", "-o", svg_filename, dot_filename, NULL);
    else
        wait(NULL);

    char* nasm_filename = malloc((strlen(filename_no_ext) + strlen(".asm") + 1) * sizeof(char));
    sprintf(nasm_filename, "%s.asm", filename_no_ext);

    printf("[INFO] Generating assembly to %s%s%s\n", GREEN(nasm_filename));
    ast_to_x86_64(ast_root, nasm_filename, c_info);

    // generate_nasm(nasm_filename, input_source, &opts);

    char* object_filename = malloc((strlen(filename_no_ext) + strlen(".o") + 1) * sizeof(char));
    sprintf(object_filename, "%s.o", filename_no_ext);

    const char* nasm_cmd_base = "nasm -g -felf64 -o ";

    printf("[CMD] %s%s%s%s %s%s%s\n", nasm_cmd_base,
           RED(object_filename),
           GREEN(nasm_filename));

    if(fork() == 0)
        execlp("nasm", "nasm", "-g", "-felf64", "-o", object_filename, nasm_filename, NULL);
    else
        wait(NULL);

    const char* ld_cmd_base = "ld -o ";

    printf("[CMD] %s%s%s%s %s%s%s ", ld_cmd_base,
        RED(filename_no_ext),
        GREEN(object_filename));

    /* Generate array of all required libs */
    int n_libs = 0;
    for(int i = 0; i < LIB_ENUM_END; i++)
        n_libs += c_info->req_libs[i];

    char* req_libs[n_libs];
    for(int i = 0, j = 0; j < LIB_ENUM_END && i < n_libs; j++){
        if(c_info->req_libs[j])
            req_libs[i++] = library_files[j];
    }

    char* ld_argv[5 + n_libs];
    ld_argv[0] = "ld";
    ld_argv[1] = "-o";
    ld_argv[2] = filename_no_ext;
    ld_argv[3] = object_filename;
    for(int i = 0; i < n_libs; i++){
        ld_argv[4 + i] = req_libs[i];
        printf("%s ", ld_argv[4 + i]);
    }
    printf("\n");
    ld_argv[4 + n_libs] = NULL;

    if(fork() == 0)
        execvp("ld", ld_argv);
    else
        wait(NULL);

    if(run_after_compile){
        const char* execute_cmd_base = "./";
        char* execute_local_file = malloc(
            (strlen(execute_cmd_base) + strlen(filename_no_ext) + 1)
            * sizeof(char));
        sprintf(execute_local_file, "%s%s", execute_cmd_base, filename_no_ext);

        printf("[CMD] %s%s%s%s\n", execute_cmd_base,
            GREEN(filename_no_ext));

        if(fork() == 0)
            execlp(execute_local_file, execute_local_file, NULL);
        else
            wait(NULL);

        free(execute_local_file);
    }

    free(object_filename);
    free(nasm_filename);
    free(filename_no_ext);

    return 0;
}
