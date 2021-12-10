#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "lexer.h"
#include "error.h"
#include "ast.h"
#include "x86_64.h"

void tree_to_dot_core(tree_node* root, int* node, int* tbody_id, int parent_body_id, FILE* dot);

tree_node* create_body(tree_node* parent, struct compile_info* c_info){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_BODY;
    res->op.body.children = malloc(NODE_ARR_SZ * sizeof(tree_node));
    res->op.body.n_children = 0;
    res->op.body.body_id = c_info->body_id++;
    res->parent = parent;

    return res;
}

efunc ekeytoefunc(ekeyword key){
    switch(key){
        case K_PRINT:
            return PRINT;
        case K_UPRINT:
            return UPRINT;
        case K_FPRINT:
            return FPRINT;
        case K_EXIT:
            return EXIT;
        case K_READ:
            return READ;
        case K_SET:
            return SET;
        case K_PUTCHAR:
            return PUTCHAR;
        case K_INT:
            return INT;
        default:
            return EXIT;
    }
}

char* efunctostrfunc(efunc func){
    switch(func){
        case PRINT:
            return "print";
        case UPRINT:
            return "uprint";
        case FPRINT:
            return "fprint";
        case EXIT:
            return "exit";
        case READ:
            return "read";
        case SET:
            return "set";
        case PUTCHAR:
            return "putchar";
        case INT:
            return "int";
        default:
            return "Error";
    }
}

char* ecmptostrcmp(ecmp_operation func){
    switch(func){
        case EQUAL:
            return "==";
        case GREATER:
            return ">";
        case GREATER_OR_EQ:
            return ">=";
        case LESS:
            return "<";
        case LESS_OR_EQ:
            return "<=";
        case NOT_EQUAL:
            return "!=";
        default:
            return "Error";
    }
}

char* earittostrarit(earit_operation arit){
    switch(arit){
        case ADD:
            return "+";
        case DIV:
            return "/";
        case MOD:
            return "%";
        case MUL:
            return "*";
        case SUB:
            return "-";
        default:
            return "Error";
    }
}

tree_node* create_func(efunc func){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_FUNC;
    res->op.func_args.func = func;
    res->op.func_args.args = malloc(NODE_ARR_SZ * sizeof(tree_node));
    res->op.func_args.n_args = 0;

    return res;
}

tree_node* create_if(tree_node* condition, tree_node* body){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_IF;
    res->op.t_if.condition = condition;
    res->op.t_if.body = body;
    res->op.t_if.elif = NULL;

    return res;
}

tree_node* get_last_if(tree_node* if_node){
    tree_node* res = if_node;

    for(;; res = res->op.t_if.elif){
        if(res->class == T_ELSE || res->op.t_if.elif == NULL){
            return res;
        }
    }

    return NULL;
}

void if_to_elif(tree_node* if_node){
    assert(if_node->class == T_IF);
    if_node->class = T_ELIF;
}

tree_node* create_else(tree_node* body){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_ELSE;
    res->op.t_else.body = body;

    return res;
}

tree_node* create_elif(tree_node* condition, tree_node* body){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_ELIF;
    res->op.t_if.condition = condition;
    res->op.t_if.body = body;

    return res;
}

tree_node* create_condition(tree_node* left, tree_node* right, ecmp_operation cmp){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_CMP;
    res->op.sides.left = left;
    res->op.sides.right = right;
    res->op.sides.cmp = cmp;

    return res;
}

tree_node* create_const(int n){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_CONST;
    res->op.value = n;

    return res;
}

int check_var(char* var, struct compile_info* c_info){
    for(int i = 0; i < c_info->var_id; i++)
        if(strcmp(var, c_info->known_vars[i]) == 0)
            return i;

    c_info->known_vars[c_info->var_id] = var;

    return c_info->var_id++;
}

tree_node* create_var(char* var, struct compile_info* c_info){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_VAR;
    res->op.n_var = check_var(var, c_info);

    return res;
}

int check_str(char* str, struct compile_info* c_info){
    for(int i = 0; i < c_info->string_id; i++)
        if(strcmp(str, c_info->known_strings[i]) == 0)
            return i;

    c_info->known_strings[c_info->string_id] = str;

    return c_info->string_id++;
}

tree_node* create_str(char* str, struct compile_info* c_info){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_STR;
    res->op.n_str = check_str(str, c_info);

    return res;
}

tree_node* create_arit(earit_operation arit, tree_node* left, tree_node* right){
    tree_node* res = malloc(sizeof(tree_node));
    res->class = T_ARIT;
    res->op.arit.arit_op = arit;
    res->op.arit.left = left;
    res->op.arit.right = right;

    return res;
}

void append_child(tree_node* body, tree_node* child){
    assert(body->class == T_BODY);
    assert(body->op.body.n_children + 1 < NODE_ARR_SZ);

    body->op.body.children[body->op.body.n_children++] = child;
}

void append_arg(tree_node* body, tree_node* child){
    assert(body->class == T_FUNC);
    assert(body->op.func_args.n_args + 1 < NODE_ARR_SZ);

    body->op.func_args.args[body->op.func_args.n_args++] = child;
}

tree_node* node_from_var_or_const(token* tk, struct compile_info* c_info){
    compiler_error_on_false(tk->type == VAR || tk->type == NUMBER, tk->line + 1, "Trying to convert token, which is not a variable or a number, to a variable or a number\n");

    tree_node* res;

    if(tk->type == VAR){
        res = create_var(tk->value.name, c_info);
    }else if(tk->type == NUMBER){
        res = create_const(tk->value.num);
    }

    return res;
}

/* Parse arithmetic expression respecting precedence */
tree_node* parse_arit_expr(token** ts, int len, struct compile_info* c_info){
    tree_node* s2[NODE_ARR_SZ];
    int s2_sz = 0;

    earit_operation last_op = ARIT_OPERATION_ENUM_END;

    /* Group '/' and '*' expressions together and leave '+', '-' and constants in place */
    for(int i = 0; i < len; i++){
        /* If we have '/' or '*' next and are not currently an operator:
         * continue because we belong to that operator */
        earit_operation next_op = ARIT_OPERATION_ENUM_END;
        for(int j = i+1; j < len; j++){
            if(ts[j]->type == OPERATOR){
                next_op = ts[j]->value.arit_operator;
                break;
            }
        }
        if((next_op == MUL || next_op == DIV) && ts[i]->type != OPERATOR){
            continue;
        }

        if(ts[i]->type == OPERATOR){
            /* Make new multiplication */
            if(ts[i]->value.arit_operator == MUL || ts[i]->value.arit_operator == DIV){
                assert(i > 0 && i + 1 < len);
                assert(ts[i-1]->type == NUMBER || ts[i-1]->type == VAR);
                assert(ts[i+1]->type == NUMBER || ts[i+1]->type == VAR);

                if(last_op == MUL || last_op == DIV){
                    /* If we follow another multiplication:
                     * incorporate previous into ourselves and replace that previous one in the array */
                    tree_node* new = create_arit(ts[i]->value.arit_operator,
                                                 s2[s2_sz-1],
                                                 node_from_var_or_const(ts[i+1], c_info));
                    s2[s2_sz-1] = new;
                }else{
                    s2[s2_sz++] = create_arit(ts[i]->value.arit_operator,
                                            node_from_var_or_const(ts[i-1], c_info),
                                            node_from_var_or_const(ts[i+1], c_info));
                }
            }else{
                s2[s2_sz++] = create_arit(ts[i]->value.arit_operator, NULL, NULL);
            }
            last_op = ts[i]->value.arit_operator;
        }else{
            /* If we follow a multiplication:
             * we are already part of a multiplication and don't need us in the array */
            if(last_op == MUL || last_op == DIV)
                continue;
            s2[s2_sz++] = node_from_var_or_const(ts[i], c_info);
        }
    }

    if(s2_sz == 1){
        return s2[0];
    }

    tree_node* root = NULL;
    tree_node* current = NULL;

    /* Parse everything into a tree */
    for(int i = 0; i < s2_sz; i++){
        if(s2[i]->class == T_ARIT){
            if(s2[i]->op.arit.arit_op == ADD || s2[i]->op.arit.arit_op == SUB){
                if(!root){
                    root = create_arit(s2[i]->op.arit.arit_op, s2[i-1], NULL);
                    current = root;
                }else{
                    tree_node* new = create_arit(s2[i]->op.arit.arit_op, s2[i-1], NULL);
                    current->op.arit.right = new;
                    current = new;
                }

                if(i+1 == s2_sz-1){
                    current->op.arit.right = s2[i+1];
                }
            }
        }
    }

    return root;
}

tree_node* parse_comparison_to_if(token** ts, int* i, tree_node* root, struct compile_info* c_info){
    tree_node* left;
    tree_node* right;

    token* next;
    int nxt_tkn = 0;

    int operator_i = -1;

    token* comparator = NULL;

    tree_node* res;

    while((next = ts[nxt_tkn++])->type != EOL){
        if(next->type == COMPARATOR){
            compiler_error_on_true(comparator, ts[0]->line + 1, "Found two operators\n");
            comparator = next;
            operator_i = nxt_tkn;
        }
    }
    compiler_error_on_true(operator_i == 1, ts[0]->line + 1, "Expected constant, variable or arithmetic expression\n");

    if(operator_i != -1){
        left = parse_arit_expr(&ts[0], operator_i-1, c_info);

        int right_len = nxt_tkn - operator_i;

        right = parse_arit_expr(&ts[operator_i], right_len-1, c_info);

        *i += nxt_tkn - 1;

        res = create_if(create_condition(left, right, comparator->value.cmp_operator),
                        create_body(root, c_info));
    }else{
        left = parse_arit_expr(&ts[0], nxt_tkn-1, c_info);
        res = create_if(create_condition(left, NULL, CMP_OPERATION_ENUM_END),
                        create_body(root, c_info));
    }

    return res;
}

tree_node* tokens_to_ast(token** tokens, int token_len, struct compile_info* c_info){
    tree_node* root = create_body(NULL, c_info);
    tree_node* saved_root = root;

    tree_node* current_if = NULL;

    tree_node* blk_stk[NODE_ARR_SZ];
    int blk_stk_sz = 0;

    for(int i = 0; i < token_len; i++){
        switch(tokens[i]->type){
            case KEYWORD:
            {
                switch(tokens[i]->value.key){
                    case K_IF:
                    {
                        tree_node* new_if = parse_comparison_to_if(&tokens[i+1], &i, root, c_info);
                        append_child(root, new_if);

                        root = new_if->op.t_if.body;
                        current_if = new_if;

                        blk_stk[blk_stk_sz++] = new_if;
                        break;
                    }
                    case K_ELIF:
                    {
                        compiler_error_on_false(current_if, tokens[i]->line + 1, "Unexpected elif");

                        tree_node* new_if = parse_comparison_to_if(&tokens[i+1], &i, root, c_info);
                        if_to_elif(new_if);

                        current_if->op.t_if.elif = new_if;
                        root = new_if->op.t_if.body;
                        current_if = new_if;
                        break;
                    }
                    case K_ELSE:
                    {
                        compiler_error_on_false(current_if, tokens[i]->line + 1, "Unexpected else");
                        compiler_error_on_false(tokens[i+1]->type == EOL, tokens[i]->line + 1, "Else accepts no arguments");

                        tree_node* new_else = create_else(create_body(root, c_info));

                        current_if->op.t_if.elif = new_else;

                        root = new_else->op.t_else.body;
                        current_if = NULL;
                        break;
                    }
                    case K_PRINT:
                    case K_UPRINT:
                    case K_FPRINT:
                    case K_EXIT:
                    case K_READ:
                    case K_SET:
                    case K_PUTCHAR:
                    case K_INT:
                    {
                        tree_node* func = create_func(ekeytoefunc(tokens[i]->value.key));
                        append_child(root, func);

                        switch(func->op.func_args.func){
                            case UPRINT:
                                c_info->req_libs[LIB_UPRINT] = true;
                                break;
                            case PUTCHAR:
                                c_info->req_libs[LIB_PUTCHAR] = true;
                                break;
                            default:
                                break;
                        }

                        int nxt_tkn = i + 1;
                        token* next;
                        while((next = tokens[nxt_tkn++])->type != EOL){
                            //TODO: print source code of invalid argument
                            compiler_error_on_true(next->type == COMPARATOR || next->type == KEYWORD || next->type == LOGICAL, tokens[i]->line + 1, "Invalid function argument\n");
                            switch(next->type){
                                case VAR:
                                    append_arg(func, create_var(next->value.name, c_info));
                                    break;
                                case STRING:
                                    append_arg(func, create_str(next->value.string, c_info));
                                    break;
                                case NUMBER:
                                    append_arg(func, create_const(next->value.num));
                                    break;
                                //TODO: char
                                default:
                                    break;
                            }
                        }
                        break;
                    }
                    case K_END:
                        compiler_error_on_false(root->parent, tokens[i]->line + 1, "Unexpected end\n");
                        // compiler_error_on_true(current_if, tokens[i]->line + 1, "Unexpected end\n");
                        if(blk_stk[blk_stk_sz-1]){
                            switch(blk_stk[blk_stk_sz-1]->class){
                                case T_IF:
                                    root = blk_stk[blk_stk_sz-1]->op.t_if.body->parent;
                                    current_if = blk_stk[blk_stk_sz-2];
                                    break;
                                default:
                                    compiler_error(tokens[i]->line, "Exiting invalid block\n");
                                    break;
                            }
                            blk_stk_sz--;
                        }else
                            root = root->parent;
                        break;
                    case K_STR:
                    case K_WHILE:
                        compiler_error(tokens[i]->line + 1, "TODO: unimplemented\n");
                        break;
                    case K_NOKEY:
                        compiler_error(tokens[i]->line + 1, "Invalid instruction\n");
                        break;
                }
            }
            default:
                break;
        }
    }
    compiler_error_on_false(root == saved_root, 0, "Unresolved blocks\n");

    return saved_root;
}

bool check_defined(struct compile_info c_info, int id){
    return c_info.defined_vars[id];
}

void tree_to_dot(tree_node* root, char* fn){
    FILE* out = fopen(fn, "w");

    fprintf(out, "digraph AST {\n");

    int node = 0;

    assert(root->class == T_BODY);
    int tbody_id = root->op.body.body_id;

    tree_to_dot_core(root, &node, &tbody_id, 0, out);

    fprintf(out, "}\n");

    fclose(out);
}

void tree_to_dot_core(tree_node* root, int* node, int* tbody_id, int parent_body_id, FILE* dot){
    switch(root->class){
        case T_BODY:
            *tbody_id = root->op.body.body_id;
            fprintf(dot, "\tNode_%d [label=\"body %d\"]\n", *tbody_id, *tbody_id - NODE_ARR_SZ);
            // fprintf(dot, "\tNode_%d -> Node_%d [label=\"body %d\"]\n", *tbody_id, (*node), *tbody_id);
            for(int i = 0; i < root->op.body.n_children; i++){
                tree_to_dot_core(root->op.body.children[i], node, tbody_id, root->op.body.body_id, dot);
            }
            break;
        case T_IF:
        {
            fprintf(dot, "\tNode_%d [label=\"if\"]\n", ++(*node));
            int s_node = *node;
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"if > body\"]\n", *node, *tbody_id + 1);
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"body > if\"]\n", parent_body_id, *node);
            tree_to_dot_core(root->op.t_if.condition, node, tbody_id, parent_body_id, dot);
            tree_to_dot_core(root->op.t_if.body, node, tbody_id, parent_body_id, dot);
            if(root->op.t_if.elif)
                tree_to_dot_core(root->op.t_if.elif, node, tbody_id, s_node, dot);
            break;
        }
        case T_ELSE:
            fprintf(dot, "\tNode_%d [label=\"else\"]\n", ++(*node));
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"else > body\"]\n", *node, *tbody_id + 1);
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"body > else\"]\n", parent_body_id, *node);
            tree_to_dot_core(root->op.t_else.body, node, tbody_id, parent_body_id, dot);
            break;
        case T_ELIF:
        {
            fprintf(dot, "\tNode_%d [label=\"elif\"]\n", ++(*node));
            int s_node = *node;
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"elif > body\"]\n", *node, *tbody_id + 1);
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"body > elif\"]\n", parent_body_id, *node);
            tree_to_dot_core(root->op.t_if.condition, node, tbody_id, parent_body_id, dot);
            tree_to_dot_core(root->op.t_if.body, node, tbody_id, parent_body_id, dot);
            if(root->op.t_if.elif)
                tree_to_dot_core(root->op.t_if.elif, node, tbody_id, s_node, dot);
            break;
        }
        case T_FUNC:
        {
            fprintf(dot, "\tNode_%d [label=\"%s\"]\n", ++(*node), efunctostrfunc(root->op.func_args.func));
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"func\"]\n", parent_body_id, *node);
            int s_node = *node;
            for(int i = 0; i < root->op.func_args.n_args; i++){
                tree_to_dot_core(root->op.func_args.args[i], node, tbody_id, s_node, dot);
            }
            break;
        }
        case T_CMP:
        {
            fprintf(dot, "\tNode_%d [label=\"cmp\"]\n", ++(*node));
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"cmp\"]\n", *node - 1, *node);
            fprintf(dot, "\tNode_%d [label=\"%s\"]\n", ++(*node), ecmptostrcmp(root->op.sides.cmp));
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"cond\"]\n", *node - 1, *node);
            int s_node = *node;
            if(root->op.sides.left)
                tree_to_dot_core(root->op.sides.left, node, tbody_id, s_node, dot);
            if(root->op.sides.right)
                tree_to_dot_core(root->op.sides.right, node, tbody_id, s_node, dot);
            break;
        }
        case T_CONST:
            fprintf(dot, "\tNode_%d [label=\"%d\"]\n", ++(*node), root->op.value);
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"const\"]\n", parent_body_id, *node);
            break;
        case T_VAR:
            fprintf(dot, "\tNode_%d [label=\"%d\"]\n", ++(*node), root->op.n_var);
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"var\"]\n", parent_body_id, *node);
            break;
        case T_STR:
            fprintf(dot, "\tNode_%d [label=\"%d\"]\n", ++(*node), root->op.n_str);
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"str\"]\n", parent_body_id, *node);
            break;
        case T_ARIT:
        {
            fprintf(dot, "\tNode_%d [label=\"%s\"]\n", ++(*node), earittostrarit(root->op.arit.arit_op));
            fprintf(dot, "\tNode_%d -> Node_%d [label=\"arit\"]\n", parent_body_id, *node);
            int s_node = *node;
            tree_to_dot_core(root->op.arit.left, node, tbody_id, s_node, dot);
            tree_to_dot_core(root->op.arit.right, node, tbody_id, s_node, dot);
            break;
        }
    }
}
