#ifndef AST_H_
#define AST_H_

#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "util.hpp"

namespace lexer {
class token;
}

namespace ast {

enum ts_class {
    T_BASE,
    T_IF,
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
    T_NUM_GENERAL, /* Stands for int var, const and arit, i.e. anything that
                      would be a number */
};

class node;
class cmp;

class node {
  public:
    int get_line() { return m_line; };
    virtual ts_class get_type() { return m_type; };

    node(int line) : m_line(line) {}

  private:
    int m_line;
    static const ts_class m_type = T_BASE;
};

class n_body : public node {
  public:
    std::shared_ptr<n_body> parent;

    std::vector<std::shared_ptr<node>> children;

    ts_class get_type() override { return m_type; };
    int get_body_id() { return m_body_id; };

    n_body(int line, std::shared_ptr<n_body> t_parent, compile_info &c_info)
        : node(line), parent(t_parent)
    {
        m_body_id = c_info.get_next_body_id();
    }

  private:
    static const ts_class m_type = T_BODY;
    int m_body_id;
};

class n_if : public node {
  public:
    std::shared_ptr<cmp> condition;
    std::shared_ptr<n_body> body;
    std::shared_ptr<node> elif;
    bool has_elif() { return elif != nullptr; }
    bool is_elif() { return m_is_elif; }

    ts_class get_type() override { return m_type; };

    n_if(int line, std::shared_ptr<cmp> t_condition,
         std::shared_ptr<n_body> t_body, bool is_elif)
        : node(line), condition(t_condition), body(t_body), elif (nullptr),
          m_is_elif(is_elif)
    {
    }

  private:
    static const ts_class m_type = T_IF;
    bool m_is_elif;
};

class n_else : public node {
  public:
    std::shared_ptr<n_body> body;

    ts_class get_type() override { return m_type; };

    n_else(int line, std::shared_ptr<n_body> t_body) : node(line), body(t_body)
    {
    }

  private:
    static const ts_class m_type = T_ELSE;
};

class n_while : public node {
  public:
    std::shared_ptr<cmp> condition;
    std::shared_ptr<n_body> body;

    ts_class get_type() override { return m_type; };

    n_while(int line, std::shared_ptr<cmp> t_condition,
            std::shared_ptr<n_body> t_body)
        : node(line), condition(t_condition), body(t_body)
    {
    }

  private:
    static const ts_class m_type = T_WHILE;
};

class n_const : public node {
  public:
    int get_value() { return m_value; };

    ts_class get_type() override { return m_type; };

    n_const(int line, int value) : node(line), m_value(value) {}

  private:
    int m_value;
    static const ts_class m_type = T_CONST;
};

class cmp : public node {
  public:
    std::shared_ptr<node> left;
    std::shared_ptr<node> right;

    ts_class get_type() override { return m_type; };
    ecmp_operation get_cmp() { return m_cmp; };

    cmp(int line, std::shared_ptr<node> t_left, std::shared_ptr<node> t_right,
        ecmp_operation t_cmp)
        : node(line), left(t_left), right(t_right), m_cmp(t_cmp)
    {
    }

  private:
    ecmp_operation m_cmp;
    static const ts_class m_type = T_CMP;
};

class func : public node {
  public:
    std::vector<std::shared_ptr<node>> args;

    ts_class get_type() override { return m_type; };
    efunc get_func() { return m_func; };

    func(int line, efunc t_func) : node(line), m_func(t_func) {}

  private:
    efunc m_func;
    static const ts_class m_type = T_FUNC;
};

class var : public node {
  public:
    ts_class get_type() override { return m_type; };
    int get_var_id() { return m_var_id; };

    var(int line, int var_id) : node(line), m_var_id(var_id) {}

  private:
    int m_var_id;
    static const ts_class m_type = T_VAR;
};

class str : public node {
  public:
    ts_class get_type() override { return m_type; };
    int get_str_id() { return m_str_id; };

    str(int line, int str_id) : node(line), m_str_id(str_id) {}

  private:
    int m_str_id;
    static const ts_class m_type = T_STR;
};

class lstr : public node {
  public:
    ts_class get_type() override { return m_type; };
    std::vector<std::shared_ptr<node>> format;

    lstr(int line, std::vector<std::shared_ptr<lexer::token>> ts,
         compile_info &c_info);

  private:
    static const ts_class m_type = T_LSTR;
};

class arit : public node {
  public:
    std::shared_ptr<node> left;
    std::shared_ptr<node> right;

    ts_class get_type() override { return m_type; };
    earit_operation get_arit() { return m_arit; };

    arit(int line, std::shared_ptr<node> t_left, std::shared_ptr<node> t_right,
         earit_operation t_arit)
        : node(line), left(t_left), right(t_right), m_arit(t_arit)
    {
    }

  private:
    earit_operation m_arit;
    static const ts_class m_type = T_ARIT;
};

std::shared_ptr<node> get_last_if(std::shared_ptr<n_if> if_node);
void tree_to_dot(std::shared_ptr<n_body> root, std::string fn,
                 compile_info &c_info);
std::shared_ptr<n_body>
gen_ast(std::vector<std::shared_ptr<lexer::token>> tokens,
        compile_info &c_info);

const std::map<const size_t, ts_class> tree_type_enum_map = {
    std::make_pair(typeid(node).hash_code(), T_BASE),
    std::make_pair(typeid(n_if).hash_code(), T_IF),
    std::make_pair(typeid(n_else).hash_code(), T_ELSE),
    std::make_pair(typeid(n_while).hash_code(), T_WHILE),
    std::make_pair(typeid(n_const).hash_code(), T_CONST),
    std::make_pair(typeid(cmp).hash_code(), T_CMP),
    std::make_pair(typeid(func).hash_code(), T_FUNC),
    std::make_pair(typeid(var).hash_code(), T_VAR),
    std::make_pair(typeid(n_body).hash_code(), T_BODY),
    std::make_pair(typeid(str).hash_code(), T_STR),
    std::make_pair(typeid(lstr).hash_code(), T_LSTR),
    std::make_pair(typeid(arit).hash_code(), T_ARIT),
};

/* Cast tree_node to desired polymorphic subtype
 * Ensures that tk was declared as a type T originally */
template <typename T> std::shared_ptr<T> safe_cast(std::shared_ptr<node> nd)
{
    assert(tree_type_enum_map.at(typeid(T).hash_code()) == nd->get_type());
    return std::dynamic_pointer_cast<T>(nd);
}

template <typename T> std::shared_ptr<node> to_base(std::shared_ptr<T> nd)
{
    return std::dynamic_pointer_cast<node>(nd);
}

void check_correct_function_call(
    std::string name, std::vector<std::shared_ptr<node>> args, size_t arg_len,
    std::vector<ts_class> types, compile_info &c_info,
    std::vector<var_type> info = std::vector<var_type>(),
    std::vector<std::pair<size_t, var_type>> define =
        std::vector<std::pair<size_t, var_type>>());

} // namespace ast

#endif /* AST_H_ */
