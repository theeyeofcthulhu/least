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
#include "elf.hpp"

#define LIBSTDLEAST "lib/libstdleast.a"

/* TODO: add more *const* to project */
/* TODO: observe blocks when looking at variable definitions */
int main(int argc, char** argv)
{
    assert_map_sizes();

    bool run_after_compile = false;
    bool output_dot = false;
    bool print_info = true;

    /* Handle command line input with getopt */
    int flag;
    while ((flag = getopt(argc, argv, "hrdq")) != -1) {
        switch (flag) {
        case 'h':
            fmt::print("Least Complicated Compiler - lcc\n"
                       "Copyright (C) 2021-2022 - theeyeofcthulhu on GitHub\n\n"
                       "usage: {} [-hrdq] FILE\n\n"
                       "-h: display this message and exit\n"
                       "-r: run program after compilation\n"
                       "-d: output graphical (SVG) representation of AST via Graphviz\n"
                       "-q: do not print information about program activity\n",
                argv[0]);
            return 0;
        case 'r':
            run_after_compile = true;
            break;
        case 'd':
            output_dot = true;
            break;
        case 'q':
            print_info = false;
            break;
        case '?':
        default:
            return 1;
        }
    }

    // Print msg, if user wants it
    auto info = [print_info](auto msg) {
        if (print_info) {
            std::cout << msg;
        }
    };

    Filename fn(argv[argc - 1]);
    CompileInfo c_info(fn.base());

    c_info.err.on_false(argc >= 2, "No input file provided");

    c_info.err.set_file(fn.base());

    /* Map contents of file into a std::string via mmap */
    info(fmt::format("[INFO] Input file: {}\n", GREEN_ARG(fn.base())));
    std::string input_source = read_source_code(fn.base(), c_info);

    /* Lex file into tokens */
    info(fmt::format("[INFO] Lexical analysis\n"));
    lexer::LexContext lex_context(input_source, c_info);
    auto ts = lex_context.lex_and_get_tokens();

    info(fmt::format("[INFO] Generating abstract syntax tree\n"));
    /* Convert tokens to abstract syntax tree */
    ast::AstContext ast_context(ts, c_info);
    std::shared_ptr<ast::Body> ast_root = ast_context.gen_ast();

    std::string dot_filename = fn.extension(".dot");

    if (output_dot) {
        /* Generate graphviz diagram from abstract syntax tree */
        info(fmt::format("[INFO] Generating tree diagram to: {}\n", GREEN_ARG(dot_filename)));
        ast_context.tree_to_dot(ast_root, dot_filename);

        std::string svg_filename = fn.extension(".svg");

        info(COLOR_CMD("dot -Tsvg -o {} {}", GREEN_ARG(svg_filename), RED_ARG(dot_filename)));
        RUN_CMD("dot -Tsvg -o {} {}", svg_filename, dot_filename);
    }

    std::string asm_filename = fn.extension(".asm");

    info(fmt::format("[INFO] Semantical analysis\n"));
    semantic::semantic_analysis(ast_root, c_info);

    // info(fmt::format("[INFO] Generating assembly to: {}\n", GREEN_ARG(asm_filename)));

    elf::X64Context asm_context(ast_root, c_info);
    elf::Instructions instructions = asm_context.gen_instructions();

    std::string object_filename = fn.extension(".o");

    info(fmt::format("[INFO] Generating object file\n"));
    elf::ElfGenerator elf_gen(fn.base(), object_filename, instructions);
    elf_gen.generate();

    // info(COLOR_CMD("nasm -g -felf64 -o {} {}", GREEN_ARG(object_filename), RED_ARG(asm_filename)));
    // RUN_CMD("nasm -g -felf64 -o {} {}", object_filename, asm_filename);

    std::string exe_filename = fn.extension("");

    info(COLOR_CMD("ld -o {} {} {}", GREEN_ARG(exe_filename), RED_ARG(object_filename), LIBSTDLEAST));
    RUN_CMD("ld -o {} {} {}", exe_filename, object_filename, LIBSTDLEAST);

    if (run_after_compile) {
        info(COLOR_CMD("./{}", GREEN_ARG(exe_filename)));
        RUN_CMD("./{}", exe_filename);
    }

    return 0;
}
