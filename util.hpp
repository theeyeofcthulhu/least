#ifndef UTIL_H_
#define UTIL_H_

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dictionary.hpp"
#include "error.hpp"

namespace ast {
class var;
}

namespace lexer {
class token;
enum token_type : int;
} // namespace lexer

#define BODY_ID_START 1024

struct var_info {
    std::string name;
    var_type type;
    bool defined;
};

class compile_info {
  public:
    std::vector<var_info> known_vars;
    std::vector<std::string> known_string;

    std::array<bool, LIB_ENUM_END> req_libs = {0};

    std::string filename;

    error_handler err;

    int get_next_body_id() { return body_id++; }

    compile_info(std::string pfilename) : filename(pfilename), err(pfilename) {}

    int check_var(std::string var);
    int check_str(std::string str);
    void error_on_undefined(std::shared_ptr<ast::var> var_id);
    void error_on_wrong_type(std::shared_ptr<ast::var> var_id, var_type tp);

  private:
    int body_id = BODY_ID_START;
};

std::vector<std::string> split(std::string str, char delim);
std::string read_source_code(std::string filename, compile_info &c_info);
std::string get_next_word(std::string str, int index, size_t &spc_idx);
size_t next_of_type_on_line(std::vector<std::shared_ptr<lexer::token>> ts,
                            size_t start, lexer::token_type ty);

/* Template functions need to be implemented in header files */
template <typename T>
std::vector<T> slice(std::vector<T> v, int start = 0, int end = -1)
{
    int v_len = v.size();
    int newlen;

    assert(start < end);

    if (end == -1 || end >= v_len)
        newlen = v_len - start;
    else
        newlen = end - start;

    std::vector<T> res(newlen);

    for (int i = 0; i < newlen; i++)
        res[i] = v[start + i];

    return res;
}

#endif // UTIL_H_
