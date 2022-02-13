#ifndef LSTRING_H_
#define LSTRING_H_

#include <map>
#include <memory>
#include <string_view>

class CompileInfo;

namespace lexer {

class Lstr;
class Num;

std::shared_ptr<Lstr> parse_string(std::string_view string, int line, CompileInfo& c_info);
std::shared_ptr<Num> parse_char(std::string_view string, int line, CompileInfo& c_info);
} // namespace lexer

#endif // LSTRING_H_
