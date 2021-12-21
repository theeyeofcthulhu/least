#ifndef LSTRING_H_
#define LSTRING_H_

#include <map>
#include <memory>
#include <string>

class compile_info;

namespace lexer{
class lstr;
}

std::shared_ptr<lexer::lstr> parse_string(std::string string, int line,
                                     compile_info &c_info);

#endif // LSTRING_H_
