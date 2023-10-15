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
    void gen_instructions_core(std::shared_ptr<ast::Node> root, int body_id);

    std::shared_ptr<ast::Body> m_root;
    CompileInfo& m_c_info;
    Instructions m_instructions;
};

}

#endif // X86_64_H_
