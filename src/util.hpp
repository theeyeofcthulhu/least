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
class Var;
}

namespace lexer {
class Token;
enum token_type : int;
} // namespace lexer

#define BODY_ID_START 1024

struct VarInfo {
    std::string name;
    var_type type;
    bool defined;
};

class CompileInfo {
  public:
    std::vector<VarInfo> known_vars;
    std::vector<std::string> known_string;

    ErrorHandler err;

    int get_next_body_id() { return body_id++; }

    CompileInfo(const std::string& pfilename) : err(pfilename), m_filename(pfilename) {}

    int check_var(const std::string& var);
    int check_str(const std::string& str);
    void error_on_undefined(std::shared_ptr<ast::Var> var_id);
    void error_on_wrong_type(std::shared_ptr<ast::Var> var_id, var_type tp);

  private:
    std::string m_filename;
    int body_id = BODY_ID_START;
};

class Filename {
public:
    std::string extension(const std::string& ext);
    std::string base() const { return m_filename; };
    Filename(const std::string& fn);
private:
    std::string m_filename;
    std::string m_noext;
};

std::vector<std::string> split(const std::string &str, char delim);
std::string read_source_code(const std::string &filename, CompileInfo &c_info);
std::string get_next_word(const std::string &str, int index, size_t &spc_idx);
size_t next_of_type_on_line(const std::vector<std::shared_ptr<lexer::Token>> &ts,
                            size_t start, lexer::token_type ty);

/* Template functions need to be implemented in header files */
template <typename T>
std::vector<T> slice(const std::vector<T> &v, int start = 0, int end = -1)
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
