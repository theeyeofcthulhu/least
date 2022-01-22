#ifndef X86_64_H_
#define X86_64_H_

#include <memory>
#include <string>

namespace ast {
class Body;
}

class CompileInfo;

/*
 * Compile an abstract syntax tree starting from root to x86_64 assembly and write it into fn
 */
void ast_to_x86_64(std::shared_ptr<ast::Body> root, const std::string& fn, CompileInfo& c_info);

#endif // X86_64_H_
