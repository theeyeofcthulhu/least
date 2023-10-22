#ifndef X86_64_H_
#define X86_64_H_

#include <array>
#include <map>
#include <memory>
#include <utility>

#include "dictionary.hpp"
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
    struct CmpJmpMap {
        int op;
        Instruction::Op corresponding_op;
        Instruction::Op opposite_op;
    };

    void number_in_register(std::shared_ptr<ast::Node> nd, Register reg);
    void print_mov_if_req(Instruction::Operand o1, Instruction::Operand o2);
    Instruction::Operand operand_from_var_or_const(std::shared_ptr<ast::Node> nd);

    void gen_instructions_core(std::shared_ptr<ast::Node> root, int body_id, int real_end_id, bool cmp_log_or=false, int cond_entry=-1);

    const std::map<int, CmpJmpMap> m_cmpjmpmap {
        { EQUAL,             CmpJmpMap(EQUAL, Instruction::Op::je, Instruction::Op::jne) },
        { NOT_EQUAL,         CmpJmpMap(EQUAL, Instruction::Op::jne, Instruction::Op::je) },
        { LESS,              CmpJmpMap(EQUAL, Instruction::Op::jl, Instruction::Op::jge) },
        { LESS_OR_EQ,        CmpJmpMap(EQUAL, Instruction::Op::jle, Instruction::Op::jg) },
        { GREATER,           CmpJmpMap(EQUAL, Instruction::Op::jg, Instruction::Op::jle) },
        { GREATER_OR_EQ,     CmpJmpMap(EQUAL, Instruction::Op::jge, Instruction::Op::jl) }
    };

    std::shared_ptr<ast::Body> m_root;
    CompileInfo& m_c_info;
    Instructions m_instructions;
};

}

#endif // X86_64_H_
