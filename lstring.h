#ifndef LSTRING_H_
#define LSTRING_H_

#include <map>
#include <string>
#include <memory>

#include "dictionary.h"

class t_lstr;
class compile_info;

#include "lexer.h"

std::shared_ptr<t_lstr> parse_string(std::string string, int line,
                                     compile_info &c_info);

#endif // LSTRING_H_
