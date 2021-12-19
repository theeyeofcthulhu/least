#include <string>
#include <iostream>
#include <cassert>
#include <fstream>
#include <stack>
#include <vector>

#include "util.h"
#include "lexer.h"
#include "error.h"
#include "ast.h"
#include "x86_64.h"

void tree_to_dot_core(tree_node* root, int* node, int* tbody_id, int parent_body_id, std::fstream& dot, compile_info& c_info);

const std::map<ekeyword, efunc> key_func_map = {
std::make_pair(K_PRINT, PRINT),
std::make_pair(K_EXIT, EXIT),
std::make_pair(K_READ, READ),
std::make_pair(K_SET, SET),
std::make_pair(K_PUTCHAR, PUTCHAR),
std::make_pair(K_INT, INT),
};

const std::map<efunc, std::string> func_str_map = {
std::make_pair(PRINT, "print"),
std::make_pair(EXIT, "exit"),
std::make_pair(READ, "read"),
std::make_pair(SET, "set"),
std::make_pair(PUTCHAR, "putchar"),
std::make_pair(INT, "int"),
};

std::string ecmptostrcmp(ecmp_operation func){
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

std::string earittostrarit(earit_operation arit){
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

int check_var(std::string var, compile_info& c_info){
    for(size_t i = 0; i < c_info.known_vars.size(); i++)
        if(var == c_info.known_vars[i].first)
            return i;

    c_info.known_vars.push_back(make_pair(var, false));

    return c_info.known_vars.size()-1;
}

int check_str(std::string str, compile_info& c_info){
    for(size_t i = 0; i < c_info.known_string.size(); i++){
        if(str == c_info.known_string[i])
            return i;
    }

    c_info.known_string.push_back(str);

    return c_info.known_string.size()-1;
}

tree_node* node_from_var_or_const(token* tk, compile_info& c_info){
    c_info.err.on_false(tk->get_type() == TK_VAR || tk->get_type() == TK_NUM,
                        "Trying to convert token, which is not a variable or a number, to a variable or a number: '%d'\n", tk->get_type());

    tree_node* res;

    if(tk->get_type() == TK_VAR){
        res = new tree_var(tk->get_line(), check_var(dynamic_cast<t_var*>(tk)->get_name(), c_info));
    }else if(tk->get_type() == TK_NUM){
        tree_const* the = new tree_const(tk->get_line(), dynamic_cast<t_num*>(tk)->get_num());
        res = the;
    }

    return res;
}

bool has_precedence(earit_operation op){
    return op == DIV || op == MUL || op == MOD;
}


/* Parse arithmetic expression respecting precedence */

tree_node* parse_arit_expr(std::vector<token*> ts, compile_info& c_info){
    std::vector<tree_node*> s2;

    earit_operation last_op = ARIT_OPERATION_ENUM_END;

    int len = ts.size();

    /* Group '/' and '*' expressions together and leave '+', '-' and constants in place */
    for(int i = 0; i < len; i++){
        /* If we have '/' or '*' next and are not currently an operator:
         * continue because we belong to that operator */
        earit_operation next_op = ARIT_OPERATION_ENUM_END;
        for(int j = i+1; j < len; j++){
            if(ts[j]->get_type() == TK_ARIT){
                next_op = dynamic_cast<t_arit*>(ts[j])->get_op();
                break;
            }
        }
        if(has_precedence(next_op) && ts[i]->get_type() != TK_ARIT){
            continue;
        }

        if(ts[i]->get_type() == TK_ARIT){
            t_arit* op = dynamic_cast<t_arit*>(ts[i]);
            /* Make new multiplication */
            if(has_precedence(op->get_op())){
                assert(i > 0 && i + 1 < len);
                assert(ts[i-1]->get_type() == TK_NUM || ts[i-1]->get_type() == TK_VAR);
                assert(ts[i+1]->get_type() == TK_NUM || ts[i+1]->get_type() == TK_VAR);

                if(has_precedence(last_op)){
                    /* If we follow another multiplication:
                     * incorporate previous into ourselves and replace that previous one in the array */
                    tree_arit* new_arit = new tree_arit(op->get_line(),
                                                        s2.back(),
                                                        node_from_var_or_const(ts[i+1], c_info),
                                                        op->get_op());
                    s2.back() = new_arit;
                }else{
                    s2.push_back(new tree_arit(op->get_line(),
                                               node_from_var_or_const(ts[i-1], c_info),
                                               node_from_var_or_const(ts[i+1], c_info),
                                               op->get_op()));
                }
            }else{
                s2.push_back(new tree_arit(op->get_line(), nullptr, nullptr, op->get_op()));
            }
            last_op = op->get_op();
        }else{
            /* If we follow a multiplication:
             * we are already part of a multiplication and don't need us in the array */
            if(has_precedence(last_op))
                continue;
            s2.push_back(node_from_var_or_const(ts[i], c_info));
        }
    }
    if(s2.size() == 1){
        return s2[0];
    }

    tree_arit* root = nullptr;
    tree_arit* current = nullptr;

    /* Parse everything into a tree */
    for(size_t i = 0; i < s2.size(); i++){
        if(s2[i]->get_type() == T_ARIT){
            tree_arit* arit = dynamic_cast<tree_arit*>(s2[i]);
            if(arit->get_arit() == ADD || arit->get_arit() == SUB){
                if(!root){
                    root = new tree_arit(arit->get_line(), s2[i-1], NULL, arit->get_arit());
                    current = root;
                }else{
                    tree_arit* new_cur = new tree_arit(arit->get_line(), s2[i-1], NULL, arit->get_arit());
                    current->right = new_cur;
                    current = new_cur;
                }

                if(i+1 == s2.size()-1){
                    current->right = s2[i+1];
                }
            }
        }
    }

    return root;
}


tree_cmp* parse_condition(std::vector<token*> ts, size_t& i, compile_info& c_info){
    tree_node* left;
    tree_node* right;

    token* next;
    int nxt_tkn = i;

    int operator_i = -1;

    t_cmp* comparator = NULL;

    tree_cmp* res;

    while((next = ts[nxt_tkn++])->get_type() != TK_EOL){
        if(next->get_type() == TK_CMP){
            c_info.err.on_true(comparator, "Found two operators\n");
            comparator = dynamic_cast<t_cmp*>(next);
            operator_i = nxt_tkn;
        }
    }
    c_info.err.on_true(operator_i == 1, "Expected constant, variable or arithmetic expression\n");

    if(operator_i != -1){
        left = parse_arit_expr(slice<token*>(ts, i, operator_i-1), c_info);
        right = parse_arit_expr(slice<token*>(ts, operator_i, nxt_tkn-1), c_info);

        res = new tree_cmp(ts[0]->get_line(), left, right, comparator->get_cmp());
    }else{
        left = parse_arit_expr(slice<token*>(ts, i, nxt_tkn-1), c_info);
        res = new tree_cmp(ts[0]->get_line(), left, nullptr, CMP_OPERATION_ENUM_END);
    }

    i = nxt_tkn - 1;

    return res;
}

tree_if* parse_condition_to_if(std::vector<token*> ts, size_t& i, tree_body* root, compile_info& c_info, bool is_elif){
    tree_cmp* condition = parse_condition(ts, i, c_info);
    return new tree_if(ts[i]->get_line(), condition, new tree_body(ts[i]->get_line(), root, c_info), is_elif);
}

tree_while* parse_condition_to_while(std::vector<token*> ts, size_t& i, tree_body* root, compile_info& c_info){
    tree_cmp* condition = parse_condition(ts, i, c_info);
    return new tree_while(ts[i]->get_line(), condition, new tree_body(ts[i]->get_line(), root, c_info));
}

tree_body* tokens_to_ast(std::vector<token*> tokens, compile_info& c_info){
    tree_body* root = new tree_body(tokens[0]->get_line(), nullptr, c_info);
    tree_body* saved_root = root;

    tree_if* current_if = nullptr;

    std::stack<tree_node*> blk_stk;

    for(size_t i = 0; i < tokens.size(); i++){
        c_info.err.set_line(tokens[i]->get_line());
        switch(tokens[i]->get_type()){
            case TK_KEY:
            {
                t_key* key = dynamic_cast<t_key*>(tokens[i]);
                switch(key->get_key()){
                    case K_IF:
                    {
                        tree_if* new_if = parse_condition_to_if(tokens, ++i, root, c_info, false);
                        root->children.push_back(new_if);
                        root = new_if->body;

                        current_if = new_if;

                        blk_stk.push(new_if);
                        break;
                    }
                    case K_ELIF:
                    {
                        c_info.err.on_false(current_if, "Unexpected elif\n");

                        tree_if* new_if = parse_condition_to_if(tokens, ++i, root, c_info, true);

                        current_if->elif = new_if;
                        root = new_if->body;

                        current_if = new_if;
                        break;
                    }
                    case K_ELSE:
                    {
                        c_info.err.on_false(current_if, "Unexpected else");
                        c_info.err.on_false(tokens[i+1]->get_type() == TK_EOL, "Else accepts no arguments");

                        tree_else* new_else = new tree_else(tokens[i]->get_line(), new tree_body(tokens[i]->get_line(), root, c_info));

                        current_if->elif = new_else;

                        root = new_else->body;
                        current_if = nullptr;
                        break;
                    }
                    case K_PRINT:
                    case K_EXIT:
                    case K_READ:
                    case K_SET:
                    case K_PUTCHAR:
                    case K_INT:
                    {
                        tree_func* func = new tree_func(key->get_line(), key_func_map.at(key->get_key()));
                        root->children.push_back(func);

                        switch(func->get_func()){
                            case PUTCHAR:
                                c_info.req_libs[LIB_PUTCHAR] = true;
                                break;
                            default:
                                break;
                        }

                        i += 1;

                        size_t next_sep;
                        while ((next_sep = next_of_type_on_line(tokens, i, TK_SEP)) < tokens.size()){
                            switch(tokens[i]->get_type()){
                                case TK_LSTR:
                                {
                                    c_info.err.on_true((next_sep - i) > 2, "Excess tokens after string argument\n");
                                    func->args.push_back(new tree_lstr(tokens[i]->get_line(),
                                                                       dynamic_cast<t_lstr*>(tokens[i])->ts,
                                                                       c_info));
                                    break;
                                }
                                case TK_NUM:
                                case TK_VAR:
                                {
                                    std::vector<token*> slc = slice<token*>(tokens, i, next_sep);

                                    func->args.push_back(parse_arit_expr(slc, c_info));
                                    break;
                                }
                                default:
                                    c_info.err.error("Unexpected argument to function: %d\n", tokens[i]->get_type());
                                    break;
                            }
                            i = next_sep+1;
                        }

                        break;
                    }
                    case K_END:
                        c_info.err.on_false(root->parent, "Unexpected end\n");
                        // c_info.err.on_true(current_if, t->line + 1, "Unexpected end\n");
                        if(!blk_stk.empty()){
                            switch(blk_stk.top()->get_type()){
                                case T_IF:
                                    root = dynamic_cast<tree_if*>(blk_stk.top())->body->parent;
                                    blk_stk.pop();

                                    if(!blk_stk.empty() && blk_stk.top()->get_type() == T_IF)
                                        current_if = dynamic_cast<tree_if*>(blk_stk.top());
                                    break;
                                case T_WHILE:
                                    root = dynamic_cast<tree_while*>(blk_stk.top())->body->parent;
                                    blk_stk.pop();
                                    break;
                                default:
                                    c_info.err.error("Exiting invalid block\n");
                                    break;
                            }
                        }else
                            root = root->parent;
                        break;
                    case K_WHILE:
                    {
                        tree_while* new_while = parse_condition_to_while(tokens, ++i, root, c_info);
                        root->children.push_back(new_while);

                        root = new_while->body;

                        blk_stk.push(new_while);
                        break;
                    }
                    case K_STR:
                        c_info.err.error("TODO: unimplemented\n");
                        break;
                    case K_NOKEY:
                        c_info.err.error("Invalid instruction\n");
                        break;
                }
            }
            case TK_EOL:
            case TK_SEP:
                break;
            default:
                c_info.err.error("Unexpected token\n");
                break;
        }
    }
    c_info.err.on_false(root == saved_root, "Unresolved blocks\n");

    return saved_root;
}

tree_node* get_last_if(tree_if* if_node){
    tree_node* res = if_node;

    for(;; res = dynamic_cast<tree_if*>(res)->elif){
        if(res->get_type() == T_ELSE || dynamic_cast<tree_if*>(res)->elif == nullptr){
            return res;
        }
    }

    return NULL;
}

void error_on_undefined(tree_var* var_id, struct compile_info& c_info){
    c_info.err.on_false(c_info.known_vars[var_id->get_var_id()].second,
                        "Variable '%s' is undefined at this time\n",
                        c_info.known_vars[var_id->get_var_id()].first.c_str());
}

void tree_to_dot(tree_body* root, std::string fn, compile_info& c_info){
    std::fstream out;
    out.open(fn, std::ios::out);

    out << "digraph AST {\n";

    int node = 0;

    int tbody_id = root->get_body_id();

    tree_to_dot_core(dynamic_cast<tree_node*>(root), &node, &tbody_id, 0, out, c_info);

    out << "}\n";

    out.close();
}

void tree_to_dot_core(tree_node* root, int* node, int* tbody_id, int parent_body_id, std::fstream& dot, compile_info& c_info){
    switch(root->get_type()){
        case T_BODY:
        {
            tree_body* body = dynamic_cast<tree_body*>(root);
            *tbody_id = body->get_body_id();
            dot << "\tNode_" << *tbody_id << "[label=\"body " << *tbody_id - NODE_ARR_SZ << "\"]\n";
            // dot << "\tNode_%d -> Node_%d [label=\"body %d\"]", *tbody_id, (*node), *tbody_id);
            for(auto child : body->children){
                tree_to_dot_core(child, node, tbody_id, body->get_body_id(), dot, c_info);
            }
            break;
        }
        case T_IF:
        {
            tree_if* t_if = dynamic_cast<tree_if*>(root);

            dot << "\tNode_" << ++(*node) << "[label=\"if\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"body > if\"]\n";

            int s_node = *node;

            tree_to_dot_core(t_if->condition, node, tbody_id, parent_body_id, dot, c_info);

            dot << "\tNode_" << s_node << " -> Node_" << *tbody_id + 1 << " [label=\"if > body\"]\n";
            tree_to_dot_core(t_if->body, node, tbody_id, parent_body_id, dot, c_info);

            if(t_if->elif)
                tree_to_dot_core(t_if->elif, node, tbody_id, s_node, dot, c_info);
            break;
        }
        case T_ELSE:
        {
            tree_else* t_else = dynamic_cast<tree_else*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"else\"]\n";
            dot << "\tNode_" << *node << " -> Node_" << *tbody_id + 1 << " [label=\"else > body\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"body > else\"]\n";
            tree_to_dot_core(t_else->body, node, tbody_id, parent_body_id, dot, c_info);
            break;
        }
        case T_ELIF:
        {
            tree_if* t_elif = dynamic_cast<tree_if*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"elif\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"body > elif\"]\n";

            int s_node = *node;

            tree_to_dot_core(t_elif->condition, node, tbody_id, parent_body_id, dot, c_info);

            dot << "\tNode_" << s_node << " -> Node_" << *tbody_id + 1 << " [label=\"elif > body\"]\n";
            tree_to_dot_core(t_elif->body, node, tbody_id, parent_body_id, dot, c_info);

            if(t_elif->elif)
                tree_to_dot_core(t_elif->elif, node, tbody_id, s_node, dot, c_info);
            break;
        }
        case T_FUNC:
        {
            tree_func* func = dynamic_cast<tree_func*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"" << func_str_map.at(func->get_func()) << "\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"func\"]\n";

            int s_node = *node;
            for(auto arg : func->args){
                tree_to_dot_core(arg, node, tbody_id, s_node, dot, c_info);
            }
            break;
        }
        case T_CMP:
        {
            tree_cmp* cmp = dynamic_cast<tree_cmp*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"cmp\"]\n";
            dot << "\tNode_" << *node - 1 << " -> Node_" << *node << " [label=\"cmp\"]\n";
            dot << "\tNode_" << ++(*node) << " [label=\"" << ecmptostrcmp(cmp->get_cmp()) << "\"]\n";
            dot << "\tNode_" << *node - 1 << " -> Node_" << *node << " [label=\"cond\"]\n";

            int s_node = *node;

            if(cmp->left)
                tree_to_dot_core(cmp->left, node, tbody_id, s_node, dot, c_info);
            if(cmp->right)
                tree_to_dot_core(cmp->right, node, tbody_id, s_node, dot, c_info);
            break;
        }
        case T_CONST:
        {
            tree_const* num = dynamic_cast<tree_const*>(root);
            dot << "\tNode_" << ++(*node) << " [label=\"" << num->get_value() << "\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"const\"]\n";
            break;
        }
        case T_VAR:
        {
            tree_var* var = dynamic_cast<tree_var*>(root);
            dot << "\tNode_" << ++(*node) << " [label=\"" << var->get_var_id() << "\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"var\"]\n";
            break;
        }
        case T_STR:
        {
            tree_str* string = dynamic_cast<tree_str*>(root);
            dot << "\tNode_" << ++(*node) << " [label=\"" << string->get_str_id() << "\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"str\"]\n";
            break;
        }
        case T_ARIT:
        {
            tree_arit* arit = dynamic_cast<tree_arit*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"" << earittostrarit(arit->get_arit()) << "\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"arit\"]\n";

            int s_node = *node;

            tree_to_dot_core(arit->left, node, tbody_id, s_node, dot, c_info);
            tree_to_dot_core(arit->right, node, tbody_id, s_node, dot, c_info);
            break;
        }
        case T_WHILE:
        {
            tree_while* t_while = dynamic_cast<tree_while*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"while\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"body > while\"]\n";

            int s_node = *node;

            tree_to_dot_core(t_while->condition, node, tbody_id, parent_body_id, dot, c_info);

            dot << "\tNode_" << s_node << " -> Node_" << *tbody_id + 1 << " [label=\"while > body\"]\n";

            tree_to_dot_core(t_while->body, node, tbody_id, parent_body_id, dot, c_info);
            break;
        }
        case T_LSTR:
        {
            tree_lstr* lstr = dynamic_cast<tree_lstr*>(root);

            dot << "\tNode_" << ++(*node) << " [label=\"lstring\"]\n";
            dot << "\tNode_" << parent_body_id << " -> Node_" << *node << " [label=\"lstring\"]\n";

            int s_node = *node;
            for(auto format : lstr->format){
                tree_to_dot_core(format, node, tbody_id, s_node, dot, c_info);
            }
            break;
        }
        case T_INV:
        {
            c_info.err.error("Invalid tree node\n");
            break;
        }
    }
}
