#ifndef LEXER_H_
#define LEXER_H_

/* typedef struct token_s token; */

#include <string>
#include <vector>

#include "dictionary.h"
#include "util.h"
#include "lstring.h"

enum token_type : int{
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

class token{
public:
    token(int line) : m_line(line)
    {}
    int get_line() { return m_line; };
    virtual token_type get_type() { return m_type; };
private:
    static const token_type m_type = TK_INV;
    int m_line{0};
};

class t_key : public token {
public:
    t_key(int line, ekeyword key) : token(line), m_key(key)
    {}
    ekeyword get_key() { return m_key; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_KEY;
    ekeyword m_key;
};

class t_arit : public token {
public:
    t_arit(int line, earit_operation op) : token(line), m_op(op)
    {}
    earit_operation get_op() { return m_op; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_ARIT;
    earit_operation m_op;
};

class t_cmp : public token {
public:
    t_cmp(int line, ecmp_operation cmp) : token(line), m_cmp(cmp)
    {}
    ecmp_operation get_cmp() { return m_cmp; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_CMP;
    ecmp_operation m_cmp;
};

class t_log : public token {
public:
    t_log(int line, elogical_operation log) : token(line), m_log(log)
    {}
    elogical_operation get_log() { return m_log; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_LOG;
    elogical_operation m_log;
};

class t_str : public token {
public:
    t_str(int line, std::string str) : token(line), m_str(str)
    {}
    std::string get_str() { return m_str; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_STR;
    std::string m_str;
};

class t_lstr : public token {
public:
    t_lstr(int line, lstring* lstr) : token(line), m_lstr(lstr)
    {}
    lstring* get_lstr() { return m_lstr; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_LSTR;
    lstring* m_lstr;
};

class t_char : public token {
public:
    t_char(int line, char the_char) : token(line), m_the_char(the_char)
    {}
    char get_char() { return m_the_char; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_CHAR;
    char m_the_char;
};

class t_num : public token {
public:
    t_num(int line, int num) : token(line), m_num(num)
    {}
    int get_num() { return m_num; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_NUM;
    int m_num;
};

class t_var : public token {
public:
    t_var(int line, std::string name) : token(line), m_name(name)
    {}
    std::string get_name() { return m_name; };
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_VAR;
    std::string m_name;
};

class t_sep : public token {
public:
    t_sep(int line) : token(line)
    {}
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_SEP;
};

class t_eol : public token {
public:
    t_eol(int line) : token(line)
    {}
    token_type get_type() override { return m_type; };
private:
    static const token_type m_type = TK_EOL;
};

std::vector<token*> lex_source(std::string source, compile_info& c_info);
void free_tokens(token** ts, int len);
bool has_next_arg(std::vector<token*> ts, size_t& len);
void debug_token(token* token);

#endif // LEXER_H_
