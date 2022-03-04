#include <fmt/color.h>
#include <fmt/core.h>
#include <getopt.h>
#include <iostream>

#include "ast.hpp"
#include "dictionary.hpp"
#include "lexer.hpp"
#include "macros.hpp"
#include "semantics.hpp"
#include "util.hpp"
#include "x86_64.hpp"

#define LIBSTDLEAST "lib/libstdleast.a"

/* TODO: add more *const* to project */
/* TODO: observe blocks when looking at variable definitions */
int main(int argc, char** argv)
{
    bool run_after_compile = false;
    /* Handle command line input with getopt */
    int flag;
    while ((flag = getopt(argc, argv, "hr")) != -1) {
        switch (flag) {
        case 'h':
            fmt::print("Least Complicated Compiler - lcc\n"
                       "Copyright (C) 2021-2022 - theeyeofcthulhu on GitHub\n\n"
                       "usage: {} [-hr] FILE\n\n"
                       "-h: display this message and exit\n"
                       "-r: run program after compilation\n",
                argv[0]);
            return 0;
        case 'r':
            run_after_compile = true;
            break;
        case '?':
        default:
            return 1;
        }
    }

    Filename fn(argv[argc - 1]);
    CompileInfo c_info(fn.base());

    c_info.err.on_false(argc >= 2, "No input file provided\n");

    c_info.err.set_file(fn.base());

    /* Map contents of file into a std::string via mmap */
    fmt::print("[INFO] Input file: {}\n", GREEN_ARG(fn.base()));
    std::string input_source = read_source_code(fn.base(), c_info);

    /* Lex file into tokens */
    fmt::print("[INFO] Lexical analysis\n");
    std::vector<std::shared_ptr<lexer::Token>> ts = lexer::do_lex(input_source, c_info);

    fmt::print("[INFO] Generating abstract syntax tree\n");
    /* Convert tokens to abstract syntax tree */
    std::shared_ptr<ast::Body> ast_root = ast::gen_ast(ts, c_info);

    std::string dot_filename = fn.extension(".dot");

    /* Generate graphviz diagram from abstract syntax tree */
    fmt::print("[INFO] Generating tree diagram to: {}\n", GREEN_ARG(dot_filename));
    tree_to_dot(ast_root, dot_filename, c_info);

    std::string svg_filename = fn.extension(".svg");

    ECHO_CMD("dot -Tsvg -o {} {}", GREEN_ARG(svg_filename), RED_ARG(dot_filename));
    RUN_CMD("dot -Tsvg -o {} {}", svg_filename, dot_filename);

    std::string asm_filename = fn.extension(".asm");

    fmt::print("[INFO] Semantical analysis\n");
    semantic::semantic_analysis(ast_root, c_info);

    fmt::print("[INFO] Generating assembly to: {}\n", GREEN_ARG(asm_filename));
    ast_to_x86_64(ast_root, asm_filename, c_info);

    std::string object_filename = fn.extension(".o");

    ECHO_CMD("nasm -g -felf64 -o {} {}", GREEN_ARG(object_filename), RED_ARG(asm_filename));
    RUN_CMD("nasm -g -felf64 -o {} {}", object_filename, asm_filename);

    std::string exe_filename = fn.extension("");

    ECHO_CMD("ld -o {} {} {}", GREEN_ARG(exe_filename), RED_ARG(object_filename), LIBSTDLEAST);
    RUN_CMD("ld -o {} {} {}", exe_filename, object_filename, LIBSTDLEAST);

    if (run_after_compile) {
        ECHO_CMD("./{}", GREEN_ARG(exe_filename));
        RUN_CMD("./{}", exe_filename);
    }

    return 0;
}
