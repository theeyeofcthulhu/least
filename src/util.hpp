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

namespace ast {
class Var;
}

namespace lexer {
class Token;
enum token_type : int;
}

#define BODY_ID_START 1024

struct VarInfo {
    enum class Arrayness {
        Yes,
        No,
        Unsure,
    };

    std::string_view name;
    var_type type;
    bool defined;
    Arrayness arrayness;
    size_t stack_units;
    size_t stack_offset;

    VarInfo(std::string_view p_name, var_type p_type, bool p_defined)
        : name(p_name)
        , type(p_type)
        , defined(p_defined)
        , arrayness(Arrayness::Unsure)
        , stack_units(1)
    {
    }

    VarInfo(std::string_view p_name, var_type p_type, bool p_defined, Arrayness p_arrayness, size_t p_stack_size)
        : name(p_name)
        , type(p_type)
        , defined(p_defined)
        , arrayness(p_arrayness)
        , stack_units(p_stack_size)
    {
    }
};

class CompileInfo {
public:
    std::vector<VarInfo> known_vars;
    std::vector<std::string_view> known_strings;
    std::vector<double> known_double_consts;

    ErrorHandler err;

    int get_next_body_id() { return body_id++; }

    CompileInfo(std::string_view pfilename)
        : err(pfilename)
        , m_filename(pfilename)
    {
    }

    int check_var(std::string_view var);
    int check_array(std::string_view array);
    int check_str(std::string_view str);
    int check_double_const(double d);

    size_t get_stack_size() const { return stack_size; }
    size_t get_stack_size_and_append(size_t length_to_append);

    void error_on_undefined(std::shared_ptr<ast::Var> var_id);
    void error_on_wrong_type(std::shared_ptr<ast::Var> var_id, var_type tp);

private:
    std::string_view m_filename;
    int body_id = BODY_ID_START;
    size_t stack_size = 0;
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
