#ifndef AST_H_
#define AST_H_

#include <cassert>
#include <fmt/ostream.h>
#include <iostream>
#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "dictionary.hpp"
#include "macros.hpp"
#include "maps.hpp"

class CompileInfo;

namespace lexer {
class Token;
}

namespace ast {

enum ts_class : int {
    T_BASE,
    T_IF,
    T_ELSE,
    T_WHILE,
    T_CONST,
    T_DOUBLE_CONST,
    T_CMP,
    T_LOG,
    T_FUNC,
    T_VFUNC,
    T_VAR,
    T_ACCESS,
    T_BODY,
    T_STR,
    T_LSTR,
    T_ARIT,
    T_INT_GENERAL, /* Stands for int var, const and arit, i.e. anything that
                      would be a number */
    T_DOUBLE_GENERAL,
    T_IN_MEMORY,
};

class Cmp;
class Arit;

class Node {
public:
    int get_line() const { return m_line; };
    virtual ts_class get_type() const { return m_type; };

    Node(int line)
        : m_line(line)
    {
    }

private:
    int m_line;
    static const ts_class m_type = T_BASE;
};

class Body : public Node {
public:
    std::shared_ptr<Body> parent;

    std::vector<std::shared_ptr<Node>> children;

    ts_class get_type() const override { return m_type; };
    int get_body_id() const { return m_body_id; };

    Body(int line, std::shared_ptr<Body> t_parent, int body_id)
        : Node(line)
        , parent(t_parent)
        , m_body_id(body_id)
    {}


private:
    static const ts_class m_type = T_BODY;
    int m_body_id;
};

class If : public Node {
public:
    std::shared_ptr<Node> condition;
    std::shared_ptr<Body> body;
    std::shared_ptr<Node> elif;
    bool has_elif() const { return elif != nullptr; }
    bool is_elif() const { return m_is_elif; }

    ts_class get_type() const override { return m_type; };

    If(int line, std::shared_ptr<Node> t_condition, std::shared_ptr<Body> t_body, bool is_elif)
        : Node(line)
        , condition(t_condition)
        , body(t_body)
        , elif (nullptr)
        , m_is_elif(is_elif)
    {
    }

private:
    static const ts_class m_type = T_IF;
    bool m_is_elif;
};

class Else : public Node {
public:
    std::shared_ptr<Body> body;

    ts_class get_type() const override { return m_type; };

    Else(int line, std::shared_ptr<Body> t_body)
        : Node(line)
        , body(t_body)
    {
    }

private:
    static const ts_class m_type = T_ELSE;
};

class While : public Node {
public:
    std::shared_ptr<Node> condition;
    std::shared_ptr<Body> body;

    ts_class get_type() const override { return m_type; };

    While(int line, std::shared_ptr<Node> t_condition, std::shared_ptr<Body> t_body)
        : Node(line)
        , condition(t_condition)
        , body(t_body)
    {
    }

private:
    static const ts_class m_type = T_WHILE;
};

class Const : public Node {
public:
    int get_value() const { return m_value; };

    ts_class get_type() const override { return m_type; };

    Const(int line, int value)
        : Node(line)
        , m_value(value)
    {
    }

private:
    int m_value;
    static const ts_class m_type = T_CONST;
};

class DoubleConst : public Node {
public:
    double get_value() const { return m_value; };

    ts_class get_type() const override { return m_type; };

    DoubleConst(int line, double value)
        : Node(line)
        , m_value(value)
    {
    }

private:
    double m_value;
    static const ts_class m_type = T_DOUBLE_CONST;
};

class Cmp : public Node {
public:
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    ts_class get_type() const override { return m_type; };
    cmp_op get_cmp() const { return m_cmp; };

    Cmp(int line, std::shared_ptr<Node> t_left, std::shared_ptr<Node> t_right, cmp_op t_cmp)
        : Node(line)
        , left(t_left)
        , right(t_right)
        , m_cmp(t_cmp)
    {
    }

private:
    cmp_op m_cmp;
    static const ts_class m_type = T_CMP;
};

class Log : public Node {
public:
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    ts_class get_type() const override { return m_type; };
    log_op get_log() const { return m_log; };

    Log(int line, std::shared_ptr<Node> t_left, std::shared_ptr<Node> t_right, log_op t_log)
        : Node(line)
        , left(t_left)
        , right(t_right)
        , m_log(t_log)
    {
    }

private:
    log_op m_log;
    static const ts_class m_type = T_LOG;
};

class Func : public Node {
public:
    std::vector<std::shared_ptr<Node>> args;

    ts_class get_type() const override { return m_type; };
    func_id get_func() const { return m_func; };

    Func(int line, func_id t_func)
        : Node(line)
        , m_func(t_func)
    {
    }

private:
    func_id m_func;
    static const ts_class m_type = T_FUNC;
};

class VFunc : public Node {
public:
    // TODO: args?

    ts_class get_type() const override { return m_type; };
    value_func_id get_value_func() const { return m_vfunc; };
    var_type get_return_type() const { return m_return_type; };

    VFunc(int line, value_func_id t_vfunc, var_type t_ret)
        : Node(line)
        , m_vfunc(t_vfunc)
        , m_return_type(t_ret)
    {
    }

private:
    value_func_id m_vfunc;
    var_type m_return_type;
    static const ts_class m_type = T_VFUNC;
};

class Var : public Node {
public:
    ts_class get_type() const override { return m_type; };

    int get_var_id() const { return m_var_id; };

    Var(int line, int var_id)
        : Node(line)
        , m_var_id(var_id)
    {
    }

private:
    int m_var_id;
    static const ts_class m_type = T_VAR;
};

class Access : public Node {
public:
    ts_class get_type() const override { return m_type; };

    int get_array_id() const { return m_array_id; };
    std::shared_ptr<Node> index;

    Access(int line, int array_id, std::shared_ptr<Node> p_index)
        : Node(line)
        , index(p_index)
        , m_array_id(array_id)
    {
    }

    /* TODO: types for arrays */

private:
    int m_array_id;
    static const ts_class m_type = T_ACCESS;
};

class Str : public Node {
public:
    ts_class get_type() const override { return m_type; };
    int get_str_id() const { return m_str_id; };

    Str(int line, int str_id)
        : Node(line)
        , m_str_id(str_id)
    {
    }

private:
    int m_str_id;
    static const ts_class m_type = T_STR;
};

class Lstr : public Node {
public:
    ts_class get_type() const override { return m_type; };
    std::vector<std::shared_ptr<Node>> format;

    Lstr(int line, std::vector<std::shared_ptr<Node>> p_format)
        : Node(line)
        , format(p_format)
    {}

private:
    static const ts_class m_type = T_LSTR;
};

class Arit : public Node {
public:
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    ts_class get_type() const override { return m_type; };
    arit_op get_arit() const { return m_arit; };

    Arit(int line, std::shared_ptr<Node> t_left, std::shared_ptr<Node> t_right, arit_op t_arit)
        : Node(line)
        , left(t_left)
        , right(t_right)
        , m_arit(t_arit)
    {
    }

private:
    arit_op m_arit;
    static const ts_class m_type = T_ARIT;
};


class AstContext {
public:

    /* Generate an abstract syntax tree from tokens and return the root */
    std::shared_ptr<Body> gen_ast();

    // NOTE: should root be saved internally
    /* Write a graphviz representation of the AST in root to fn */
    void tree_to_dot(std::shared_ptr<Body> root, std::string_view fn);

    AstContext(const std::vector<std::shared_ptr<lexer::Token>>& tokens, CompileInfo &c_info)
        : m_tokens(tokens)
        , m_c_info(c_info)
    {}

private:
    std::shared_ptr<ast::Node> node_from_numeric_token(std::shared_ptr<lexer::Token> tk);

    void ensure_arit_correctness(const std::vector<std::shared_ptr<lexer::Token>>& ts);
    std::shared_ptr<Node> parse_arit_expr(const std::vector<std::shared_ptr<lexer::Token>>& ts);
    std::shared_ptr<Node> parse_logical(size_t& i);
    std::shared_ptr<Cmp> parse_condition(const std::vector<std::shared_ptr<lexer::Token>>& ts);
    std::shared_ptr<If> parse_condition_to_if(size_t& i, std::shared_ptr<Body> root, bool is_elif);
    std::shared_ptr<While> parse_condition_to_while(size_t& i, std::shared_ptr<Body> root);

    // Constructors require additional information/calculations
    std::shared_ptr<Lstr> make_lstr(int line, const std::vector<std::shared_ptr<lexer::Token>>& ts);
    std::shared_ptr<Body> make_body(int line, std::shared_ptr<Body> t_parent);

    void tree_to_dot_core(std::shared_ptr<Node> root, int& node, int& tbody_id, int parent_body_id, std::ofstream& dot);

    const std::vector<std::shared_ptr<lexer::Token>>& m_tokens;
    CompileInfo& m_c_info;
};

/*
* Traverse the pointers to elifs/elses on if_node until the
* last one
*/
std::shared_ptr<Node> get_last_if(std::shared_ptr<If> if_node);

#define AST_SAFE_CAST(type, tk) ast::safe_cast_core<type>((tk), __FILE__, __LINE__)

/*
 * Cast tree_node to desired polymorphic subtype
 * Ensures that tk was declared as a type T originally
 */
template<typename T>
std::shared_ptr<T> safe_cast_core(std::shared_ptr<Node> nd, std::string_view file, int line)
{
    try {
        if (tree_type_enum_map.at(typeid(T).hash_code()) != nd->get_type()) {
            fmt::print(std::cerr, "{}:{}: Invalid type-match: {}\nIn call: {}\n", file, line, typeid(T).name(), __PRETTY_FUNCTION__);
            std::exit(1);
        }
    } catch (std::out_of_range& e) {
        fmt::print(std::cerr, "Type '{}' not in map in lexer::safe_cast()\n", typeid(T).name());
        std::exit(1);
    }
    return std::dynamic_pointer_cast<T>(nd);
}

/*
 * Cast a subclass tree node back to Node
 */
template<typename T>
std::shared_ptr<Node> to_base(std::shared_ptr<T> nd)
{
    return std::dynamic_pointer_cast<Node>(nd);
}

inline bool has_precedence(arit_op op)
{
    return op == DIV || op == MUL || op == MOD;
}

inline bool could_be_num(ts_class type)
{
    return type == ast::T_ARIT || type == ast::T_CONST || type == ast::T_VFUNC || type == ast::T_VAR || type == ast::T_ACCESS || type == ast::T_DOUBLE_CONST;
}

} // namespace ast

#endif /* AST_H_ */
