#ifndef X86_64_H_
#define X86_64_H_

#include <memory>

namespace ast {
class n_body;
}

class compile_info;

void ast_to_x86_64(std::shared_ptr<ast::n_body> root, std::string fn,
                   compile_info &c_info);

#endif // X86_64_H_
