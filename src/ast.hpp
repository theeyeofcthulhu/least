#ifndef AST_H_
#define AST_H_

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <vector>

#include "dictionary.hpp"

class CompileInfo;

namespace lexer {
class Token;
}

namespace ast {

enum ts_class {
    T_BASE,
    T_IF,
    T_ELSE,
    T_WHILE,
    T_CONST,
    T_CMP,
    T_LOG,
    T_FUNC,
    T_VFUNC,
    T_VAR,
    T_BODY,
    T_STR,
    T_LSTR,
    T_ARIT,
    T_NUM_GENERAL, /* Stands for int var, const and arit, i.e. anything that
                      would be a number */
};

class Cmp;

class Node {
   public:
    int get_line() const { return m_line; };
    virtual ts_class get_type() const { return m_type; };

    Node(int line) : m_line(line) {}

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

    Body(int line, std::shared_ptr<Body> t_parent, CompileInfo &c_info);

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
        : Node(line), condition(t_condition), body(t_body), elif (nullptr), m_is_elif(is_elif)
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

    Else(int line, std::shared_ptr<Body> t_body) : Node(line), body(t_body) {}

   private:
    static const ts_class m_type = T_ELSE;
};

class While : public Node {
   public:
    std::shared_ptr<Node> condition;
    std::shared_ptr<Body> body;

    ts_class get_type() const override { return m_type; };

    While(int line, std::shared_ptr<Node> t_condition, std::shared_ptr<Body> t_body)
        : Node(line), condition(t_condition), body(t_body)
    {
    }

   private:
    static const ts_class m_type = T_WHILE;
};

class Const : public Node {
   public:
    int get_value() const { return m_value; };

    ts_class get_type() const override { return m_type; };

    Const(int line, int value) : Node(line), m_value(value) {}

   private:
    int m_value;
    static const ts_class m_type = T_CONST;
};

class Cmp : public Node {
   public:
    std::shared_ptr<Node> left;
    std::shared_ptr<Node> right;

    ts_class get_type() const override { return m_type; };
    cmp_op get_cmp() const { return m_cmp; };

    Cmp(int line, std::shared_ptr<Node> t_left, std::shared_ptr<Node> t_right, cmp_op t_cmp)
        : Node(line), left(t_left), right(t_right), m_cmp(t_cmp)
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
        : Node(line), left(t_left), right(t_right), m_log(t_log)
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

    Func(int line, func_id t_func) : Node(line), m_func(t_func) {}

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
        : Node(line), m_vfunc(t_vfunc), m_return_type(t_ret)
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

    Var(int line, int var_id) : Node(line), m_var_id(var_id) {}

   private:
    int m_var_id;
    static const ts_class m_type = T_VAR;
};

class Str : public Node {
   public:
    ts_class get_type() const override { return m_type; };
    int get_str_id() const { return m_str_id; };

    Str(int line, int str_id) : Node(line), m_str_id(str_id) {}

   private:
    int m_str_id;
    static const ts_class m_type = T_STR;
};

class Lstr : public Node {
   public:
    ts_class get_type() const override { return m_type; };
    std::vector<std::shared_ptr<Node>> format;

    Lstr(int line, const std::vector<std::shared_ptr<lexer::Token>> &ts, CompileInfo &c_info);

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
        : Node(line), left(t_left), right(t_right), m_arit(t_arit)
    {
    }

   private:
    arit_op m_arit;
    static const ts_class m_type = T_ARIT;
};

struct FunctionSpec {
    std::string name;
    size_t exp_arg_len;
    std::vector<ts_class> types;
    std::vector<var_type> info;
    std::vector<std::pair<size_t, var_type>> define;
    FunctionSpec(const std::string &t_name,
                 size_t t_len,
                 const std::vector<ts_class> &t_types,
                 const std::vector<var_type> &t_info,
                 const std::vector<std::pair<size_t, var_type>> &t_def)
        : name(t_name), exp_arg_len(t_len), types(t_types), info(t_info), define(t_def)
    {
    }
};

std::shared_ptr<Node> get_last_if(std::shared_ptr<If> if_node);
void check_correct_function_call(const FunctionSpec &spec,
                                 const std::vector<std::shared_ptr<Node>> &args,
                                 CompileInfo &c_info);
void tree_to_dot(std::shared_ptr<Body> root, std::string fn, CompileInfo &c_info);
std::shared_ptr<Body> gen_ast(const std::vector<std::shared_ptr<lexer::Token>> &tokens,
                              CompileInfo &c_info);

const std::map<const size_t, ts_class> tree_type_enum_map = {
    std::make_pair(typeid(Node).hash_code(), T_BASE),
    std::make_pair(typeid(If).hash_code(), T_IF),
    std::make_pair(typeid(Else).hash_code(), T_ELSE),
    std::make_pair(typeid(While).hash_code(), T_WHILE),
    std::make_pair(typeid(Const).hash_code(), T_CONST),
    std::make_pair(typeid(Cmp).hash_code(), T_CMP),
    std::make_pair(typeid(Log).hash_code(), T_LOG),
    std::make_pair(typeid(Func).hash_code(), T_FUNC),
    std::make_pair(typeid(VFunc).hash_code(), T_VFUNC),
    std::make_pair(typeid(Var).hash_code(), T_VAR),
    std::make_pair(typeid(Body).hash_code(), T_BODY),
    std::make_pair(typeid(Str).hash_code(), T_STR),
    std::make_pair(typeid(Lstr).hash_code(), T_LSTR),
    std::make_pair(typeid(Arit).hash_code(), T_ARIT),
};

/* Cast tree_node to desired polymorphic subtype
 * Ensures that tk was declared as a type T originally */
template <typename T>
std::shared_ptr<T> safe_cast(std::shared_ptr<Node> nd)
{
    try {
        assert(tree_type_enum_map.at(typeid(T).hash_code()) == nd->get_type());
    } catch (std::out_of_range &e) {
        std::cerr << "Type '" << typeid(T).name() << "' not in map in ast::safe_cast()\n";
        std::exit(1);
    }
    return std::dynamic_pointer_cast<T>(nd);
}

template <typename T>
std::shared_ptr<Node> to_base(std::shared_ptr<T> nd)
{
    return std::dynamic_pointer_cast<Node>(nd);
}

inline constexpr bool has_precedence(arit_op op)
{
    return op == DIV || op == MUL || op == MOD;
}

}  // namespace ast

#endif /* AST_H_ */
