#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "ast.hpp"
#include "util.hpp"
#include "x86_64.hpp"

#define MAX_DIGITS 32
#define STR_RESERVED_SIZE 128

struct cmp_operation {
    ecmp_operation op_enum;
    std::string asm_name;
    std::string opposite_asm_name;
};

const cmp_operation cmp_operation_structs[CMP_OPERATION_ENUM_END] = {
    {EQUAL, "je", "jne"},   {NOT_EQUAL, "jne", "je"},
    {LESS, "jl", "jge"},    {LESS_OR_EQ, "jle", "jg"},
    {GREATER, "jg", "jle"}, {GREATER_OR_EQ, "jge", "jl"},
};

void ast_to_x86_64_core(std::shared_ptr<ast::node> root, std::fstream &out,
                        compile_info &c_info, int body_id, int real_end_id);

/* Get an assembly reference to a numeric variable or a constant
 * ensures variable is a number */
std::string asm_from_var_or_const(std::shared_ptr<ast::node> node,
                                  compile_info &c_info)
{
    std::stringstream var_or_const;

    assert(node->get_type() == ast::T_VAR || node->get_type() == ast::T_CONST);
    if (node->get_type() == ast::T_VAR) {
        auto t_var = ast::safe_cast<ast::var>(node);

        c_info.error_on_undefined(t_var);
        c_info.error_on_wrong_type(t_var, V_INT);

        var_or_const << "qword [rbp - " << (t_var->get_var_id() + 1) * 8 << "]";
    } else if (node->get_type() == ast::T_CONST) {
        var_or_const << ast::safe_cast<ast::n_const>(node)->get_value();
    }

    return var_or_const.str();
}

/* Print assembly mov from source to target if they are not equal */
void print_mov_if_req(std::string target, std::string source, std::fstream &out)
{
    if (!(target == source))
        out << "mov " << target << ", " << source << '\n';
}

/* Parse a tree representing an arithmetic expression into assembly recursively
 */
void arithmetic_tree_to_x86_64(std::shared_ptr<ast::node> root, std::string reg,
                               std::fstream &out, compile_info &c_info)
{
    /* If we are only a number: mov us into the target and leave */
    if (root->get_type() == ast::T_VAR || root->get_type() == ast::T_CONST) {
        out << "mov " << reg << ", " << asm_from_var_or_const(root, c_info)
            << "\n\n";
        return;
    }

    assert(root->get_type() == ast::T_ARIT);

    std::shared_ptr<ast::arit> arit = ast::safe_cast<ast::arit>(root);

    assert(arit->left->get_type() == ast::T_ARIT ||
           arit->left->get_type() == ast::T_CONST ||
           arit->left->get_type() == ast::T_VAR);
    assert(arit->right->get_type() == ast::T_ARIT ||
           arit->right->get_type() == ast::T_CONST ||
           arit->right->get_type() == ast::T_VAR);

    bool value_in_rax = false;

    /* If our children are also calculations: recurse */
    if (arit->left->get_type() == ast::T_ARIT) {
        arithmetic_tree_to_x86_64(arit->left, "rax", out, c_info);
        value_in_rax = true;
    }
    if (arit->right->get_type() == ast::T_ARIT) {
        /* Preserve rax */
        if (value_in_rax)
            out << "push rax"
                << "\n\n";
        arithmetic_tree_to_x86_64(arit->right, "rcx", out, c_info);
        if (value_in_rax)
            out << "pop rax"
                << "\n\n";
    }

    /* If our children are numbers: mov them into the target */
    if (arit->left->get_type() == ast::T_CONST ||
        arit->left->get_type() == ast::T_VAR)
        out << "mov rax, " << asm_from_var_or_const(arit->left, c_info)
            << "\n\n";
    if (arit->right->get_type() == ast::T_CONST ||
        arit->right->get_type() == ast::T_VAR)
        out << "mov rcx, " << asm_from_var_or_const(arit->right, c_info)
            << "\n\n";

    /* Execute the calculation */
    switch (arit->get_arit()) {
    case ADD:
        out << "add rax, rcx\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case SUB:
        out << "sub rax, rcx\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case DIV:
        out << "xor rdx, rdx\n"
               "div rcx\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case MOD:
        out << "xor rdx, rdx\n"
               "div rcx\n";
        print_mov_if_req(reg, "rdx", out);
        break;
    case MUL:
        out << "xor rdx, rdx\n"
               "mul rcx\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case ARIT_OPERATION_ENUM_END:
        c_info.err.error(0, "Invalid arithmetic operation\n");
        break;
    }

    out << '\n';
}

void number_in_register(std::shared_ptr<ast::node> nd, std::string reg,
                        std::fstream &out, compile_info &c_info)
{
    assert(nd->get_type() == ast::T_ARIT || nd->get_type() == ast::T_VAR ||
           nd->get_type() == ast::T_CONST);

    switch (nd->get_type()) {
    case ast::T_ARIT:
        arithmetic_tree_to_x86_64(nd, reg, out, c_info);
        break;
    case ast::T_VAR:
    case ast::T_CONST:
        print_mov_if_req(reg, asm_from_var_or_const(nd, c_info), out);
        break;
    default:
        c_info.err.error("UNREACHABLE: Unexpected node_type\n");
    }
}

void ast_to_x86_64(std::shared_ptr<ast::n_body> root, std::string fn,
                   compile_info &c_info)
{
    std::fstream out;
    out.open(fn, std::ios::out);

    out << ";; Generated by Least Complicated Compiler (lcc)\n"
           "global _start\n"
           "section .text\n"
           "_start:\n\n";

    if (!c_info.known_vars.empty())
        out << "mov rbp, rsp\n"
               "sub rsp, "
            << (c_info.known_vars.size()) * 8 << '\n';

    ast_to_x86_64_core(ast::to_base(root), out, c_info, root->get_body_id(),
                       root->get_body_id());

    out << "mov rax, 60\n"
           "xor rdi, rdi\n"
           "syscall\n\n"
           "section .data\n";

    for (size_t i = 0; i < c_info.known_string.size(); i++) {
        out << "str" << i << ": db \"" << c_info.known_string[i]
            << "\"\n"
               "str"
            << i << "Len: equ $ - str" << i << "\n\n";
    }

    auto is_str = [](var_info v) { return v.type == V_STR; };
    if (std::find_if(c_info.known_vars.begin(), c_info.known_vars.end(),
                     is_str) != c_info.known_vars.end()) {
        out << "section .bss\n";
        for (size_t i = 0; i < c_info.known_vars.size(); i++) {
            auto v = c_info.known_vars[i];
            if (v.type == V_STR) {
                out << "strvar" << i << ": resq " << STR_RESERVED_SIZE
                    << "\n"
                       "strvar"
                    << i << "len: resq 1\n";
            }
        }
    }

    if (!c_info.known_vars.empty()) {
        for (size_t i = 0; i < c_info.known_vars.size(); i++) {
        }
    }

    if (c_info.req_libs[LIB_UPRINT])
        out << "extern uprint\n";
    if (c_info.req_libs[LIB_PUTCHAR])
        out << "extern putchar\n";

    out.close();
}

void ast_to_x86_64_core(std::shared_ptr<ast::node> root, std::fstream &out,
                        compile_info &c_info, int body_id, int real_end_id)
{
    c_info.err.set_line(root->get_line());
    switch (root->get_type()) {
    case ast::T_BODY:
    {
        std::shared_ptr<ast::n_body> body = ast::safe_cast<ast::n_body>(root);
        for (auto child : body->children) {
            ast_to_x86_64_core(child, out, c_info, body->get_body_id(),
                               real_end_id);
        }
        break;
    }
    case ast::T_IF:
    {
        std::shared_ptr<ast::n_if> t_if = ast::safe_cast<ast::n_if>(root);

        /* Getting the end label for the whole block
         * we jmp there if one if succeeded and we traversed its block */
        if (!t_if->is_elif()) {
            std::shared_ptr<ast::node> last_if = ast::get_last_if(t_if);
            if (last_if) {
                if (last_if->get_type() == ast::T_ELSE) {
                    real_end_id = ast::safe_cast<ast::n_else>(last_if)
                                      ->body->get_body_id();
                } else if (last_if->get_type() == ast::T_IF) {
                    real_end_id =
                        ast::safe_cast<ast::n_if>(last_if)->body->get_body_id();
                }
            }
        }

        out << ";; " << (t_if->is_elif() ? "elif" : "if") << '\n';
        ast_to_x86_64_core(t_if->condition, out, c_info,
                           t_if->body->get_body_id(), real_end_id);
        ast_to_x86_64_core(t_if->body, out, c_info, t_if->body->get_body_id(),
                           real_end_id);

        if (t_if->elif != nullptr) {
            out << "jmp .end" << real_end_id
                << "\n\n"
                   ".end"
                << t_if->body->get_body_id() << ":"
                << "\n\n";
            ast_to_x86_64_core(t_if->elif, out, c_info,
                               t_if->body->get_body_id(), real_end_id);
        } else {
            out << ".end" << t_if->body->get_body_id() << ":"
                << "\n\n";
        }
        break;
    }
    case ast::T_ELSE:
    {
        std::shared_ptr<ast::n_else> t_else = ast::safe_cast<ast::n_else>(root);

        out << ";; else\n";
        ast_to_x86_64_core(t_else->body, out, c_info,
                           t_else->body->get_body_id(), real_end_id);
        out << ".end" << t_else->body->get_body_id() << ":\n\n";
        break;
    }
    case ast::T_WHILE:
    {
        std::shared_ptr<ast::n_while> t_while =
            ast::safe_cast<ast::n_while>(root);

        out << ";; while\n";
        out << ".entry" << t_while->body->get_body_id() << ":\n\n";

        ast_to_x86_64_core(t_while->condition, out, c_info,
                           t_while->body->get_body_id(), real_end_id);
        ast_to_x86_64_core(t_while->body, out, c_info,
                           t_while->body->get_body_id(), real_end_id);

        out << "jmp .entry" << t_while->body->get_body_id() << "\n\n";
        out << ".end" << t_while->body->get_body_id() << ":\n\n";
        break;
    }
    case ast::T_FUNC:
    {
        std::shared_ptr<ast::func> t_func = ast::safe_cast<ast::func>(root);
        switch (t_func->get_func()) {
        case EXIT:
        {
            ast::check_correct_function_call("exit", t_func->args, 1,
                                             {ast::T_NUM_GENERAL}, c_info);
            out << "mov rax, 60\n";
            number_in_register(t_func->args[0], "rdi", out, c_info);
            out << "syscall\n";
            break;
        }
        case STR:
        {
            ast::check_correct_function_call("str", t_func->args, 1,
                                             {ast::T_VAR}, c_info, {V_STR},
                                             {{0, V_STR}});

            c_info
                .known_vars[ast::safe_cast<ast::var>(t_func->args[0])
                                ->get_var_id()]
                .type = V_STR;
            break;
        }
        case INT:
        {
            ast::check_correct_function_call("int", t_func->args, 2,
                                             {ast::T_VAR, ast::T_NUM_GENERAL},
                                             c_info, {V_INT}, {{0, V_INT}});

            c_info
                .known_vars[ast::safe_cast<ast::var>(t_func->args[0])
                                ->get_var_id()]
                .type = V_INT;

            number_in_register(t_func->args[1], "rax", out, c_info);
            print_mov_if_req(asm_from_var_or_const(t_func->args[0], c_info),
                             "rax", out);
            break;
        }
        case PRINT:
        {
            ast::check_correct_function_call("print", t_func->args, 1,
                                             {ast::T_LSTR}, c_info);

            std::shared_ptr<ast::lstr> ls =
                ast::safe_cast<ast::lstr>(t_func->args[0]);

            for (auto format : ls->format) {
                switch (format->get_type()) {
                case ast::T_STR:
                {
                    std::shared_ptr<ast::str> str =
                        ast::safe_cast<ast::str>(format);

                    out << "mov rax, 1\n"
                           "mov rdi, 1\n"
                           "mov rsi, str"
                        << str->get_str_id()
                        << "\n"
                           "mov rdx, str"
                        << str->get_str_id()
                        << "Len\n"
                           "syscall\n";
                    break;
                }
                case ast::T_VAR:
                {
                    auto the_var = ast::safe_cast<ast::var>(format);
                    auto the_var_info =
                        c_info.known_vars[the_var->get_var_id()];

                    c_info.error_on_undefined(the_var);

                    if (the_var_info.type == V_INT) {
                        out << "mov rax, "
                            << asm_from_var_or_const(format, c_info)
                            << "\n"
                               "call uprint\n";
                        c_info.req_libs[LIB_UPRINT] = true;
                    } else if (the_var_info.type == V_STR) {
                        out << "mov rax, 1\n"
                               "mov rdi, 1\n"
                               "mov rsi, strvar"
                            << the_var->get_var_id()
                            << "\n"
                               "mov rdx, strvar"
                            << the_var->get_var_id()
                            << "len\n"
                               "syscall\n";
                    } else {
                        assert(
                            false &&
                            "Defined variable not assigned type, unreachable");
                    }
                    break;
                }
                case ast::T_CONST:
                {
                    out << "mov rax, " << asm_from_var_or_const(format, c_info)
                        << "\n"
                           "call uprint\n";
                    c_info.req_libs[LIB_UPRINT] = true;
                    break;
                }
                default:
                    c_info.err.error("Unexpected format token in string\n");
                }
            }
            break;
        }
        case SET:
        {
            ast::check_correct_function_call("set", t_func->args, 2,
                                             {ast::T_VAR, ast::T_NUM_GENERAL},
                                             c_info, {V_INT});

            number_in_register(t_func->args[1], "rax", out, c_info);
            print_mov_if_req(asm_from_var_or_const(t_func->args[0], c_info),
                             "rax", out);
            break;
        }
        case READ:
        {
            auto t_var = ast::safe_cast<ast::var>(t_func->args[0]);

            ast::check_correct_function_call("read", t_func->args, 1,
                                             {ast::T_VAR}, c_info, {V_STR});

            out << ";; read\n"
                   "xor rax, rax\n"
                   "xor rdi, rdi\n"
                   "mov rsi, strvar"
                << t_var->get_var_id()
                << "\n"
                   "mov rdx, "
                << STR_RESERVED_SIZE
                << "\n"
                   "syscall\n"
                   "dec rax\n"
                   "mov [strvar"
                << t_var->get_var_id()
                << "len], rax\n" /* Move return value of read, i.e., length of
                                    input into the length variable*/
                   "mov byte [rsi + rax], 0\n"; /* Clear newline at end of input
                                                 */
            break;
        }
        case PUTCHAR:
            c_info.err.error("TODO: unimplemented\n");
            break;
        }
        out << '\n';
        break;
    }
    case ast::T_CMP:
    {
        std::shared_ptr<ast::cmp> cmp = ast::safe_cast<ast::cmp>(root);

        arithmetic_tree_to_x86_64(cmp->left, "r8", out, c_info);
        if (cmp->right) {
            arithmetic_tree_to_x86_64(cmp->right, "r9", out, c_info);
            out << "cmp r8, r9\n"
                << cmp_operation_structs[cmp->get_cmp()].opposite_asm_name
                << " .end" << body_id << '\n';
        } else {
            /* If we are not comparing something: just check against one */
            out << "cmp r8, 1\n"
                << cmp_operation_structs[EQUAL].opposite_asm_name << " .end"
                << body_id << '\n';
        }

        break;
    }
    default:
        c_info.err.error("Unexpected tree node\n");
        break;
    }
}
