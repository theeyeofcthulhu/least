#ifndef X86_64_H_
#define X86_64_H_

#include <memory>

#include "instruction.hpp"
#include "ast.hpp"

class CompileInfo;

namespace elf {

static const size_t WORD_SIZE = 8;

class X64Context {
public:
    X64Context(std::shared_ptr<ast::Body> root, CompileInfo& c_info)
        : m_root(root)
        , m_c_info(c_info)
    {}

    Instructions gen_instructions();
private:
    void number_in_register(std::shared_ptr<ast::Node> nd, Register reg);
    void print_mov_if_req(Instruction::Operand o1, Instruction::Operand o2);
    Instruction::Operand operand_from_var_or_const(std::shared_ptr<ast::Node> nd);
    void call(std::string_view symbol);

    void gen_instructions_core(std::shared_ptr<ast::Node> root, int body_id, int real_end_id);

    std::shared_ptr<ast::Body> m_root;
    CompileInfo& m_c_info;
    Instructions m_instructions;
};

}

#endif // X86_64_H_
