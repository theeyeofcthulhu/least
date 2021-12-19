#ifndef X86_64_H_
#define X86_64_H_

#include <stdio.h>

#include "ast.h"

void ast_to_x86_64(std::shared_ptr<tree_body> root, std::string fn,
                   compile_info &c_info);

#endif // X86_64_H_
