#include <algorithm>
#include <cassert>
#include <stack>
#include <string_view>

#include "ast.hpp"
#include "elf_consts.hpp"
#include "macros.hpp"
#include "util.hpp"
#include "instruction.hpp"

#include "x86_64.hpp"

using namespace std::literals;

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
        m_instructions.add(Instruction(Instruction::Op::mov, o1, o2));
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

void X64Context::call(std::string_view symbol)
{
    m_instructions.add(Instruction(Instruction::Op::call, Instruction::Operand(Instruction::OpType::SymbolName, symbol)));
}

Instructions X64Context::gen_instructions()
{
    m_instructions.add_label(LabelInfo::infile("_start", STB_GLOBAL));
    /** Allocate space for variables on stack
      *
      * Has to be in 64-bit mode, because the stack pointer can
      * be a 64-bit number.
      */
    if (!m_c_info.known_vars.empty()) {
        m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rbp)),
                                                             Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rsp))));
        m_instructions.make_top_64bit();
        m_instructions.add(Instruction(Instruction::Op::sub, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rsp)),
                                                             Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(m_c_info.get_stack_size() * WORD_SIZE))));
        m_instructions.make_top_64bit();
    }

    gen_instructions_core(ast::to_base(m_root), m_root->get_body_id(), m_root->get_body_id());

    m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rax)),
                                                         Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(60))));
    m_instructions.add(Instruction(Instruction::Op::xor_, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdi)),
                                                          Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdi))));
    m_instructions.add(Instruction(Instruction::Op::syscall));

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

    m_instructions.add_label(LabelInfo::externsym("uprint"));
    m_instructions.add_label(LabelInfo::externsym("fprint"));
    m_instructions.add_label(LabelInfo::externsym("putchar"));

    return m_instructions;
}

void X64Context::gen_instructions_core(std::shared_ptr<ast::Node> root, int body_id, int real_end_id)
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
        fmt::print("TODO: T_IF");
        std::exit(1);
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
            m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rax)),
                                                                 Instruction::Operand(Instruction::OpType::Immediate, 60)));
            m_instructions.add(Instruction(Instruction::Op::syscall));
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

                    m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rax)),
                                                                         Instruction::Operand(Instruction::OpType::Immediate, 1)));
                    m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdi)),
                                                                         Instruction::Operand(Instruction::OpType::Immediate, 1)));
                    m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rsi)),
                                                                         Instruction::Operand(Instruction::OpType::String, Instruction::OpContent(str->get_str_id()))));
                    m_instructions.add(Instruction(Instruction::Op::mov, Instruction::Operand(Instruction::OpType::Register, Instruction::OpContent(Register::rdx)),
                                                                         Instruction::Operand(Instruction::OpType::Immediate, Instruction::OpContent(m_c_info.known_strings[str->get_str_id()].length()))));
                    m_instructions.add(Instruction(Instruction::Op::syscall));

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
                            call("uprint"sv);
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
    case ast::T_CMP: {
        fmt::print("TODO: T_CMP");
        std::exit(1);
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
