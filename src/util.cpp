#include "util.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include "ast.hpp"
#include "error.hpp"
#include "lexer.hpp"

/* Get the next word starting from index and store the next space
 * in spc_idx */
std::string_view get_next_word(std::string_view str, int index, size_t& spc_idx)
{
    std::string_view from_index = str.substr(index);

    spc_idx = from_index.find(' ');

    if (spc_idx == std::string_view::npos)
        spc_idx = from_index.size();

    return from_index.substr(0, spc_idx);
}

std::vector<std::string_view> split(std::string_view str, char delim)
{
    std::vector<std::string_view> res;
    size_t delim_idx, saved, length;
    bool looping = true;

    saved = 0;
    while (looping) {
        if ((delim_idx = str.find(delim, saved)) == std::string_view::npos)
            looping = false;

        length = delim_idx - saved;

        res.push_back(str.substr(saved, length));
        saved = delim_idx + 1;
    }

    return res;
}

/* Memory map a file and return the contents */
std::string read_source_code(std::string_view filename, CompileInfo& c_info)
{
    /* FIXME: there is a proposal in C++ to make std::ifstream constructable from
     * std::string_view, once this happens, simplify this. */
    const std::string temp(filename);

    std::ifstream fs(temp);
    c_info.err.on_false(fs.is_open(), "%: Could not open file\n", filename);

    std::string res;

    std::ostringstream ss;
    ss << fs.rdbuf();
    res = ss.str();

    fs.close();

    return res;
}

/* Get the index of the next token of type 'ty' on line starting from start.
 * Return ts.size() on failure. */
size_t next_of_type_on_line(const std::vector<std::shared_ptr<lexer::Token>>& ts,
    size_t start,
    lexer::token_type ty)
{
    for (size_t i = start; i < ts.size(); i++) {
        if (ts[i]->get_type() == lexer::TK_EOL && ty != lexer::TK_EOL) {
            return ts.size();
        } else if (ts[i]->get_type() == ty) {
            return i;
        }
    }

    return ts.size();
}

/* If a var is defined: return its index
 * else:                add a new variable to c_info's known_vars */
int CompileInfo::check_var(std::string_view var)
{
    for (size_t i = 0; i < known_vars.size(); i++) {
        if (var == known_vars[i].name)
            return i;
    }

    known_vars.push_back({ var, V_UNSURE, false });

    return known_vars.size() - 1;
}

/* Same as check_var but with string */
int CompileInfo::check_str(std::string_view str)
{
    for (size_t i = 0; i < known_string.size(); i++) {
        if (str == known_string[i])
            return i;
    }

    known_string.push_back(str);

    return known_string.size() - 1;
}

void CompileInfo::error_on_undefined(std::shared_ptr<ast::Var> var_id)
{
    err.on_false(known_vars[var_id->get_var_id()].defined,
        "Variable '%' is undefined at this time\n", known_vars[var_id->get_var_id()].name);
}

void CompileInfo::error_on_wrong_type(std::shared_ptr<ast::Var> var_id, var_type tp)
{
    err.on_false(known_vars[var_id->get_var_id()].type == tp, "Expected '%' to be type '%'\n",
        known_vars[var_id->get_var_id()].name, var_type_str_map.at(tp));
}

Filename::Filename(std::string_view fn)
    : m_filename(fn)
{
    size_t dot = fn.find_last_of('.');
    if (dot == std::string_view::npos)
        std::cout << "TODO: handle files without dots\n";

    m_noext = fn.substr(0, dot);
}

std::string Filename::extension(std::string_view ext) const
{
    if (ext.empty())
        return std::string(m_noext);

    std::stringstream res;
    res << m_noext << ext;

    return res.str();
}
