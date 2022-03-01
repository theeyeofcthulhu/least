#ifndef LEXER_H_
#define LEXER_H_

#include <cassert>
#include <fmt/ostream.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "dictionary.hpp"

class CompileInfo;

namespace lexer {

enum token_type : int {
    TK_KEY,
    TK_ARIT,
    TK_CMP,
    TK_LOG,
    TK_STR,
    TK_LSTR,
    TK_CHAR,
    TK_NUM,
    TK_VAR,
    TK_ACCESS,
    TK_SEP,
    TK_CALL,
    TK_EOL,
    TK_INV,
};

class Token {
public:
    Token(int line)
        : m_line(line)
    {
    }
    int get_line() const { return m_line; };
    virtual token_type get_type() const { return m_type; };

private:
    static const token_type m_type = lexer::TK_INV;
    int m_line { 0 };
};

class Key : public Token {
public:
    Key(int line, keyword t_key)
        : Token(line)
        , m_key(t_key)
    {
    }
    keyword get_key() const { return m_key; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_KEY;
    keyword m_key;
};

class Arit : public Token {
public:
    Arit(int line, arit_op op)
        : Token(line)
        , m_op(op)
    {
    }
    arit_op get_op() const { return m_op; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_ARIT;
    arit_op m_op;
};

class Cmp : public Token {
public:
    Cmp(int line, cmp_op t_cmp)
        : Token(line)
        , m_cmp(t_cmp)
    {
    }
    cmp_op get_cmp() const { return m_cmp; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_CMP;
    cmp_op m_cmp;
};

class Log : public Token {
public:
    Log(int line, log_op t_log)
        : Token(line)
        , m_log(t_log)
    {
    }
    log_op get_log() const { return m_log; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_LOG;
    log_op m_log;
};

class Str : public Token {
public:
    Str(int line, const std::string& t_str)
        : Token(line)
        , m_str(t_str)
    {
    }
    std::string_view get_str() const { return m_str; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_STR;
    std::string m_str;
};

class Lstr : public Token {
public:
    Lstr(int line)
        : Token(line)
    {
    }
    std::vector<std::shared_ptr<Token>> ts;
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_LSTR;
};

class Num : public Token {
public:
    Num(int line, int t_num)
        : Token(line)
        , m_num(t_num)
    {
    }
    int get_num() const { return m_num; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_NUM;
    int m_num;
};

class Var : public Token {
public:
    Var(int line, const std::string_view name)
        : Token(line)
        , m_name(name)
    {
    }
    std::string_view get_name() const { return m_name; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_VAR;
    std::string_view m_name;
};

class Access : public Token {
public:
    Access(int line, std::string_view array_name, const std::vector<std::shared_ptr<Token>>& p_expr)
        : Token(line)
        , expr(p_expr)
        , m_array_name(array_name)
    {
    }
    std::string_view get_array_name() const { return m_array_name; };
    token_type get_type() const override { return m_type; };
    std::vector<std::shared_ptr<Token>> expr;

private:
    static const token_type m_type = lexer::TK_ACCESS;
    std::string_view m_array_name;
};

class Call : public Token {
public:
    Call(int line, value_func_id vfunc)
        : Token(line)
        , m_vfunc(vfunc)
    {
    }
    value_func_id get_value_func() const { return m_vfunc; };
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_CALL;
    value_func_id m_vfunc;
};

class Sep : public Token {
public:
    Sep(int line)
        : Token(line)
    {
    }
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_SEP;
};

class Eol : public Token {
public:
    Eol(int line)
        : Token(line)
    {
    }
    token_type get_type() const override { return m_type; };

private:
    static const token_type m_type = lexer::TK_EOL;
};

/*
 * Print the type of each token to stdout
 */
void debug_tokens(const std::vector<std::shared_ptr<Token>>& ts);

/*
 * Checks if the list of tokens supplies another function argument
 */
bool has_next_arg(const std::vector<std::shared_ptr<Token>>& ts, size_t& len);

/*
 * Lexes the source code in source to list of tokens
 */
std::vector<std::shared_ptr<Token>> do_lex(std::string_view source,
    CompileInfo& c_info,
    bool no_set_line = false);

const std::map<const size_t, token_type> token_type_enum_map = {
    std::make_pair(typeid(Key).hash_code(), lexer::TK_KEY),
    std::make_pair(typeid(Arit).hash_code(), lexer::TK_ARIT),
    std::make_pair(typeid(Cmp).hash_code(), lexer::TK_CMP),
    std::make_pair(typeid(Log).hash_code(), lexer::TK_LOG),
    std::make_pair(typeid(Str).hash_code(), lexer::TK_STR),
    std::make_pair(typeid(Lstr).hash_code(), lexer::TK_LSTR),
    std::make_pair(typeid(Num).hash_code(), lexer::TK_NUM),
    std::make_pair(typeid(Var).hash_code(), lexer::TK_VAR),
    std::make_pair(typeid(Access).hash_code(), lexer::TK_ACCESS),
    std::make_pair(typeid(Call).hash_code(), lexer::TK_CALL),
    std::make_pair(typeid(Sep).hash_code(), lexer::TK_SEP),
    std::make_pair(typeid(Eol).hash_code(), lexer::TK_EOL),
};

#define LEXER_SAFE_CAST(type, tk) lexer::safe_cast_core<type>((tk), __FILE__, __LINE__)

/*
 * Cast token to desired polymorphic subtype
 * Ensures that tk was declared as a type T originally
 */
template<typename T>
std::shared_ptr<T> safe_cast_core(std::shared_ptr<Token> tk, std::string_view file, int line)
{
    try {
        if (token_type_enum_map.at(typeid(T).hash_code()) != tk->get_type()) {
            fmt::print(std::cerr, "{}:{}: Invalid type-match: {}\nIn call: {}\n", file, line, typeid(T).name(), __PRETTY_FUNCTION__);
            std::exit(1);
        }
    } catch (std::out_of_range& e) {
        fmt::print(std::cerr, "Type '{}' not in map in lexer::safe_cast()\n", typeid(T).name());
        std::exit(1);
    }
    return std::dynamic_pointer_cast<T>(tk);
}

inline bool could_be_num(lexer::token_type tt)
{
    return tt == TK_NUM || tt == TK_VAR || tt == TK_CALL || tt == TK_ACCESS;
}

} // namespace lexer

#endif // LEXER_H_
