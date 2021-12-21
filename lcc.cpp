#include <getopt.h>
#include <iostream>

#include "ast.h"
#include "dictionary.h"
#include "lexer.h"
#include "util.h"
#include "x86_64.h"

#define SHELL_GREEN "\033[0;32m"
#define SHELL_RED "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

#define RED(str) SHELL_RED << str << SHELL_WHITE
#define GREEN(str) SHELL_GREEN << str << SHELL_WHITE

/* NOTE:
 * https://gcc.gnu.org/onlinedocs/libstdc++/manual/streambufs.html#io.streambuf.buffering
 * says that std::endl slows buffering by flushing; does this really matter? */
int main(int argc, char *argv[]) {
    bool run_after_compile = false;
    /* Handle command line input with getopt */
    int flag;
    while ((flag = getopt(argc, argv, "hr")) != -1) {
        switch (flag) {
        case 'h':
            std::cout << "Least Complicated Compiler - lcc\n"
                         "Copyright (C) 2021 - theeyeofcthulhu on GitHub\n\n"
                         "usage: "
                      << argv[0]
                      << " [-hr] FILE\n\n"
                         "-h: display this message and exit\n"
                         "-r: run program after compilation\n";
            return 0;
        case 'r':
            run_after_compile = true;
            break;
        case '?':
        default:
            return 1;
        }
    }

    std::string filename = argv[argc - 1];

    compile_info c_info(filename);

    c_info.err.on_false(argc >= 2, "No input file provided\n");

    c_info.err.set_file(argv[argc - 1]);

    size_t dot = filename.find('.');
    c_info.err.on_true(dot == std::string::npos, "No dot found in filename\n");

    std::string filename_no_ext = filename.substr(0, dot);

    c_info.err.on_false(filename.substr(dot, std::string::npos) == ".least",
                        "File format not recognized\n");

    /* Map contents of file into a std::string via mmap */
    std::cout << "[INFO] Input file: " << GREEN(filename) << '\n';
    std::string input_source = read_source_code(filename, c_info);

    /* Lex file into tokens */
    std::cout << "[INFO] Lexing file: " << GREEN(filename) << '\n';
    std::vector<std::shared_ptr<lexer::token>> ts = lexer::do_lex(input_source, c_info);

    /* Convert tokens to abstract syntax tree */
    std::shared_ptr<ast::n_body> ast_root = ast::gen_ast(ts, c_info);

    std::string dot_filename = filename_no_ext;
    dot_filename.append(".dot");

    /* Generate graphviz diagram from abstract syntax tree */
    std::cout << "[INFO] Generating tree diagram to: " << GREEN(dot_filename)
              << '\n';
    tree_to_dot(ast_root, dot_filename, c_info);

    std::string svg_filename = filename_no_ext;
    svg_filename.append(".svg");

    std::string dot_cmd_base = "dot -Tsvg -o ";
    std::cout << "[CMD] " << dot_cmd_base << RED(svg_filename) << " "
              << GREEN(dot_filename) << '\n';

    std::string dot_cmd =
        dot_cmd_base.append(svg_filename).append(" ").append(dot_filename);
    std::system(dot_cmd.c_str());

    std::string asm_filename = filename_no_ext;
    asm_filename.append(".asm");

    std::cout << "[INFO] Generating assembly to " << GREEN(asm_filename)
              << '\n';
    ast_to_x86_64(ast_root, asm_filename, c_info);

    std::string object_filename = filename_no_ext;
    object_filename.append(".o");

    std::string nasm_cmd_base = "nasm -g -felf64 -o ";
    std::cout << "[CMD] " << nasm_cmd_base << RED(object_filename) << " "
              << GREEN(asm_filename) << '\n';

    std::string nasm_cmd =
        nasm_cmd_base.append(object_filename).append(" ").append(asm_filename);
    std::system(nasm_cmd.c_str());

    std::string ld_cmd_base = "ld -o ";

    std::cout << "[CMD] " << ld_cmd_base << RED(filename_no_ext) << " "
              << GREEN(object_filename);
    std::string ld_cmd =
        ld_cmd_base.append(filename_no_ext).append(" ").append(object_filename);

    /* Generate array of all required libs */
    for (int i = 0; auto lib : c_info.req_libs) {
        if (lib) {
            std::string new_o = " ";
            new_o.append(library_files[i]);

            std::cout << new_o;
            ld_cmd.append(new_o);
        }
        i++;
    }
    std::cout << '\n';

    std::system(ld_cmd.c_str());

    if (run_after_compile) {
        std::string exe_cmd_base = "./";
        std::string exe_cmd = exe_cmd_base;
        exe_cmd.append(filename_no_ext);

        std::cout << "[CMD] " << exe_cmd_base << GREEN(filename_no_ext) << '\n';

        std::system(exe_cmd.c_str());
    }

    return 0;
}
