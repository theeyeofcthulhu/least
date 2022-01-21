#ifndef LSTRING_H_
#define LSTRING_H_

#include <map>
#include <memory>
#include <string>

class CompileInfo;

namespace lexer {

class Lstr;
class Num;

std::shared_ptr<Lstr> parse_string(const std::string &string, int line, CompileInfo &c_info);
std::shared_ptr<Num> parse_char(const std::string &string, int line, CompileInfo &c_info);
}  // namespace lexer

#endif  // LSTRING_H_
