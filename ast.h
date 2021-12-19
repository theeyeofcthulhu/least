#ifndef AST_H_
#define AST_H_

#include <iostream>
#include <memory>

#include "dictionary.h"
#include "lexer.h"
#include "util.h"

int check_var(std::string var, compile_info &c_info);
int check_str(std::string str, compile_info &c_info);

enum ts_class {
    T_IF,
    T_ELIF,
    T_ELSE,
    T_WHILE,
    T_CONST,
    T_CMP,
    T_FUNC,
    T_VAR,
    T_BODY,
    T_STR,
    T_LSTR,
    T_ARIT,
    T_INV,
};

class tree_node;
class tree_cmp;

class tree_node {
  public:
    int get_line() { return m_line; };
    virtual ts_class get_type() { return m_type; };

    tree_node(int line) : m_line(line) {}

  private:
    int m_line;
    static const ts_class m_type = T_INV;
};

class tree_body : public tree_node {
  public:
    std::shared_ptr<tree_body> parent;

    std::vector<std::shared_ptr<tree_node>> children;

    ts_class get_type() override { return m_type; };
    int get_body_id() { return m_body_id; };

    tree_body(int line, std::shared_ptr<tree_body> t_parent,
              compile_info &c_info)
        : tree_node(line), parent(t_parent) {
        m_body_id = c_info.get_next_body_id();
    }

  private:
    static const ts_class m_type = T_BODY;
    int m_body_id;
};

class tree_if : public tree_node {
  public:
    std::shared_ptr<tree_cmp> condition;
    std::shared_ptr<tree_body> body;
    std::shared_ptr<tree_node> elif;
    bool has_elif() { return elif != nullptr; }

    ts_class get_type() override { return m_type; };

    tree_if(int line, std::shared_ptr<tree_cmp> t_condition,
            std::shared_ptr<tree_body> t_body, bool is_elif)
        : tree_node(line), condition(t_condition), body(t_body),
          elif (nullptr) {
        m_type = is_elif ? T_ELIF : T_IF;
    }

  private:
    ts_class m_type;
};

class tree_else : public tree_node {
  public:
    std::shared_ptr<tree_body> body;

    ts_class get_type() override { return m_type; };

    tree_else(int line, std::shared_ptr<tree_body> t_body)
        : tree_node(line), body(t_body) {}

  private:
    static const ts_class m_type = T_ELSE;
};

class tree_while : public tree_node {
  public:
    std::shared_ptr<tree_cmp> condition;
    std::shared_ptr<tree_body> body;

    ts_class get_type() override { return m_type; };

    tree_while(int line, std::shared_ptr<tree_cmp> t_condition,
               std::shared_ptr<tree_body> t_body)
        : tree_node(line), condition(t_condition), body(t_body) {}

  private:
    static const ts_class m_type = T_WHILE;
};

class tree_const : public tree_node {
  public:
    int get_value() { return m_value; };

    ts_class get_type() override { return m_type; };

    tree_const(int line, int value) : tree_node(line), m_value(value) {}

  private:
    int m_value;
    static const ts_class m_type = T_CONST;
};

class tree_cmp : public tree_node {
  public:
    std::shared_ptr<tree_node> left;
    std::shared_ptr<tree_node> right;

    ts_class get_type() override { return m_type; };
    ecmp_operation get_cmp() { return m_cmp; };

    tree_cmp(int line, std::shared_ptr<tree_node> t_left,
             std::shared_ptr<tree_node> t_right, ecmp_operation cmp)
        : tree_node(line), left(t_left), right(t_right), m_cmp(cmp) {}

  private:
    ecmp_operation m_cmp;
    static const ts_class m_type = T_CMP;
};

class tree_func : public tree_node {
  public:
    std::vector<std::shared_ptr<tree_node>> args;

    ts_class get_type() override { return m_type; };
    efunc get_func() { return m_func; };

    tree_func(int line, efunc func) : tree_node(line), m_func(func) {}

  private:
    efunc m_func;
    static const ts_class m_type = T_FUNC;
};

class tree_var : public tree_node {
  public:
    ts_class get_type() override { return m_type; };
    int get_var_id() { return m_var_id; };

    tree_var(int line, int var_id) : tree_node(line), m_var_id(var_id) {}

  private:
    int m_var_id;
    static const ts_class m_type = T_VAR;
};

class tree_str : public tree_node {
  public:
    ts_class get_type() override { return m_type; };
    int get_str_id() { return m_str_id; };

    tree_str(int line, int str_id) : tree_node(line), m_str_id(str_id) {}

  private:
    int m_str_id;
    static const ts_class m_type = T_STR;
};

class tree_lstr : public tree_node {
  public:
    ts_class get_type() override { return m_type; };

    std::vector<std::shared_ptr<tree_node>> format;

    tree_lstr(int line, std::vector<std::shared_ptr<token>> ts,
              compile_info &c_info)
        : tree_node(line) {
        for (auto tk : ts) {
            switch (tk->get_type()) {
            case TK_STR:
            {
                std::shared_ptr<t_str> str =
                    std::dynamic_pointer_cast<t_str>(tk);
                format.push_back(std::make_shared<tree_str>(
                    line, check_str(str->get_str(), c_info)));
                break;
            }
            case TK_VAR:
            {
                std::shared_ptr<t_var> var =
                    std::dynamic_pointer_cast<t_var>(tk);
                format.push_back(std::make_shared<tree_var>(
                    line, check_var(var->get_name(), c_info)));
                break;
            }
            case TK_NUM:
            {
                std::shared_ptr<t_num> cnst =
                    std::dynamic_pointer_cast<t_num>(tk);
                format.push_back(
                    std::make_shared<tree_const>(line, cnst->get_num()));
                break;
            }
            default:
                c_info.err.error("Found invalid token in lstr in tree_lstr "
                                 "constructor: %d\n",
                                 tk->get_type());
                break;
            }
        }
    }

  private:
    static const ts_class m_type = T_LSTR;
};

class tree_arit : public tree_node {
  public:
    std::shared_ptr<tree_node> left;
    std::shared_ptr<tree_node> right;

    ts_class get_type() override { return m_type; };
    earit_operation get_arit() { return m_arit; };

    tree_arit(int line, std::shared_ptr<tree_node> t_left,
              std::shared_ptr<tree_node> t_right, earit_operation arit)
        : tree_node(line), left(t_left), right(t_right), m_arit(arit) {}

  private:
    earit_operation m_arit;
    static const ts_class m_type = T_ARIT;
};

void error_on_undefined(std::shared_ptr<tree_var> var_id,
                        struct compile_info &c_info);
std::shared_ptr<tree_node> get_last_if(std::shared_ptr<tree_if> if_node);
void tree_to_dot(std::shared_ptr<tree_body> root, std::string fn,
                 compile_info &c_info);
std::shared_ptr<tree_body>
tokens_to_ast(std::vector<std::shared_ptr<token>> tokens, compile_info &c_info);

#endif /* AST_H_ */
