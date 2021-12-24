#ifndef LEXER_H_
#define LEXER_H_

/* typedef struct token_s token; */

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "dictionary.hpp"

class compile_info;

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
    TK_SEP,
    TK_EOL,
    TK_INV,
};

class token {
  public:
    token(int line) : m_line(line) {}
    int get_line() { return m_line; };
    virtual token_type get_type() { return m_type; };

  private:
    static const token_type m_type = lexer::TK_INV;
    int m_line{0};
};

class key : public token {
  public:
    key(int line, ekeyword t_key) : token(line), m_key(t_key) {}
    ekeyword get_key() { return m_key; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_KEY;
    ekeyword m_key;
};

class arit : public token {
  public:
    arit(int line, earit_operation op) : token(line), m_op(op) {}
    earit_operation get_op() { return m_op; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_ARIT;
    earit_operation m_op;
};

class cmp : public token {
  public:
    cmp(int line, ecmp_operation t_cmp) : token(line), m_cmp(t_cmp) {}
    ecmp_operation get_cmp() { return m_cmp; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_CMP;
    ecmp_operation m_cmp;
};

class log : public token {
  public:
    log(int line, elogical_operation t_log) : token(line), m_log(t_log) {}
    elogical_operation get_log() { return m_log; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_LOG;
    elogical_operation m_log;
};

class str : public token {
  public:
    str(int line, std::string t_str) : token(line), m_str(t_str) {}
    std::string get_str() { return m_str; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_STR;
    std::string m_str;
};

class lstr : public token {
  public:
    lstr(int line) : token(line) {}
    std::vector<std::shared_ptr<token>> ts;
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_LSTR;
};

class t_char : public token {
  public:
    t_char(int line, char the_char) : token(line), m_the_char(the_char) {}
    char get_char() { return m_the_char; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_CHAR;
    char m_the_char;
};

class num : public token {
  public:
    num(int line, int t_num) : token(line), m_num(t_num) {}
    int get_num() { return m_num; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_NUM;
    int m_num;
};

class var : public token {
  public:
    var(int line, std::string name) : token(line), m_name(name) {}
    std::string get_name() { return m_name; };
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_VAR;
    std::string m_name;
};

class sep : public token {
  public:
    sep(int line) : token(line) {}
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_SEP;
};

class eol : public token {
  public:
    eol(int line) : token(line) {}
    token_type get_type() override { return m_type; };

  private:
    static const token_type m_type = lexer::TK_EOL;
};

std::vector<std::shared_ptr<token>>
do_lex(std::string source, compile_info &c_info, bool no_set_line = false);
bool has_next_arg(std::vector<std::shared_ptr<token>> ts, size_t &len);
void debug_tokens(std::vector<std::shared_ptr<token>> ts);

const std::map<const size_t, token_type> token_type_enum_map = {
    std::make_pair(typeid(key).hash_code(), lexer::TK_KEY),
    std::make_pair(typeid(arit).hash_code(), lexer::TK_ARIT),
    std::make_pair(typeid(cmp).hash_code(), lexer::TK_CMP),
    std::make_pair(typeid(log).hash_code(), lexer::TK_LOG),
    std::make_pair(typeid(str).hash_code(), lexer::TK_STR),
    std::make_pair(typeid(lstr).hash_code(), lexer::TK_LSTR),
    std::make_pair(typeid(t_char).hash_code(), lexer::TK_CHAR),
    std::make_pair(typeid(num).hash_code(), lexer::TK_NUM),
    std::make_pair(typeid(var).hash_code(), lexer::TK_VAR),
    std::make_pair(typeid(sep).hash_code(), lexer::TK_SEP),
    std::make_pair(typeid(eol).hash_code(), lexer::TK_EOL),
};

/* Cast token to desired polymorphic subtype
 * Ensures that tk was declared as a type T originally */
template <typename T> std::shared_ptr<T> safe_cast(std::shared_ptr<token> tk)
{
    assert(token_type_enum_map.at(typeid(T).hash_code()) == tk->get_type());
    return std::dynamic_pointer_cast<T>(tk);
}

} // namespace lexer

#endif // LEXER_H_
