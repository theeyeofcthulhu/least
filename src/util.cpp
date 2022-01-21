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
std::string get_next_word(const std::string& str, int index, size_t& spc_idx)
{
    std::string substr = str.substr(index, std::string::npos);

    spc_idx = substr.find(' ');

    if (spc_idx == std::string::npos)
        spc_idx = substr.size();

    std::string res = substr.substr(0, spc_idx);
    return res;
}

std::vector<std::string> split(const std::string& str, char delim)
{
    std::vector<std::string> res;

    std::stringstream lines(str);

    std::string ln;

    while (std::getline(lines, ln, delim)) {
        res.push_back(ln);
    }

    return res;
}

/* Memory map a file and return the contents */
std::string read_source_code(const std::string& filename, CompileInfo& c_info)
{
    int input_file = open(filename.c_str(), O_RDONLY);
    c_info.err.on_true(input_file < 0, "Could not open file '%'\n", filename);

    struct stat file_stat;
    c_info.err.on_true((fstat(input_file, &file_stat)) < 0, "Could not stat file '%'\n", filename);

    int input_size = file_stat.st_size;

    char* c_res = (char*)mmap(nullptr, input_size, PROT_READ, MAP_PRIVATE, input_file, 0);
    c_info.err.on_true(c_res == MAP_FAILED, "Failed to map file '%'\n", filename);

    std::string result(c_res, input_size);

    munmap(c_res, file_stat.st_size);
    close(input_file);

    return result;
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
int CompileInfo::check_var(const std::string& var)
{
    for (size_t i = 0; i < known_vars.size(); i++)
        if (var == known_vars[i].name)
            return i;

    known_vars.push_back({ var, V_UNSURE, false });

    return known_vars.size() - 1;
}

/* Same as check_var but with string */
int CompileInfo::check_str(const std::string& str)
{
    for (size_t i = 0; i < known_string.size(); i++)
        if (str == known_string[i])
            return i;

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

Filename::Filename(const std::string& fn)
    : m_filename(fn)
{
    size_t dot = fn.find_last_of('.');
    if (dot == std::string::npos)
        std::cout << "TODO: handle files without dots\n";

    m_noext = fn.substr(0, dot);
}

std::string Filename::extension(const std::string& ext) const
{
    if (ext.empty())
        return m_noext;

    std::string result = m_noext;
    return result.append(ext);
}
