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
    cmp_op op_enum;
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
std::string asm_from_int_or_const(std::shared_ptr<ast::node> node,
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
/* TODO: simplify using immediate values for add and sub */
void arithmetic_tree_to_x86_64(std::shared_ptr<ast::node> root, std::string reg,
                               std::fstream &out, compile_info &c_info)
{
    /* If we are only a number: mov us into the target and leave */
    if (root->get_type() == ast::T_VAR || root->get_type() == ast::T_CONST) {
        out << "mov " << reg << ", " << asm_from_int_or_const(root, c_info)
            << "\n";
        return;
    }

    assert(root->get_type() == ast::T_ARIT);

    std::shared_ptr<ast::arit> arit = ast::safe_cast<ast::arit>(root);

    bool rcx_can_be_immediate = !ast::has_precedence(
        arit->get_arit()); /* Only 'add' and 'sub' accept immediate values as
                              the second operand */
    std::string second_value = "rcx";

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
            out << "push rax\n";
        arithmetic_tree_to_x86_64(arit->right, "rcx", out, c_info);
        if (value_in_rax)
            out << "pop rax\n";
    }

    /* If our children are numbers: mov them into the target
     * Only check for this the second time around because the numbers
     * could get overwritten if we moved before doing another calculation */
    if (arit->left->get_type() == ast::T_CONST ||
        arit->left->get_type() == ast::T_VAR) {
        out << "mov rax, " << asm_from_int_or_const(arit->left, c_info) << "\n";
    }

    switch (arit->right->get_type()) {
    case ast::T_CONST:
    {
        if (rcx_can_be_immediate) {
            second_value = asm_from_int_or_const(arit->right, c_info);
            break;
        }
        __attribute__((fallthrough)); /* If rcx can't be immediate: do the same
                                         as you would for var */
    }
    case ast::T_VAR:
    {
        out << "mov rcx, " << asm_from_int_or_const(arit->right, c_info)
            << "\n";
        break;
    }
    default:
        break;
    }

    /* Execute the calculation */
    switch (arit->get_arit()) {
    case ADD:
        out << "add rax, " << second_value << "\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case SUB:
        out << "sub rax, " << second_value << "\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case DIV:
        out << "xor rdx, rdx\n"
               "div "
            << second_value << "\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case MOD:
        out << "xor rdx, rdx\n"
               "div "
            << second_value << "\n";
        print_mov_if_req(reg, "rdx", out);
        break;
    case MUL:
        out << "xor rdx, rdx\n"
               "mul "
            << second_value << "\n";
        print_mov_if_req(reg, "rax", out);
        break;
    case ARIT_OPERATION_ENUM_END:
        c_info.err.error(0, "Invalid arithmetic operation\n");
        break;
    }
}

/* Move a tree_node, which evaluates to a number into a register */
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
        print_mov_if_req(reg, asm_from_int_or_const(nd, c_info), out);
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
           "_start:\n";

    if (!c_info.known_vars.empty())
        out << "mov rbp, rsp\n"
               "sub rsp, "
            << (c_info.known_vars.size()) * 8 << '\n';

    ast_to_x86_64_core(ast::to_base(root), out, c_info, root->get_body_id(),
                       root->get_body_id());

    out << "mov rax, 60\n"
           "xor rdi, rdi\n"
           "syscall\n"
           "section .data\n";

    for (size_t i = 0; i < c_info.known_string.size(); i++) {
        out << "str" << i << ": db \"" << c_info.known_string[i]
            << "\"\n"
               "str"
            << i << "Len: equ $ - str" << i << "\n";
    }

    /* Reserved string variables */
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
                << "\n"
                   ".end"
                << t_if->body->get_body_id() << ":"
                << "\n";
            ast_to_x86_64_core(t_if->elif, out, c_info,
                               t_if->body->get_body_id(), real_end_id);
        } else {
            out << ".end" << t_if->body->get_body_id() << ":"
                << "\n";
        }
        break;
    }
    case ast::T_ELSE:
    {
        std::shared_ptr<ast::n_else> t_else = ast::safe_cast<ast::n_else>(root);

        out << ";; else\n";
        ast_to_x86_64_core(t_else->body, out, c_info,
                           t_else->body->get_body_id(), real_end_id);
        out << ".end" << t_else->body->get_body_id() << ":\n";
        break;
    }
    case ast::T_WHILE:
    {
        std::shared_ptr<ast::n_while> t_while =
            ast::safe_cast<ast::n_while>(root);

        out << ";; while\n";
        out << ".entry" << t_while->body->get_body_id() << ":\n";

        ast_to_x86_64_core(t_while->condition, out, c_info,
                           t_while->body->get_body_id(), real_end_id);
        ast_to_x86_64_core(t_while->body, out, c_info,
                           t_while->body->get_body_id(), real_end_id);

        out << "jmp .entry" << t_while->body->get_body_id() << "\n";
        out << ".end" << t_while->body->get_body_id() << ":\n";
        break;
    }
    case ast::T_FUNC:
    {
        std::shared_ptr<ast::func> t_func = ast::safe_cast<ast::func>(root);
        switch (t_func->get_func()) {
        case F_EXIT:
        {
            ast::check_correct_function_call("exit", t_func->args, 1,
                                             {ast::T_NUM_GENERAL}, c_info);
            out << "mov rax, 60\n";
            number_in_register(t_func->args[0], "rdi", out, c_info);
            out << "syscall\n";
            break;
        }
        case F_STR:
        {
            /* check_correct_function_call defines the variable */
            ast::check_correct_function_call("str", t_func->args, 1,
                                             {ast::T_VAR}, c_info, {V_STR},
                                             {{0, V_STR}});
            break;
        }
        case F_INT:
        {
            ast::check_correct_function_call("int", t_func->args, 2,
                                             {ast::T_VAR, ast::T_NUM_GENERAL},
                                             c_info, {V_INT}, {{0, V_INT}});

            number_in_register(t_func->args[1],
                               asm_from_int_or_const(t_func->args[0], c_info),
                               out, c_info);
            break;
        }
        case F_PRINT:
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

                    switch (the_var_info.type) {
                    case V_INT:
                    {
                        out << "mov rax, "
                            << asm_from_int_or_const(format, c_info)
                            << "\n"
                               "call uprint\n";
                        c_info.req_libs[LIB_UPRINT] = true;
                        break;
                    }
                    case V_STR:
                    {
                        out << "mov rax, 1\n"
                               "mov rdi, 1\n"
                               "mov rsi, strvar"
                            << the_var->get_var_id()
                            << "\n"
                               "mov rdx, strvar"
                            << the_var->get_var_id()
                            << "len\n"
                               "syscall\n";
                        break;
                    }
                    default:
                    {
                        assert(
                            false &&
                            "Defined variable not assigned type, unreachable");
                        break;
                    }
                    }
                    break;
                }
                case ast::T_CONST:
                {
                    out << "mov rax, " << asm_from_int_or_const(format, c_info)
                        << "\n"
                           "call uprint\n";
                    c_info.req_libs[LIB_UPRINT] = true;
                    break;
                }
                default:
                    c_info.err.error("Unexpected format token in string\n");
                    break;
                }
            }
            break;
        }
        /* TODO: implement an increment/decrement function */
        case F_SET:
        {
            ast::check_correct_function_call("set", t_func->args, 2,
                                             {ast::T_VAR, ast::T_NUM_GENERAL},
                                             c_info, {V_INT});

            number_in_register(t_func->args[1],
                               asm_from_int_or_const(t_func->args[0], c_info),
                               out, c_info);
            break;
        }
        case F_ADD:
        {
            ast::check_correct_function_call("add", t_func->args, 2,
                                             {ast::T_VAR, ast::T_NUM_GENERAL},
                                             c_info, {V_INT});

            if (t_func->args[1]->get_type() == ast::T_CONST) {
                out << "add " << asm_from_int_or_const(t_func->args[0], c_info)
                    << ", " << asm_from_int_or_const(t_func->args[1], c_info)
                    << "\n";
            } else {
                number_in_register(t_func->args[1], "rax", out, c_info);
                out << "add " << asm_from_int_or_const(t_func->args[0], c_info)
                    << ", rax\n";
            }
            break;
        }
        case F_READ:
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
        case F_PUTCHAR:
            c_info.err.error("TODO: unimplemented\n");
            break;
        }
        break;
    }
    case ast::T_CMP:
    {
        std::shared_ptr<ast::cmp> cmp = ast::safe_cast<ast::cmp>(root);
        std::array<std::string, 2> regs;
        cmp_op op;

        /* Cannot use immediate value as first operand to 'cmp' */
        if (cmp->left->get_type() == ast::T_VAR) {
            regs[0] = asm_from_int_or_const(cmp->left, c_info);
        } else {
            arithmetic_tree_to_x86_64(cmp->left, "r8", out, c_info);
            regs[0] = "r8";
        }

        if (cmp->right) {
            if (cmp->right->is_var_or_const()) {
                regs[1] = asm_from_int_or_const(cmp->right, c_info);
            } else {
                arithmetic_tree_to_x86_64(cmp->right, "r9", out, c_info);
                regs[1] = "r9";
            }

            op = cmp->get_cmp();
        } else {
            /* If we are not comparing something: just check against one */
            regs[1] = "1";
            op = EQUAL;
        }
        /* Why the opposite jump of what we are doing?
         * lets say:
         * if 2 == 2
         *     ....
         * end
         * we don't want to do:
         * cmp 2, 2
         * je beginning of if block
         * jmp end of if block
         * beginning
         * ....
         * end
         *
         * just:
         * cmp 2, 2
         * jne end of if block
         * ....
         * end */
        out << "cmp " << regs[0] << ", " << regs[1] << "\n"
            << cmp_operation_structs[op].opposite_asm_name << " .end" << body_id
            << '\n';

        break;
    }
    default:
        c_info.err.error("Unexpected tree node\n");
        break;
    }
}
