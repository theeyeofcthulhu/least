#ifndef UTIL_H_
#define UTIL_H_

#include <array>
#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "dictionary.hpp"
#include "error.hpp"

#define DBG(x) std::cout << #x << ": " << x << '\n';

namespace ast {
class Var;
}

namespace lexer {
class Token;
enum token_type : int;
}

#define BODY_ID_START 1024

struct VarInfo {
    std::string_view name;
    var_type type;
    bool defined;
};

class CompileInfo {
public:
    std::vector<VarInfo> known_vars;
    std::vector<std::string_view> known_strings; // TODO: rename to known_strings

    ErrorHandler err;

    int get_next_body_id() { return body_id++; }

    CompileInfo(std::string_view pfilename)
        : err(pfilename)
        , m_filename(pfilename)
    {
    }

    int check_var(std::string_view var);
    int check_str(std::string_view str);
    void error_on_undefined(std::shared_ptr<ast::Var> var_id);
    void error_on_wrong_type(std::shared_ptr<ast::Var> var_id, var_type tp);

private:
    std::string_view m_filename;
    int body_id = BODY_ID_START;
};

class Filename {
public:
    std::string extension(std::string_view ext) const;
    std::string_view base() const { return m_filename; };
    Filename(std::string_view fn);

private:
    std::string_view m_filename;
    std::string_view m_noext;
};

std::vector<std::string_view> split(std::string_view str, char delim);
std::string read_source_code(std::string_view filename, CompileInfo& c_info);
std::string_view get_next_word(std::string_view str, int index, size_t& spc_idx);
size_t next_of_type_on_line(const std::vector<std::shared_ptr<lexer::Token>>& ts,
    size_t start,
    lexer::token_type ty);

/* Template functions need to be implemented in header files */
template<typename T>
std::vector<T> slice(const std::vector<T>& v, int start = 0, int end = -1)
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
