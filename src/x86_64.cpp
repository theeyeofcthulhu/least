#include "x86_64.hpp"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stack>
#include <string>

#include "ast.hpp"
#include "util.hpp"

#define MAX_DIGITS 32
#define STR_RESERVED_SIZE 128

struct cmp_operation {
    cmp_op op_enum;
    std::string asm_name;
    std::string opposite_asm_name;
};

std::string asm_from_int_or_const(std::shared_ptr<ast::Node> node, CompileInfo &c_info);
void print_mov_if_req(const std::string &target, const std::string &source, std::fstream &out);
void print_vfunc_in_reg(std::shared_ptr<ast::VFunc> vfunc_nd,
                        const std::string &reg,
                        std::fstream &out,
                        CompileInfo &c_info);
void number_in_register(std::shared_ptr<ast::Node> nd,
                        const std::string &reg,
                        std::fstream &out,
                        CompileInfo &c_info);

void arithmetic_tree_to_x86_64(std::shared_ptr<ast::Node> root,
                               const std::string &reg,
                               std::fstream &out,
                               CompileInfo &c_info);
void ast_to_x86_64_core(std::shared_ptr<ast::Node> root,
                        std::fstream &out,
                        CompileInfo &c_info,
                        int body_id,
                        int real_end_id,
                        bool cmp_log_or = false,
                        int cond_entry = -1);

const cmp_operation cmp_operation_structs[CMP_OPERATION_ENUM_END] = {
    {EQUAL, "je", "jne"},      {NOT_EQUAL, "jne", "je"}, {LESS, "jl", "jge"},
    {LESS_OR_EQ, "jle", "jg"}, {GREATER, "jg", "jle"},   {GREATER_OR_EQ, "jge", "jl"},
};

/* How each function needs to be called */
const std::map<func_id, ast::FunctionSpec> func_spec_map = {
    std::make_pair<func_id, ast::FunctionSpec>(F_PRINT,     {"print",       1, {ast::T_LSTR}, {}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_EXIT,      {"exit",        1, {ast::T_NUM_GENERAL}, {}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_READ,      {"read",        1, {ast::T_VAR}, {V_STR}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_SET,       {"set",         2, {ast::T_VAR, ast::T_NUM_GENERAL}, {V_INT}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_ADD,       {"add",         2, {ast::T_VAR, ast::T_NUM_GENERAL}, {V_INT}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_SUB,       {"sub",         2, {ast::T_VAR, ast::T_NUM_GENERAL}, {V_INT}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_BREAK,     {"break",       0, {}, {}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_CONT,      {"continue",    0, {}, {}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_PUTCHAR,   {"putchar",     1, {ast::T_NUM_GENERAL}, {}, {}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_INT,       {"int",         2, {ast::T_VAR, ast::T_NUM_GENERAL}, {V_INT}, {{0, V_INT}}}),
    std::make_pair<func_id, ast::FunctionSpec>(F_STR,       {"str",         1, {ast::T_VAR}, {V_STR}, {{0, V_STR}}}),
};

/* Get an assembly reference to a numeric variable or a constant
 * ensures variable is a number */
std::string asm_from_int_or_const(std::shared_ptr<ast::Node> node, CompileInfo &c_info)
{
    std::stringstream var_or_const;

    assert(node->get_type() == ast::T_VAR || node->get_type() == ast::T_CONST);
    if (node->get_type() == ast::T_VAR) {
        auto t_var = ast::safe_cast<ast::Var>(node);

        c_info.error_on_undefined(t_var);
        c_info.error_on_wrong_type(t_var, V_INT);

        var_or_const << "qword [rbp - " << (t_var->get_var_id() + 1) * 8 << "]";
    } else if (node->get_type() == ast::T_CONST) {
        var_or_const << ast::safe_cast<ast::Const>(node)->get_value();
    }

    return var_or_const.str();
}

/* Print assembly mov from source to target if they are not equal */
inline void print_mov_if_req(const std::string &target,
                             const std::string &source,
                             std::fstream &out)
{
    if (target != source)
        out << "mov " << target << ", " << source << '\n';
}

void print_vfunc_in_reg(std::shared_ptr<ast::VFunc> vfunc_nd,
                        const std::string &reg,
                        std::fstream &out,
                        CompileInfo &c_info)
{
    auto vfunc = vfunc_nd->get_value_func();

    switch (vfunc) {
        case VF_TIME:
        {
            out << "mov rax, 201\n"
                   "xor rdi, rdi\n"
                   "syscall\n";
            print_mov_if_req(reg, "rax", out);
            break;
        }
        case VF_GETUID:
        {
            out << "mov rax, 102\n"
                   "syscall\n";
            print_mov_if_req(reg, "rax", out);
            break;
        }
        default:
            c_info.err.error("Unknown vfunc\n");
            break;
    }
}

/* Move a tree_node, which evaluates to a number into a register */
void number_in_register(std::shared_ptr<ast::Node> nd,
                        const std::string &reg,
                        std::fstream &out,
                        CompileInfo &c_info)
{
    assert(nd->get_type() == ast::T_ARIT || nd->get_type() == ast::T_VAR ||
           nd->get_type() == ast::T_CONST || nd->get_type() == ast::T_VFUNC);

    switch (nd->get_type()) {
        case ast::T_ARIT:
            arithmetic_tree_to_x86_64(nd, reg, out, c_info);
            break;
        case ast::T_VAR:
        case ast::T_CONST:
            print_mov_if_req(reg, asm_from_int_or_const(nd, c_info), out);
            break;
        case ast::T_VFUNC:
        {
            auto vfunc = ast::safe_cast<ast::VFunc>(nd);
            c_info.err.on_false(vfunc->get_return_type() == V_INT,
                                "'%' has wrong return type '%'\n",
                                vfunc_str_map.at(vfunc->get_value_func()),
                                var_type_str_map.at(vfunc->get_return_type()));

            print_vfunc_in_reg(ast::safe_cast<ast::VFunc>(nd), reg, out, c_info);
            break;
        }
        default:
            c_info.err.error("UNREACHABLE: Unexpected node_type\n");
    }
}

/* Parse a tree representing an arithmetic expression into assembly recursively
 */
void arithmetic_tree_to_x86_64(std::shared_ptr<ast::Node> root,
                               const std::string &reg,
                               std::fstream &out,
                               CompileInfo &c_info)
{
    /* If we are only a number: mov us into the target and leave */
    if (root->get_type() == ast::T_VAR || root->get_type() == ast::T_CONST) {
        out << "mov " << reg << ", " << asm_from_int_or_const(root, c_info) << "\n";
        return;
    }

    assert(root->get_type() == ast::T_ARIT);

    std::shared_ptr<ast::Arit> arit = ast::safe_cast<ast::Arit>(root);

    bool rcx_can_be_immediate =
        !ast::has_precedence(arit->get_arit()); /* Only 'add' and 'sub' accept immediate values as
                                                   the second operand */
    std::string second_value = "rcx";

    assert(arit->left->get_type() == ast::T_ARIT || arit->left->get_type() == ast::T_CONST ||
           arit->left->get_type() == ast::T_VFUNC || arit->left->get_type() == ast::T_VAR);
    assert(arit->right->get_type() == ast::T_ARIT || arit->right->get_type() == ast::T_CONST ||
           arit->right->get_type() == ast::T_VFUNC || arit->right->get_type() == ast::T_VAR);

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
    if (arit->left->get_type() == ast::T_CONST || arit->left->get_type() == ast::T_VAR ||
        arit->left->get_type() == ast::T_VFUNC) {
        number_in_register(arit->left, "rax", out, c_info);

        value_in_rax = true;
    }

    switch (arit->right->get_type()) {
        case ast::T_CONST:
        {
            if (rcx_can_be_immediate) {
                second_value = asm_from_int_or_const(arit->right, c_info);
                break;
            }
            __attribute__((fallthrough)); /* If rcx can't be immediate: do the
                                             same as you would for var */
        }
        case ast::T_VAR:
        {
            out << "mov rcx, " << asm_from_int_or_const(arit->right, c_info) << "\n";
            break;
        }
        case ast::T_VFUNC:
        {
            if (value_in_rax)
                out << "push rax\n";
            print_vfunc_in_reg(ast::safe_cast<ast::VFunc>(arit->right), "rcx", out, c_info);
            if (value_in_rax)
                out << "pop rax\n";
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

void ast_to_x86_64(std::shared_ptr<ast::Body> root, const std::string &fn, CompileInfo &c_info)
{
    std::fstream out;
    out.open(fn, std::ios::out);

    out << ";; Generated by Least Complicated Compiler (lcc)\n"
           "global _start\n"
           "section .text\n"
           "_start:\n";

    /* Allocate space var variables on stack */
    if (!c_info.known_vars.empty()) {
        out << "mov rbp, rsp\n"
               "sub rsp, "
            << (c_info.known_vars.size()) * 8 << '\n';
    }

    ast_to_x86_64_core(ast::to_base(root), out, c_info, root->get_body_id(), root->get_body_id());

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
    auto is_str = [](VarInfo v) { return v.type == V_STR; };
    if (std::find_if(c_info.known_vars.begin(), c_info.known_vars.end(), is_str) !=
        c_info.known_vars.end()) {
        out << "section .bss\n";
        for (size_t i = 0; i < c_info.known_vars.size(); i++) {
            auto v = c_info.known_vars[i];
            if (v.type == V_STR) {
                out << "strvar" << i << ": resb " << STR_RESERVED_SIZE
                    << "\n"
                       "strvar"
                    << i << "len: resq 1\n";
            }
        }
    }

    out << "extern uprint\n";
    out << "extern putchar\n";

    out.close();
}

void ast_to_x86_64_core(std::shared_ptr<ast::Node> root,
                        std::fstream &out,
                        CompileInfo &c_info,
                        int body_id,
                        int real_end_id,
                        bool cmp_log_or,
                        int cond_entry)
{
    static std::stack<int> while_ends{};

    c_info.err.set_line(root->get_line());
    switch (root->get_type()) {
        case ast::T_BODY:
        {
            std::shared_ptr<ast::Body> body = ast::safe_cast<ast::Body>(root);
            for (const auto &child : body->children) {
                ast_to_x86_64_core(child, out, c_info, body->get_body_id(), real_end_id);
            }
            break;
        }
        case ast::T_IF:
        {
            std::shared_ptr<ast::If> t_if = ast::safe_cast<ast::If>(root);

            /* Getting the end label for the whole block
             * we jmp there if one if succeeded and we traversed its block */
            if (!t_if->is_elif()) {
                std::shared_ptr<ast::Node> last_if = ast::get_last_if(t_if);
                if (last_if) {
                    if (last_if->get_type() == ast::T_ELSE) {
                        real_end_id = ast::safe_cast<ast::Else>(last_if)->body->get_body_id();
                    } else if (last_if->get_type() == ast::T_IF) {
                        real_end_id = ast::safe_cast<ast::If>(last_if)->body->get_body_id();
                    }
                }
            }

            out << ";; " << (t_if->is_elif() ? "elif" : "if") << '\n';
            ast_to_x86_64_core(t_if->condition, out, c_info, t_if->body->get_body_id(),
                               real_end_id);
            ast_to_x86_64_core(t_if->body, out, c_info, t_if->body->get_body_id(), real_end_id);

            if (t_if->elif != nullptr) {
                out << "jmp .end" << real_end_id
                    << "\n"
                       ".end"
                    << t_if->body->get_body_id() << ":\n";
                ast_to_x86_64_core(t_if->elif, out, c_info, t_if->body->get_body_id(), real_end_id);
            } else {
                out << ".end" << t_if->body->get_body_id() << ":\n";
            }
            break;
        }
        case ast::T_ELSE:
        {
            std::shared_ptr<ast::Else> t_else = ast::safe_cast<ast::Else>(root);

            out << ";; else\n";
            ast_to_x86_64_core(t_else->body, out, c_info, t_else->body->get_body_id(), real_end_id);
            out << ".end" << t_else->body->get_body_id() << ":\n";
            break;
        }
        case ast::T_WHILE:
        {
            std::shared_ptr<ast::While> t_while = ast::safe_cast<ast::While>(root);

            while_ends.push(t_while->body->get_body_id());

            out << ";; while\n";
            out << ".entry" << t_while->body->get_body_id() << ":\n";

            ast_to_x86_64_core(t_while->condition, out, c_info, t_while->body->get_body_id(),
                               real_end_id);
            ast_to_x86_64_core(t_while->body, out, c_info, t_while->body->get_body_id(),
                               real_end_id);

            out << "jmp .entry" << t_while->body->get_body_id() << "\n";
            out << ".end" << t_while->body->get_body_id() << ":\n";

            while_ends.pop();
            break;
        }
        case ast::T_FUNC:
        {
            std::shared_ptr<ast::Func> t_func = ast::safe_cast<ast::Func>(root);

            ast::check_correct_function_call(func_spec_map.at(t_func->get_func()), t_func->args,
                                             c_info);

            const std::string func_name = func_str_map.at(t_func->get_func());
            out << ";; " << func_name << '\n';

            switch (t_func->get_func()) {
                case F_EXIT:
                {
                    number_in_register(t_func->args[0], "rdi", out, c_info);
                    out << "mov rax, 60\n"
                           "syscall\n";
                    break;
                }
                case F_STR:
                {
                    /* check_correct_function_call defines the variable */
                    break;
                }
                case F_INT:
                {
                    number_in_register(t_func->args[1],
                                       asm_from_int_or_const(t_func->args[0], c_info), out, c_info);
                    break;
                }
                case F_PRINT:
                {
                    std::shared_ptr<ast::Lstr> ls = ast::safe_cast<ast::Lstr>(t_func->args[0]);

                    for (const auto &format : ls->format) {
                        switch (format->get_type()) {
                            case ast::T_STR:
                            {
                                std::shared_ptr<ast::Str> str = ast::safe_cast<ast::Str>(format);

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
                                auto the_var = ast::safe_cast<ast::Var>(format);
                                auto the_var_info = c_info.known_vars[the_var->get_var_id()];

                                c_info.error_on_undefined(the_var);

                                switch (the_var_info.type) {
                                    case V_INT:
                                    {
                                        out << "mov rax, " << asm_from_int_or_const(format, c_info)
                                            << "\n"
                                               "call uprint\n";
                                        break;
                                    }
                                    case V_STR:
                                    {
                                        out << "mov rax, 1\n"
                                               "mov rdi, 1\n"
                                               "mov rsi, strvar"
                                            << the_var->get_var_id()
                                            << "\n"
                                               "mov rdx, [strvar"
                                            << the_var->get_var_id()
                                            << "len]\n"
                                               "syscall\n";
                                        break;
                                    }
                                    default:
                                    {
                                        assert(false &&
                                               "Defined variable not assigned "
                                               "type, unreachable");
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
                                break;
                            }
                            default:
                                c_info.err.error("Unexpected format token in string\n");
                                break;
                        }
                    }
                    break;
                }
                case F_SET:
                {
                    number_in_register(t_func->args[1],
                                       asm_from_int_or_const(t_func->args[0], c_info), out, c_info);
                    break;
                }
                case F_ADD:
                case F_SUB:
                {
                    if (t_func->args[1]->get_type() == ast::T_CONST) {
                        out << func_name << " "
                            << asm_from_int_or_const(t_func->args[0],
                                                     c_info) /* In this case func name
                                                                ('add' or 'sub') is actually
                                                                the correct instruction */
                            << ", " << asm_from_int_or_const(t_func->args[1], c_info) << "\n";
                    } else {
                        number_in_register(t_func->args[1], "rax", out, c_info);
                        out << func_name << " " << asm_from_int_or_const(t_func->args[0], c_info)
                            << ", rax\n";
                    }
                    break;
                }
                case F_READ:
                {
                    auto t_var = ast::safe_cast<ast::Var>(t_func->args[0]);

                    out << "xor rax, rax\n"
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
                        << "len], rax\n"                /* Move return value of read, i.e.,
                                                           length of input into the length
                                                           variable*/
                           "mov byte [rsi + rax], 0\n"; /* Clear newline at end
                                                         * of input
                                                         */
                    break;
                }
                case F_PUTCHAR:
                {
                    number_in_register(t_func->args[0], "rax", out, c_info);
                    out << "call putchar\n";

                    break;
                }
                case F_BREAK:
                case F_CONT:
                {
                    c_info.err.on_true(while_ends.empty(), "'%' outside of loop\n", func_name);

                    out << "jmp ." << (t_func->get_func() == F_BREAK ? "end" : "entry")
                        << while_ends.top() << '\n';
                    break;
                }
                default:
                    c_info.err.error("TODO: unimplemented\n");
                    break;
            }
            break;
        }
        case ast::T_CMP:
        {
            std::shared_ptr<ast::Cmp> cmp = ast::safe_cast<ast::Cmp>(root);
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
                if (cmp->right->get_type() == ast::T_CONST) {
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

            if (cmp_log_or) {
                /* We are part of an or-condition, meaning that if we conceed,
                 * we immediately go to the beginning of the body */
                out << "cmp " << regs[0] << ", " << regs[1] << "\n"
                    << cmp_operation_structs[op].asm_name << " .cond_entry" << cond_entry << '\n';
            } else {
                out << "cmp " << regs[0] << ", " << regs[1] << "\n"
                    << cmp_operation_structs[op].opposite_asm_name << " .end" << body_id << '\n';
            }

            break;
        }
        case ast::T_LOG:
        {
            auto log = ast::safe_cast<ast::Log>(root);

            bool need_entry = log->get_log() == OR && cond_entry == -1;
            if (cond_entry == -1 && need_entry) {
                cond_entry = c_info.get_next_body_id();
            }

            switch (log->get_log()) {
                case AND:
                {
                    ast_to_x86_64_core(log->left, out, c_info, body_id, real_end_id, false,
                                       cond_entry);
                    ast_to_x86_64_core(log->right, out, c_info, body_id, real_end_id, false,
                                       cond_entry);
                    break;
                }
                case OR:
                {
                    ast_to_x86_64_core(log->left, out, c_info, body_id, real_end_id, true,
                                       cond_entry);
                    ast_to_x86_64_core(log->right, out, c_info, body_id, real_end_id, false,
                                       cond_entry);
                    break;
                }
                default:
                    c_info.err.error("Unexpected tree node\n");
                    break;
            }

            /* Is always going to be printed at the end of the whole conditional
             * block, because the whole tree will be traversed at this point,
             * even if we were not the first node */
            if (need_entry) {
                out << ".cond_entry" << cond_entry << ":\n";
            }

            break;
        }
        default:
            c_info.err.error("Unexpected tree node\n");
            break;
    }
}
