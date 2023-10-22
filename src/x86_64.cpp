#include <algorithm>
#include <cassert>
#include <fmt/color.h>
#include <memory>
#include <stack>
#include <string_view>

#include "ast.hpp"
#include "elf_consts.hpp"
#include "macros.hpp"
#include "util.hpp"
#include "instruction.hpp"
#include "semantics.hpp"

#include "x86_64.hpp"

namespace elf {

void X64Context::number_in_register(std::shared_ptr<ast::Node> nd, Register reg)
{
    assert(ast::could_be_num(nd->get_type()));

    switch(nd->get_type()) {
    case ast::T_ARIT: {
        fmt::print("{}\nTODO: T_ARIT\n", __PRETTY_FUNCTION__);
        std::exit(1);
        break;
    }
    case ast::T_VAR: {
        auto var = AST_SAFE_CAST(ast::Var, nd);
        if (m_c_info.known_vars[var->get_var_id()].type == V_INT) {
            print_mov_if_req(Instruction::Operand(Instruction::OpType::Register, reg), operand_from_var_or_const(nd));
        } else if (m_c_info.known_vars[var->get_var_id()].type == V_INT) {
            fmt::print("{}\nTODO: Doubles\n", __PRETTY_FUNCTION__);
            std::exit(1);
        }
        break;
    }
    case ast::T_CONST: {
        print_mov_if_req(Instruction::Operand(Instruction::OpType::Register, reg), operand_from_var_or_const(nd));
        break;
    }
    case ast::T_DOUBLE_CONST:
        fmt::print("{}\nTODO: T_DOUBLE_CONST\n", __PRETTY_FUNCTION__);
        std::exit(1);
        break;
    case ast::T_ACCESS:
        fmt::print("{}\nTODO: T_ACCESS\n", __PRETTY_FUNCTION__);
        std::exit(1);
        break;
    case ast::T_VFUNC:
        fmt::print("{}\nTODO: T_VFUNC\n", __PRETTY_FUNCTION__);
        std::exit(1);
        break;
    default:
        UNREACHABLE();
        break;
    }
}

void X64Context::print_mov_if_req(Instruction::Operand o1, Instruction::Operand o2)
{
    if (!Instruction::OpContent::equal(o1.type, o1.cont, o2.cont))
        m_instructions.mov(o1, o2);
}

Instruction::Operand X64Context::operand_from_var_or_const(std::shared_ptr<ast::Node> nd)
{
    assert(nd->get_type() == ast::T_VAR || nd->get_type() == ast::T_CONST);

    if (nd->get_type() == ast::T_VAR) {
        // TODO: UNTESTED!
        auto var = AST_SAFE_CAST(ast::Var, nd);

        m_c_info.error_on_undefined(var);
        m_c_info.error_on_wrong_type(var, V_INT);

        return Instruction::Operand(Instruction::OpType::Memory,
                Instruction::OpContent(
                    MemoryAccess(Register::rbp, (int) (-1 * m_c_info.known_vars[var->get_var_id()].stack_offset * WORD_SIZE))));
    } else {
        auto const_ = AST_SAFE_CAST(ast::Const, nd);
        return Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(const_->get_value()));
    }
}

Instructions X64Context::gen_instructions()
{
    m_instructions.add_code_label(LabelInfo::infile("_start", STB_GLOBAL));
    /** Allocate space for variables on stack
      *
      * Has to be in 64-bit mode, because the stack pointer can
      * be a 64-bit number.
      */
    if (!m_c_info.known_vars.empty()) {
        m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rbp)),
                                                             Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rsp)));
        m_instructions.make_top_64bit();
        m_instructions.sub(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rsp)),
                                                             Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(m_c_info.get_stack_size() * WORD_SIZE)));
        m_instructions.make_top_64bit();
    }

    gen_instructions_core(ast::to_base(m_root), m_root->get_body_id(), m_root->get_body_id());

    m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rax)),
                        Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(60)));
    m_instructions.xor_(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdi)),
                         Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdi)));
    m_instructions.syscall();

    for (size_t i = 0; i < m_c_info.known_strings.size(); i++) {
        m_instructions.add_string(i, m_c_info.known_strings[i]);
    }

    for (size_t i = 0; i < m_c_info.known_double_consts.size(); i++) {
        fmt::print("TODO: reimplement doubles");
    }

    /* Reserved string variables */
    if (std::find_if(m_c_info.known_vars.begin(), m_c_info.known_vars.end(), [](VarInfo v) { return v.type == V_STR; }) != m_c_info.known_vars.end()) {
        fmt::print("TODO: reimplement .bss strings");
    }

    m_instructions.add_code_label(LabelInfo::externsym("uprint"));
    m_instructions.add_code_label(LabelInfo::externsym("fprint"));
    m_instructions.add_code_label(LabelInfo::externsym("putchar"));

    return m_instructions;
}

void X64Context::gen_instructions_core(std::shared_ptr<ast::Node> root, int body_id, int real_end_id, bool cmp_log_or, int cond_entry)
{
    static std::stack<int> while_ends {};

    m_c_info.err.set_line(root->get_line());
    switch(root->get_type()) {
    case ast::T_BODY: {
        std::shared_ptr<ast::Body> body = AST_SAFE_CAST(ast::Body, root);
        for (const auto& child : body->children) {
            gen_instructions_core(child, body->get_body_id(), real_end_id);
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

        gen_instructions_core(t_if->condition, t_if->body->get_body_id(), real_end_id);
        gen_instructions_core(t_if->body, t_if->body->get_body_id(), real_end_id);

        if (t_if->elif != nullptr) {
            m_instructions.jmp(Instruction::Operand(Instruction::OpType::SymbolName, Instruction::OpContent(fmt::format(".end{}", real_end_id))));
            m_instructions.add_code_label(LabelInfo::infile(fmt::format(".end{}", t_if->body->get_body_id()), STB_LOCAL));
            gen_instructions_core(t_if->elif, t_if->body->get_body_id(), real_end_id);
        } else {
            m_instructions.add_code_label(LabelInfo::infile(fmt::format(".end{}", t_if->body->get_body_id()), STB_LOCAL));
        }
        break;

    }
    case ast::T_ELSE: {
        fmt::print("TODO: T_ELSE");
        std::exit(1);
    }
    case ast::T_WHILE: {
        fmt::print("TODO: T_WHILE");
        std::exit(1);
    }
    case ast::T_FUNC: {
        std::shared_ptr<ast::Func> t_func = AST_SAFE_CAST(ast::Func, root);

        switch (t_func->get_func()) {
        case F_EXIT: {
            number_in_register(t_func->args[0], Register::rdi);
            m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rax)),
                                Instruction::Operand(Instruction::OpType::Immediate, 60));
            m_instructions.syscall();
            break;
        }
        case F_ARRAY:
        case F_STR: {
            /* check_correct_function_call defines the variable */
            break;
        }
        case F_INT: {
            print_mov_if_req(operand_from_var_or_const(t_func->args[0]), operand_from_var_or_const(t_func->args[1]));
            break;
        }
        case F_DOUBLE: {
            fmt::print("TODO: F_DOUBLE\n");
            std::exit(1);
            break;
        }
        case F_PRINT: {
            std::shared_ptr<ast::Lstr> ls = AST_SAFE_CAST(ast::Lstr, t_func->args[0]);

            for (const auto& format : ls->format) {
                switch (format->get_type()) {
                case ast::T_STR: {
                    /** fmt::print(out, "mov rax, 1\n"
                                    "mov rdi, 1\n"
                                    "mov rsi, str{0}\n"
                                    "mov rdx, str{0}Len\n"
                                    "syscall\n",
                        str->get_str_id()); **/
                    std::shared_ptr<ast::Str> str = AST_SAFE_CAST(ast::Str, format);

                    m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rax)),
                                        Instruction::Operand(Instruction::OpType::Immediate, 1));
                    m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdi)),
                                        Instruction::Operand(Instruction::OpType::Immediate, 1));
                    m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rsi)),
                                        Instruction::Operand(Instruction::OpType::String, Instruction::OpContent(str->get_str_id())));
                    m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdx)),
                                        Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(m_c_info.known_strings[str->get_str_id()].length())));
                    m_instructions.syscall();

                    break;
                }
                case ast::T_VAR: {
                    auto var = AST_SAFE_CAST(ast::Var, format);
                    auto var_info = m_c_info.known_vars[var->get_var_id()];

                    m_c_info.error_on_undefined(var);

                    switch (var_info.type) {
                        case V_DOUBLE: {
                            fmt::print("TODO: PRINT T_VAR V_DOUBLE\n");
                            std::exit(1);
                            break;
                        }
                        case V_STR: {
                            fmt::print("TODO: PRINT T_VAR V_STR\n");
                            std::exit(1);
                            break;
                        }
                        case V_INT: {
                            number_in_register(format, Register::rdi);
                            m_instructions.call("uprint");
                            break;
                        }
                        default:
                            UNREACHABLE();
                            break;
                    }
                    break;
                }
                case ast::T_DOUBLE_CONST:
                case ast::T_CONST:
                case ast::T_ACCESS: {
                    fmt::print("TODO: PRINT T_DOUBLE_CONST, T_CONST, T_ACCESS\n");
                    std::exit(1);
                    break;
                }
                default:
                    m_c_info.err.error("Unexpected format token in string");
                    break;
                }
            }
            break;
        }
        case F_SET: {
            fmt::print("TODO: F_SET\n");
            std::exit(1);
            break;
        }
        // TODO: make this function obsolete by overloading F_SET
        case F_SETD: {
            fmt::print("TODO: F_SETD\n");
            std::exit(1);
            break;
        }
        case F_ADD:
        case F_SUB: {
            fmt::print("TODO: F_ADD, F_SUB\n");
            std::exit(1);
            break;
        }
        case F_READ: {
            fmt::print("TODO: F_READ\n");
            std::exit(1);
            break;
        }
        case F_PUTCHAR: {
            fmt::print("TODO: F_PUTCHAR\n");
            std::exit(1);
            break;
        }
        case F_BREAK:
        case F_CONT: {
            fmt::print("TODO: F_BREAK, F_CONT\n");
            std::exit(1);
            break;
        }
        default:
            UNREACHABLE();
            break;
        }
        break;
    }
    // BIG TODO: Compile time comparisons of compile time values?
    case ast::T_CMP: {
    std::shared_ptr<ast::Cmp> cmp = AST_SAFE_CAST(ast::Cmp, root);
    std::array<Instruction::Operand, 2> ops;
    cmp_op op;

    var_type type = semantic::get_number_type(cmp->left, m_c_info);
    if (cmp->right) {
        m_c_info.err.on_false(type == semantic::get_number_type(cmp->right, m_c_info), 
            "Mismatched types in comparison: '{}' and '{}'" , 
            var_type_str_map.at(type), 
            var_type_str_map.at(semantic::get_number_type(cmp->right, m_c_info)));
    }

    if (type == V_INT) {
        if (cmp->left->get_type() == ast::T_CONST && !cmp->right) {
            auto cnst = AST_SAFE_CAST(ast::Const, cmp->left);

            // Factor out comparisons of compile time values against nothing
            if (cnst->get_value() != 0 && cmp_log_or) {
                m_instructions.jmp(Instruction::Operand(Instruction::OpType::SymbolName, fmt::format(".cond_entry{}", cond_entry)));
            } else if (cnst->get_value() == 0) {
                m_instructions.jmp(Instruction::Operand(Instruction::OpType::SymbolName, fmt::format(".end{}", body_id)));
            }

            break;
        }

        /* Cannot use immediate value as first operand to 'cmp' */
        if (cmp->left->get_type() == ast::T_VAR) {
            ops[0] = operand_from_var_or_const(cmp->left);
        } else if (cmp->left->get_type() == ast::T_CONST) {
            m_instructions.mov(Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::r8)), operand_from_var_or_const(cmp->left));
            ops[0] = Instruction::Operand(Instruction::OpType::Register, Register::r8);
        } else {
            fmt::print("T_CMP TODO ARITHMETIC\n");
            std::exit(1);
            // arithmetic_tree_to_x86_64(cmp->left, "r8", out, c_info);
            // regs[0] = "r8";
        }

        if (cmp->right) {
            if (cmp->right->get_type() == ast::T_CONST) {
                ops[1] = operand_from_var_or_const(cmp->right);
            } else {
                fmt::print("T_CMP TODO ARITHMETIC\n");
                std::exit(1);
                // arithmetic_tree_to_x86_64(cmp->right, "r9", out, c_info);
                // regs[1] = "r9";
            }

            op = cmp->get_cmp();
        } else {
            /* If we are not comparing something: just check against one */
            // NOTE: 1 correct here??
            ops[1] = Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(1));
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
            m_instructions.cmp(ops[0], ops[1]);
            m_instructions.add(Instruction(m_cmpjmpmap.at(op).opposite_op, 
                Instruction::Operand(Instruction::OpType::SymbolName, Instruction::OpContent(fmt::format(".cond_entry{}", body_id)))));
        } else {
            m_instructions.cmp(ops[0], ops[1]);
            m_instructions.add(Instruction(m_cmpjmpmap.at(op).opposite_op, 
                Instruction::Operand(Instruction::OpType::SymbolName, Instruction::OpContent(fmt::format(".end{}", body_id)))));
        }
    } else if (type == V_DOUBLE) {
        fmt::print("T_CMP TODO DOUBLE\n");
        std::exit(1);
        /** if (cmp->left->get_type() == ast::T_DOUBLE_CONST && !cmp->right) {
            auto cnst = AST_SAFE_CAST(ast::DoubleConst, cmp->left);

            if (cnst->get_value() != 0.0f && cmp_log_or) {
                fmt::print(out, "jmp .cond_entry{}\n", cond_entry);
            } else if (cnst->get_value() == 0.0f) {
                fmt::print(out, "jmp .end{}\n", body_id);
            }

            break;
        }

        // TODO: check for unordered values

        // Cannot use immediate value or memory access as first operand to 'cmp'
        arithmetic_tree_to_x86_64_double(cmp->left, "xmm8", out, c_info);
        regs[0] = "xmm8";

        if (cmp->right) {
            if (cmp->right->get_type() == ast::T_DOUBLE_CONST) {
                regs[1] = asm_from_double_or_const(cmp->right, c_info);
            } else {
                arithmetic_tree_to_x86_64_double(cmp->right, "xmm9", out, c_info);
                regs[1] = "xmm9";
            }

            op = cmp->get_cmp();
        } else {
            / If we are not comparing something: just check against one
            regs[1] = "0";
            op = NOT_EQUAL;
        }
        if (cmp_log_or) {
            // We are part of an or-condition, meaning that if we conceed,
               we immediately go to the beginning of the body
            fmt::print(out, "comisd {}, {}\n"
                            "{} .cond_entry{}\n",
                regs[0], regs[1], comisd_operation_structs[op].asm_name, cond_entry);
        } else {
            fmt::print(out, "comisd {}, {}\n"
                            "{} .end{}\n",
                regs[0], regs[1], comisd_operation_structs[op].opposite_asm_name, body_id);
        } **/
    } else {
        UNREACHABLE();
    }

    break;
    }
    case ast::T_LOG: {
        fmt::print("TODO: T_LOG");
        std::exit(1);
    }
    default: {
        UNREACHABLE();
        break;
    }
    }
}

}
