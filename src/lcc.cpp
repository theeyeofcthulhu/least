#include <getopt.h>
#include <iostream>

#include "ast.hpp"
#include "dictionary.hpp"
#include "lexer.hpp"
#include "util.hpp"
#include "x86_64.hpp"

#define SHELL_GREEN "\033[0;32m"
#define SHELL_RED "\033[0;31m"
#define SHELL_WHITE "\033[0;37m"

#define RED(str) SHELL_RED << str << SHELL_WHITE
#define GREEN(str) SHELL_GREEN << str << SHELL_WHITE

#define LIBSTDLEAST "lib/libstdleast.a"

/* TODO: observe blocks when looking at variable definitions */
int main(int argc, char *argv[])
{
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

    filename fn(argv[argc - 1]);
    compile_info c_info(fn.base());

    c_info.err.on_false(argc >= 2, "No input file provided\n");

    c_info.err.set_file(fn.base());

    /* Map contents of file into a std::string via mmap */
    std::cout << "[INFO] Input file: " << GREEN(fn.base()) << '\n';
    std::string input_source = read_source_code(fn.base(), c_info);

    /* Lex file into tokens */
    std::cout << "[INFO] Lexing file: " << GREEN(fn.base()) << '\n';
    std::vector<std::shared_ptr<lexer::token>> ts =
        lexer::do_lex(input_source, c_info);

    /* Convert tokens to abstract syntax tree */
    std::shared_ptr<ast::n_body> ast_root = ast::gen_ast(ts, c_info);

    std::string dot_filename = fn.extension(".dot");

    /* Generate graphviz diagram from abstract syntax tree */
    std::cout << "[INFO] Generating tree diagram to: " << GREEN(dot_filename)
              << '\n';
    tree_to_dot(ast_root, dot_filename, c_info);

    std::string svg_filename = fn.extension(".svg");

    std::string dot_cmd_base = "dot -Tsvg -o ";
    std::cout << "[CMD] " << dot_cmd_base << RED(svg_filename) << " "
              << GREEN(dot_filename) << '\n';

    std::string dot_cmd =
        dot_cmd_base.append(svg_filename).append(" ").append(dot_filename);
    std::system(dot_cmd.c_str());

    std::string asm_filename = fn.extension(".asm");

    std::cout << "[INFO] Generating assembly to " << GREEN(asm_filename)
              << '\n';
    ast_to_x86_64(ast_root, asm_filename, c_info);

    std::string object_filename = fn.extension(".o");

    std::string nasm_cmd_base = "nasm -g -felf64 -o ";
    std::cout << "[CMD] " << nasm_cmd_base << RED(object_filename) << " "
              << GREEN(asm_filename) << '\n';

    std::string nasm_cmd =
        nasm_cmd_base.append(object_filename).append(" ").append(asm_filename);
    std::system(nasm_cmd.c_str());

    std::string ld_cmd_base = "ld -o ";

    std::string exe_filename = fn.extension("");

    std::cout << "[CMD] " << ld_cmd_base << RED(exe_filename) << " "
              << GREEN(object_filename) << " " << LIBSTDLEAST << '\n';
    std::string ld_cmd = ld_cmd_base.append(exe_filename)
                             .append(" ")
                             .append(object_filename)
                             .append(" ")
                             .append(LIBSTDLEAST);

    std::system(ld_cmd.c_str());

    if (run_after_compile) {
        std::string exe_cmd_base = "./";
        std::string exe_cmd = exe_cmd_base;
        exe_cmd.append(exe_filename);

        std::cout << "[CMD] " << exe_cmd_base << GREEN(exe_filename) << '\n';

        std::system(exe_cmd.c_str());
    }

    return 0;
}
