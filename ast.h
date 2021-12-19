#ifndef AST_H_
#define AST_H_

#include <iostream>

#include "dictionary.h"
#include "lexer.h"
#include "util.h"

int check_var(std::string var, compile_info& c_info);
int check_str(std::string str, compile_info& c_info);

enum ts_class{
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

class tree_node{
public:
    int get_line() { return m_line; };
    virtual ts_class get_type() { return m_type; };

    tree_node(int line) : m_line(line)
    {}
private:
    int m_line;
    static const ts_class m_type = T_INV;
};

class tree_body : public tree_node{
public:
    tree_body* parent;

    std::vector<tree_node*> children;

    ts_class get_type() override { return m_type; };
    int get_body_id() { return m_body_id; };

    tree_body(int line, tree_body* t_parent, compile_info& c_info) : tree_node(line), parent(t_parent)
    {
        m_body_id = c_info.get_next_body_id();
    }
private:
    static const ts_class m_type = T_BODY;
    int m_body_id;
};

class tree_if : public tree_node{
public:
    tree_cmp* condition;
    tree_body* body;
    tree_node* elif;
    bool has_elif() {
        return elif != nullptr;
    }

    ts_class get_type() override { return m_type; };

    tree_if(int line, tree_cmp* t_condition, tree_body* t_body, bool is_elif) : tree_node(line), condition(t_condition), body(t_body), elif(nullptr)
    {
        m_type = is_elif ? T_ELIF : T_IF;
    }
private:
    ts_class m_type;
};

class tree_else : public tree_node{
public:
    tree_body* body;

    ts_class get_type() override { return m_type; };

    tree_else(int line, tree_body* t_body) : tree_node(line), body(t_body)
    {}
private:
    static const ts_class m_type = T_ELSE;
};

class tree_while : public tree_node{
public:
    tree_cmp* condition;
    tree_body* body;

    ts_class get_type() override { return m_type; };

    tree_while(int line, tree_cmp* t_condition, tree_body* t_body) : tree_node(line), condition(t_condition), body(t_body)
    {}
private:
    static const ts_class m_type = T_WHILE;
};

class tree_const : public tree_node{
public:
    int get_value() { return m_value; };

    ts_class get_type() override { return m_type; };

    tree_const(int line, int value) : tree_node(line), m_value(value)
    {}
private:
    int m_value;
    static const ts_class m_type = T_CONST;
};

class tree_cmp : public tree_node{
public:
    tree_node* left;
    tree_node* right;

    ts_class get_type() override { return m_type; };
    ecmp_operation get_cmp() { return m_cmp; };

    tree_cmp(int line, tree_node* t_left, tree_node* t_right, ecmp_operation cmp) : tree_node(line), left(t_left), right(t_right), m_cmp(cmp)
    {}
private:
    ecmp_operation m_cmp;
    static const ts_class m_type = T_CMP;
};

class tree_func : public tree_node{
public:
    std::vector<tree_node*> args;

    ts_class get_type() override { return m_type; };
    efunc get_func() { return m_func; };

    tree_func(int line, efunc func) : tree_node(line), m_func(func)
    {}
private:
    efunc m_func;
    static const ts_class m_type = T_FUNC;
};

class tree_var : public tree_node{
public:
    ts_class get_type() override { return m_type; };
    int get_var_id() { return m_var_id; };

    tree_var(int line, int var_id) : tree_node(line), m_var_id(var_id)
    {}
private:
    int m_var_id;
    static const ts_class m_type = T_VAR;
};

class tree_str : public tree_node{
public:
    ts_class get_type() override { return m_type; };
    int get_str_id() { return m_str_id; };

    tree_str(int line, int str_id) : tree_node(line), m_str_id(str_id)
    {}
private:
    int m_str_id;
    static const ts_class m_type = T_STR;
};

class tree_lstr : public tree_node{
public:
    ts_class get_type() override { return m_type; };

    std::vector<tree_node*> format;

    tree_lstr(int line, std::vector<token*> ts, compile_info& c_info) : tree_node(line)
    {
        for(auto tk : ts){
            switch(tk->get_type()){
                case TK_STR:
                {
                    t_str* str = dynamic_cast<t_str*>(tk);
                    format.push_back(new tree_str(line, check_str(str->get_str(), c_info)));
                    break;
                }
                case TK_VAR:
                {
                    t_var* var = dynamic_cast<t_var*>(tk);
                    format.push_back(new tree_var(line, check_var(var->get_name(), c_info)));
                    break;
                }
                case TK_NUM:
                {
                    t_num* cnst = dynamic_cast<t_num*>(tk);
                    format.push_back(new tree_const(line, cnst->get_num()));
                    break;
                }
                default:
                    c_info.err.error("Found invalid token in lstr in tree_lstr constructor: %d\n", tk->get_type());
                    break;
            }
        }
    }
private:
    static const ts_class m_type = T_LSTR;
};

class tree_arit : public tree_node{
public:
    tree_node* left;
    tree_node* right;

    ts_class get_type() override { return m_type; };
    earit_operation get_arit() { return m_arit; };

    tree_arit(int line, tree_node* t_left, tree_node* t_right, earit_operation arit) : tree_node(line), left(t_left), right(t_right), m_arit(arit)
    {}
private:
    earit_operation m_arit;
    static const ts_class m_type = T_ARIT;
};


// typedef struct tree_node_s tree_node;

// struct tree_node_s{
//     enum ts_class cls;
//     union{
//         struct {
//             tree_node* condition;
//             tree_node* body;
//             tree_node* elif;
//         }t_if;
//         struct {
//             tree_node* body;
//         }t_else;
//         struct {
//             tree_node* condition;
//             tree_node* body;
//         }t_while;
//         int value;
//         struct {
//             ecmp_operation cmp;
//             tree_node* left;
//             tree_node* right;
//         }sides;
//         struct {
//             efunc func;
//             tree_node** args;
//             int n_args;
//         }func_args;
//         int n_var;
//         struct {
//             tree_node** children;
//             int n_children;
//             int body_id;
//         }body;
//         int n_str;
//         struct {
//             tree_node** format;
//             int n_format;
//         } lstr;
//         struct {
//             earit_operation arit_op;
//             tree_node* left;
//             tree_node* right;
//         }arit;
//     }op;
//     tree_node* parent;
// };

void error_on_undefined(tree_var* var_id, struct compile_info& c_info);
tree_node* get_last_if(tree_if* if_node);
void tree_to_dot(tree_body* root, std::string fn, compile_info& c_info);
tree_body* tokens_to_ast(std::vector<token*> tokens, compile_info& c_info);

#endif /* AST_H_ */
