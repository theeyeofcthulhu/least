#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>

#include <fmt/ostream.h>

#include "ast.hpp"
#include "maps.hpp"
#include "util.hpp"
#include "x86_64.hpp"

#define MAX_DIGITS 32
#define STR_RESERVED_SIZE 128

static const size_t WORD_SIZE = 8;

struct cmp_operation {
    cmp_op op_enum;
    std::string_view asm_name;
    std::string_view opposite_asm_name;
};

std::string asm_from_int_or_const(std::shared_ptr<ast::Node> node, CompileInfo& c_info);

void mov_reg_into_array_access(std::shared_ptr<ast::Access> node, std::string_view reg, std::string_view intermediate, std::ofstream& out, CompileInfo& c_info);
void array_access_into_register(std::shared_ptr<ast::Access> node, std::string_view reg, std::string_view intermediate, std::ofstream& out, CompileInfo& c_info);

void print_mov_if_req(std::string_view target, std::string_view source, std::ofstream& out);
void print_vfunc_in_reg(std::shared_ptr<ast::VFunc> vfunc_nd,
    std::string_view reg,
    std::ofstream& out,
    CompileInfo& c_info);
void number_in_register(std::shared_ptr<ast::Node> nd,
    std::string_view reg,
    std::ofstream& out,
    CompileInfo& c_info);

void arithmetic_tree_to_x86_64(std::shared_ptr<ast::Node> root,
    std::string_view reg,
    std::ofstream& out,
    CompileInfo& c_info);
void ast_to_x86_64_core(std::shared_ptr<ast::Node> root,
    std::ofstream& out,
    CompileInfo& c_info,
    int body_id,
    int real_end_id,
    bool cmp_log_or = false,
    int cond_entry = -1);

const cmp_operation cmp_operation_structs[CMP_OPERATION_ENUM_END] = {
    { EQUAL, "je", "jne" },
    { NOT_EQUAL, "jne", "je" },
    { LESS, "jl", "jge" },
    { LESS_OR_EQ, "jle", "jg" },
    { GREATER, "jg", "jle" },
    { GREATER_OR_EQ, "jge", "jl" },
};

/* Get an assembly reference to a numeric variable or a constant
 * ensures variable is a number */
std::string asm_from_int_or_const(std::shared_ptr<ast::Node> node, CompileInfo& c_info)
{
    assert(node->get_type() == ast::T_VAR || node->get_type() == ast::T_CONST);
    if (node->get_type() == ast::T_VAR) {
        auto t_var = AST_SAFE_CAST(ast::Var, node);

        c_info.error_on_undefined(t_var);
        c_info.error_on_wrong_type(t_var, V_INT);

        return fmt::format("qword [rbp - {}]", c_info.known_vars[t_var->get_var_id()].stack_offset * WORD_SIZE);
    } else if (node->get_type() == ast::T_CONST) {
        return fmt::format("{}", AST_SAFE_CAST(ast::Const, node)->get_value());
    } else {
        assert(false);
    }
}

void mov_reg_into_array_access(std::shared_ptr<ast::Access> node, std::string_view reg, std::string_view intermediate, std::ofstream& out, CompileInfo& c_info)
{
    if (node->index->get_type() == ast::T_CONST) {
        fmt::print(out, "mov qword [rbp - {}], {}\n", (c_info.known_vars[node->get_array_id()].stack_offset * WORD_SIZE) - (AST_SAFE_CAST(ast::Const, node->index)->get_value() * WORD_SIZE), reg);
    } else {
        number_in_register(node->index, intermediate, out, c_info);
        fmt::print(out, "mov qword [rbp - {} + {} * {}], {}\n", (c_info.known_vars[node->get_array_id()].stack_offset * WORD_SIZE), intermediate, WORD_SIZE, reg);
    }
}

void array_access_into_register(std::shared_ptr<ast::Access> node, std::string_view reg, std::string_view intermediate, std::ofstream& out, CompileInfo& c_info)
{
    if (node->index->get_type() == ast::T_CONST) {
        fmt::print(out, "mov {}, qword [rbp - {}]\n", reg, (c_info.known_vars[node->get_array_id()].stack_offset * WORD_SIZE) - (AST_SAFE_CAST(ast::Const, node->index)->get_value() * WORD_SIZE));
    } else {
        number_in_register(node->index, intermediate, out, c_info);
        fmt::print(out, "mov {}, qword [rbp - {} + {} * {}]\n", reg, (c_info.known_vars[node->get_array_id()].stack_offset * WORD_SIZE), intermediate, WORD_SIZE);
    }
}

/* Print assembly mov from source to target if they are not equal */
inline void print_mov_if_req(std::string_view target,
    std::string_view source,
    std::ofstream& out)
{
    if (target != source)
        fmt::print(out, "mov {}, {}\n", target, source);
}

void print_vfunc_in_reg(std::shared_ptr<ast::VFunc> vfunc_nd,
    std::string_view reg,
    std::ofstream& out,
    CompileInfo& c_info)
{
    auto vfunc = vfunc_nd->get_value_func();

    switch (vfunc) {
    case VF_TIME: {
        fmt::print(out, "mov rax, 201\n"
                        "xor rdi, rdi\n"
                        "syscall\n");
        print_mov_if_req(reg, "rax", out);
        break;
    }
    case VF_GETUID: {
        fmt::print(out, "mov rax, 102\n"
                        "syscall\n");
        print_mov_if_req(reg, "rax", out);
        break;
    }
    default:
        c_info.err.error("Unknown vfunc");
        break;
    }
}

/* Move a tree_node, which evaluates to a number into a register */
void number_in_register(std::shared_ptr<ast::Node> nd,
    std::string_view reg,
    std::ofstream& out,
    CompileInfo& c_info)
{
    assert(ast::could_be_num(nd->get_type()));

    switch (nd->get_type()) {
    case ast::T_ARIT:
        arithmetic_tree_to_x86_64(nd, reg, out, c_info);
        break;
    case ast::T_VAR:
    case ast::T_CONST:
        print_mov_if_req(reg, asm_from_int_or_const(nd, c_info), out);
        break;
    case ast::T_ACCESS:
        array_access_into_register(AST_SAFE_CAST(ast::Access, nd), reg, reg == "rax" ? "rbx" : "rax", out, c_info);
        break;
    case ast::T_VFUNC: {
        auto vfunc = AST_SAFE_CAST(ast::VFunc, nd);
        c_info.err.on_false(vfunc->get_return_type() == V_INT,
            "'{}' has wrong return type '{}'",
            vfunc_str_map.at(vfunc->get_value_func()),
            var_type_str_map.at(vfunc->get_return_type()));

        print_vfunc_in_reg(AST_SAFE_CAST(ast::VFunc, nd), reg, out, c_info);
        break;
    }
    default:
        c_info.err.error("UNREACHABLE: Unexpected node_type");
    }
}

/* Parse a tree representing an arithmetic expression into assembly recursively
 */
void arithmetic_tree_to_x86_64(std::shared_ptr<ast::Node> root,
    std::string_view reg,
    std::ofstream& out,
    CompileInfo& c_info)
{
    /* If we are only a number: mov us into the target and leave */
    if (root->get_type() == ast::T_VAR || root->get_type() == ast::T_CONST) {
        fmt::print(out, "mov {}, {}\n", reg, asm_from_int_or_const(root, c_info));
        return;
    }

    assert(root->get_type() == ast::T_ARIT);

    std::shared_ptr<ast::Arit> arit = AST_SAFE_CAST(ast::Arit, root);

    bool rcx_can_be_immediate = !ast::has_precedence(arit->get_arit()); /* Only 'add' and 'sub' accept immediate values as
                                                                           the second operand */
    std::string_view second_value = "rcx";

    assert(ast::could_be_num(arit->left->get_type()) && ast::could_be_num(arit->right->get_type()));

    bool value_in_rax = false;

    /* If our children are also calculations: recurse */
    if (arit->left->get_type() == ast::T_ARIT) {
        arithmetic_tree_to_x86_64(arit->left, "rax", out, c_info);
        value_in_rax = true;
    }
    if (arit->right->get_type() == ast::T_ARIT) {
        /* Preserve rax */
        if (value_in_rax)
            fmt::print(out, "push rax\n");
        arithmetic_tree_to_x86_64(arit->right, "rcx", out, c_info);
        if (value_in_rax)
            fmt::print(out, "pop rax\n");
    }

    /* If our children are numbers: mov them into the target
     * Only check for this the second time around because the numbers
     * could get overwritten if we moved before doing another calculation */
    if (arit->left->get_type() == ast::T_CONST || arit->left->get_type() == ast::T_VAR || arit->left->get_type() == ast::T_VFUNC || arit->left->get_type() == ast::T_ACCESS) {
        number_in_register(arit->left, "rax", out, c_info);

        value_in_rax = true;
    }

    switch (arit->right->get_type()) {
    case ast::T_CONST: {
        if (rcx_can_be_immediate) {
            second_value = asm_from_int_or_const(arit->right, c_info);
            break;
        }
        __attribute__((fallthrough)); /* If rcx can't be immediate: do the
                                         same as you would for var */
    }
    case ast::T_VAR: {
        fmt::print(out, "mov rcx, {}\n", asm_from_int_or_const(arit->right, c_info));
        break;
    }
    case ast::T_ACCESS: {
        number_in_register(arit->left, "rcx", out, c_info);
        break;
    }
    case ast::T_VFUNC: {
        if (value_in_rax)
            fmt::print(out, "push rax\n");
        print_vfunc_in_reg(AST_SAFE_CAST(ast::VFunc, arit->right), "rcx", out, c_info);
        if (value_in_rax)
            fmt::print(out, "pop rax\n");
        break;
    }
    default:
        break;
    }

    /* NOTE: Switch to i- div, mul once signed numbers
     * are supported */

    /* Execute the calculation */
    switch (arit->get_arit()) {
    case ADD:
        fmt::print(out, "add rax, {}\n", second_value);
        print_mov_if_req(reg, "rax", out);
        break;
    case SUB:
        fmt::print(out, "sub rax, {}\n", second_value);
        print_mov_if_req(reg, "rax", out);
        break;
    case DIV:
        fmt::print(out, "xor rdx, rdx\n"
                        "div {}\n",
            second_value);
        print_mov_if_req(reg, "rax", out);
        break;
    case MOD:
        fmt::print(out, "xor rdx, rdx\n"
                        "div {}\n",
            second_value);
        print_mov_if_req(reg, "rdx", out);
        break;
    case MUL:
        fmt::print(out, "xor rdx, rdx\n"
                        "mul {}\n",
            second_value);
        print_mov_if_req(reg, "rax", out);
        break;
    default:
        assert(false);
        break;
    }
}

void ast_to_x86_64(std::shared_ptr<ast::Body> root, std::string_view fn, CompileInfo& c_info)
{
    const std::string temp(fn);
    std::ofstream out(temp);

    fmt::print(out, ";; Generated by Least Complicated Compiler (lcc)\n"
                    "global _start\n"
                    "section .text\n"
                    "_start:\n");

    /* Allocate space var variables on stack */
    if (!c_info.known_vars.empty()) {
        fmt::print(out, "mov rbp, rsp\n"
                        "sub rsp, {}\n",
            c_info.get_stack_size() * WORD_SIZE);
    }

    ast_to_x86_64_core(ast::to_base(root), out, c_info, root->get_body_id(), root->get_body_id());

    fmt::print(out, "mov rax, 60\n"
                    "xor rdi, rdi\n"
                    "syscall\n"
                    "section .data\n");

    for (size_t i = 0; i < c_info.known_strings.size(); i++) {
        fmt::print(out, "str{0}: db \"{1}\"\n"
                        "str{0}Len: equ $ - str{0}\n",
            i, c_info.known_strings[i]);
    }

    /* Reserved string variables */
    auto is_str = [](VarInfo v) { return v.type == V_STR; };
    if (std::find_if(c_info.known_vars.begin(), c_info.known_vars.end(), is_str) != c_info.known_vars.end()) {
        fmt::print(out, "section .bss\n");
        for (size_t i = 0; i < c_info.known_vars.size(); i++) {
            auto v = c_info.known_vars[i];
            if (v.type == V_STR) {
                fmt::print(out, "strvar{0}: resb {1}\n"
                                "strvar{0}len: resq 1\n",
                    i, STR_RESERVED_SIZE);
            }
        }
    }

    fmt::print(out, "extern uprint\n");
    fmt::print(out, "extern putchar\n");

    out.close();
}

void ast_to_x86_64_core(std::shared_ptr<ast::Node> root,
    std::ofstream& out,
    CompileInfo& c_info,
    int body_id,
    int real_end_id,
    bool cmp_log_or,
    int cond_entry)
{
    static std::stack<int> while_ends {};

    c_info.err.set_line(root->get_line());
    switch (root->get_type()) {
    case ast::T_BODY: {
        std::shared_ptr<ast::Body> body = AST_SAFE_CAST(ast::Body, root);
        for (const auto& child : body->children) {
            ast_to_x86_64_core(child, out, c_info, body->get_body_id(), real_end_id);
        }
        break;
    }
    case ast::T_IF: {
        std::shared_ptr<ast::If> t_if = AST_SAFE_CAST(ast::If, root);

        /* Getting the end label for the whole block
         * we jmp there if one if succeeded and we traversed its block */
        if (!t_if->is_elif()) {
            std::shared_ptr<ast::Node> last_if = ast::get_last_if(t_if);
            if (last_if) {
                if (last_if->get_type() == ast::T_ELSE) {
                    real_end_id = AST_SAFE_CAST(ast::Else, last_if)->body->get_body_id();
                } else if (last_if->get_type() == ast::T_IF) {
                    real_end_id = AST_SAFE_CAST(ast::If, last_if)->body->get_body_id();
                }
            }
        }

        fmt::print(out, ";; {}\n", (t_if->is_elif() ? "elif" : "if"));
        ast_to_x86_64_core(t_if->condition, out, c_info, t_if->body->get_body_id(),
            real_end_id);
        ast_to_x86_64_core(t_if->body, out, c_info, t_if->body->get_body_id(), real_end_id);

        if (t_if->elif != nullptr) {
            fmt::print(out, "jmp .end{}\n"
                            ".end{}:\n",
                real_end_id, t_if->body->get_body_id());
            ast_to_x86_64_core(t_if->elif, out, c_info, t_if->body->get_body_id(), real_end_id);
        } else {
            fmt::print(out, ".end{}:\n", t_if->body->get_body_id());
        }
        break;
    }
    case ast::T_ELSE: {
        std::shared_ptr<ast::Else> t_else = AST_SAFE_CAST(ast::Else, root);

        fmt::print(out, ";; else\n");
        ast_to_x86_64_core(t_else->body, out, c_info, t_else->body->get_body_id(), real_end_id);
        fmt::print(out, ".end{}:\n", t_else->body->get_body_id());
        break;
    }
    case ast::T_WHILE: {
        std::shared_ptr<ast::While> t_while = AST_SAFE_CAST(ast::While, root);

        while_ends.push(t_while->body->get_body_id());

        fmt::print(out, ";; while\n");
        fmt::print(out, ".entry{}:\n", t_while->body->get_body_id());

        ast_to_x86_64_core(t_while->condition, out, c_info, t_while->body->get_body_id(),
            real_end_id);
        ast_to_x86_64_core(t_while->body, out, c_info, t_while->body->get_body_id(),
            real_end_id);

        fmt::print(out, "jmp .entry{}\n", t_while->body->get_body_id());
        fmt::print(out, ".end{}:\n", t_while->body->get_body_id());

        while_ends.pop();
        break;
    }
    case ast::T_FUNC: {
        std::shared_ptr<ast::Func> t_func = AST_SAFE_CAST(ast::Func, root);

        std::string_view func_name = func_str_map.at(t_func->get_func());
        fmt::print(out, ";; {}\n", func_name);

        switch (t_func->get_func()) {
        case F_EXIT: {
            number_in_register(t_func->args[0], "rdi", out, c_info);
            fmt::print(out, "mov rax, 60\n"
                            "syscall\n");
            break;
        }
        case F_ARRAY:
        case F_STR: {
            /* check_correct_function_call defines the variable */
            break;
        }
        case F_INT: {
            number_in_register(t_func->args[1],
                asm_from_int_or_const(t_func->args[0], c_info), out, c_info);
            break;
        }
        case F_PRINT: {
            std::shared_ptr<ast::Lstr> ls = AST_SAFE_CAST(ast::Lstr, t_func->args[0]);

            for (const auto& format : ls->format) {
                switch (format->get_type()) {
                case ast::T_STR: {
                    std::shared_ptr<ast::Str> str = AST_SAFE_CAST(ast::Str, format);

                    fmt::print(out, "mov rax, 1\n"
                                    "mov rdi, 1\n"
                                    "mov rsi, str{0}\n"
                                    "mov rdx, str{0}Len\n"
                                    "syscall\n",
                        str->get_str_id());
                    break;
                }
                case ast::T_VAR: {
                    auto the_var = AST_SAFE_CAST(ast::Var, format);
                    auto the_var_info = c_info.known_vars[the_var->get_var_id()];

                    c_info.error_on_undefined(the_var);

                    switch (the_var_info.type) {
                    case V_INT: {
                        fmt::print(out, "mov rdi, {}\n"
                                        "call uprint\n",
                            asm_from_int_or_const(format, c_info));
                        break;
                    }
                    case V_STR: {
                        fmt::print(out, "mov rax, 1\n"
                                        "mov rdi, 1\n"
                                        "mov rsi, strvar{0}\n"
                                        "mov rdx, [strvar{0}len]\n"
                                        "syscall\n",
                            the_var->get_var_id());
                        break;
                    }
                    default: {
                        assert(false && "Defined variable not assigned "
                                        "type, unreachable");
                        break;
                    }
                    }
                    break;
                }
                case ast::T_CONST:
                case ast::T_ACCESS: {
                    number_in_register(format, "rdi", out, c_info);
                    fmt::print(out, "call uprint\n");
                    break;
                }
                default:
                    c_info.err.error("Unexpected format token in string");
                    break;
                }
            }
            break;
        }
        case F_SET: {
            std::string input;

            if (t_func->args[1]->get_type() == ast::T_ACCESS) {
                array_access_into_register(AST_SAFE_CAST(ast::Access, t_func->args[1]), "rax", "rbx", out, c_info);
                input = "rax";
            } else if (t_func->args[1]->get_type() == ast::T_CONST) {
                input = std::to_string(AST_SAFE_CAST(ast::Const, t_func->args[1])->get_value());
            } else {
                number_in_register(t_func->args[1], "rax", out, c_info);
                input = "rax";
            }

            if (t_func->args[0]->get_type() == ast::T_ACCESS) {
                mov_reg_into_array_access(AST_SAFE_CAST(ast::Access, t_func->args[0]), input, "rbx", out, c_info);
            } else if (t_func->args[0]->get_type() == ast::T_VAR) {
                print_mov_if_req(asm_from_int_or_const(t_func->args[0], c_info), input, out);
            } else {
                assert(false);
            }
            break;
        }
        case F_ADD:
        case F_SUB: {
            if (t_func->args[1]->get_type() == ast::T_CONST) {
                /* In this case func name
                ('add' or 'sub') is actually
                the correct instruction */
                fmt::print(out, "{} {}, {}\n", func_name, asm_from_int_or_const(t_func->args[0], c_info), asm_from_int_or_const(t_func->args[1], c_info));
            } else {
                number_in_register(t_func->args[1], "rax", out, c_info);
                fmt::print(out, "{} {}, rax\n", func_name, asm_from_int_or_const(t_func->args[0], c_info));
            }
            break;
        }
        case F_READ: {
            auto t_var = AST_SAFE_CAST(ast::Var, t_func->args[0]);

            fmt::print(out, "xor rax, rax\n"
                            "xor rdi, rdi\n"
                            "mov rsi, strvar{0}\n"
                            "mov rdx, {1}\n"
                            "syscall\n"
                            "dec rax\n"
                            "mov [strvar{0}len], rax\n"  /* Move return value of read, i.e., length of input into the length variable */
                            "mov byte [rsi + rax], 0\n", /* Clear newline at end of input */
                t_var->get_var_id(), STR_RESERVED_SIZE);

            break;
        }
        case F_PUTCHAR: {
            number_in_register(t_func->args[0], "rdi", out, c_info);
            fmt::print(out, "call putchar\n");

            break;
        }
        case F_BREAK:
        case F_CONT: {
            c_info.err.on_true(while_ends.empty(), "'{}' outside of loop", func_name);

            /* On *break*: Jump to after the loop
             * On *continue*: Jump to the beginning of the loop */
            fmt::print(out, "jmp .{}{}\n", t_func->get_func() == F_BREAK ? "end" : "entry", while_ends.top());
            break;
        }
        default:
            c_info.err.error("TODO: unimplemented");
            break;
        }
        break;
    }
    case ast::T_CMP: {
        std::shared_ptr<ast::Cmp> cmp = AST_SAFE_CAST(ast::Cmp, root);
        std::array<std::string, 2> regs; /* Have to use std::string here because the strings returned
                                          * from asm_from_int_or_const() go out of scope. */
        cmp_op op;

        if (cmp->left->get_type() == ast::T_CONST && !cmp->right) {
            auto cnst = AST_SAFE_CAST(ast::Const, cmp->left);

            if (cnst->get_value() != 0 && cmp_log_or) {
                fmt::print(out, "jmp .cond_entry{}\n", cond_entry);
            } else if (cnst->get_value() == 0) {
                fmt::print(out, "jmp .end{}\n", body_id);
            }

            break;
        }

        /* Cannot use immediate value as first operand to 'cmp' */
        if (cmp->left->get_type() == ast::T_VAR) {
            regs[0] = asm_from_int_or_const(cmp->left, c_info);
        } else {
            arithmetic_tree_to_x86_64(cmp->left, "r8", out, c_info);
            regs[0] = "r8";
        }

        if (cmp->right) {
            if (cmp->right->get_type() == ast::T_CONST) {
                regs[1] = asm_from_int_or_const(cmp->right, c_info);
            } else {
                arithmetic_tree_to_x86_64(cmp->right, "r9", out, c_info);
                regs[1] = "r9";
            }

            op = cmp->get_cmp();
        } else {
            /* If we are not comparing something: just check against one */
            regs[1] = "0";
            op = NOT_EQUAL;
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

        if (cmp_log_or) {
            /* We are part of an or-condition, meaning that if we conceed,
             * we immediately go to the beginning of the body */
            fmt::print(out, "cmp {}, {}\n"
                            "{} .cond_entry{}\n",
                regs[0], regs[1], cmp_operation_structs[op].asm_name, cond_entry);
        } else {
            fmt::print(out, "cmp {}, {}\n"
                            "{} .end{}\n",
                regs[0], regs[1], cmp_operation_structs[op].opposite_asm_name, body_id);
        }

        break;
    }
    case ast::T_LOG: {
        auto log = AST_SAFE_CAST(ast::Log, root);

        bool need_entry = log->get_log() == OR && cond_entry == -1;
        if (need_entry) {
            cond_entry = c_info.get_next_body_id();
        }

        switch (log->get_log()) {
        case AND: {
            ast_to_x86_64_core(log->left, out, c_info, body_id, real_end_id, false,
                cond_entry);
            ast_to_x86_64_core(log->right, out, c_info, body_id, real_end_id, false,
                cond_entry);
            break;
        }
        case OR: {
            ast_to_x86_64_core(log->left, out, c_info, body_id, real_end_id, true,
                cond_entry);
            ast_to_x86_64_core(log->right, out, c_info, body_id, real_end_id, false,
                cond_entry);
            break;
        }
        default:
            c_info.err.error("Unexpected tree node");
            break;
        }

        /* Is always going to be printed at the end of the whole conditional
         * block, because the whole tree will be traversed at this point,
         * even if we were not the first node */
        if (need_entry) {
            fmt::print(out, ".cond_entry{}:\n", cond_entry);
        }

        break;
    }
    default:
        c_info.err.error("Unexpected tree node");
        break;
    }
}
