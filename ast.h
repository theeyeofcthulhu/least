#ifndef AST_H_
#define AST_H_

#include "dictionary.h"
#include "lexer.h"
#include "util.h"

typedef struct tree_node_s tree_node;

struct tree_node_s{
    enum{T_IF, T_ELIF, T_ELSE, T_CONST, T_CMP, T_FUNC, T_VAR, T_BODY, T_STR, T_ARIT} class;
    union{
        struct {
            tree_node* condition;
            tree_node* body;
            tree_node* elif;
        }t_if;
        struct {
            tree_node* body;
        }t_else;
        int value;
        struct {
            ecmp_operation cmp;
            tree_node* left;
            tree_node* right;
        }sides;
        struct {
            efunc func;
            tree_node** args;
            int n_args;
        }func_args;
        int n_var;
        struct {
            tree_node** children;
            int n_children;
            int body_id;
        }body;
        int n_str;
        struct {
            earit_operation arit_op;
            tree_node* left;
            tree_node* right;
        }arit;
    }op;
    tree_node* parent;
};

bool check_defined(struct compile_info c_info, int id);
tree_node* get_last_if(tree_node* if_node);
void tree_to_dot(tree_node* root, char* fn);
tree_node* tokens_to_ast(token** tokens, int token_len, struct compile_info* c_info);

#endif /* AST_H_ */
