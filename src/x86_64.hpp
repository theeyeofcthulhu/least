#ifndef X86_64_H_
#define X86_64_H_

#include <memory>

namespace ast {
class Body;
}

class CompileInfo;

void ast_to_x86_64(std::shared_ptr<ast::Body> root, std::string fn,
                   CompileInfo &c_info);

#endif // X86_64_H_
