#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "ast.h"
#include "error.h"
#include "lexer.h"
#include "util.h"
#include "x86_64.h"

void tree_to_dot_core(std::shared_ptr<tree_node> root, int& node, int& tbody_id,
                      int parent_body_id, std::fstream &dot,
                      compile_info &c_info);

const std::map<ekeyword, efunc> key_func_map = {
    std::make_pair(K_PRINT, PRINT),     std::make_pair(K_EXIT, EXIT),
    std::make_pair(K_READ, READ),       std::make_pair(K_SET, SET),
    std::make_pair(K_PUTCHAR, PUTCHAR), std::make_pair(K_INT, INT),
};

/* Maps for converting enum values to strings */

const std::map<efunc, std::string> func_str_map = {
    std::make_pair(PRINT, "print"),     std::make_pair(EXIT, "exit"),
    std::make_pair(READ, "read"),       std::make_pair(SET, "set"),
    std::make_pair(PUTCHAR, "putchar"), std::make_pair(INT, "int"),
};

const std::map<ecmp_operation, std::string> cmp_str_map = {
    std::make_pair(EQUAL, "=="),         std::make_pair(GREATER, ">"),
    std::make_pair(GREATER_OR_EQ, ">="), std::make_pair(LESS, "<"),
    std::make_pair(LESS_OR_EQ, "<="),    std::make_pair(NOT_EQUAL, "!="),
};

const std::map<earit_operation, std::string> arit_str_map = {
    std::make_pair(ADD, "+"), std::make_pair(DIV, "/"),
    std::make_pair(MOD, "%"), std::make_pair(MUL, "*"),
    std::make_pair(SUB, "-"),
};

/* If a var is defined: return its index
 * else:                add a new variable to c_info's known_vars */
int check_var(std::string var, compile_info &c_info) {
    for (size_t i = 0; i < c_info.known_vars.size(); i++)
        if (var == c_info.known_vars[i].first)
            return i;

    c_info.known_vars.push_back(make_pair(var, false));

    return c_info.known_vars.size() - 1;
}

/* Same as check_var but with string */
int check_str(std::string str, compile_info &c_info) {
    for (size_t i = 0; i < c_info.known_string.size(); i++)
        if (str == c_info.known_string[i])
            return i;

    c_info.known_string.push_back(str);

    return c_info.known_string.size() - 1;
}

/* Tree node from variable or constant */
std::shared_ptr<tree_node> node_from_var_or_const(std::shared_ptr<token> tk,
                                                  compile_info &c_info) {
    c_info.err.on_false(tk->get_type() == TK_VAR || tk->get_type() == TK_NUM,
                        "Trying to convert token, which is not a variable or a "
                        "number, to a variable or a number: '%d'\n",
                        tk->get_type());

    std::shared_ptr<tree_node> res;

    if (tk->get_type() == TK_VAR) {
        res = std::make_shared<tree_var>(
            tk->get_line(),
            check_var(std::dynamic_pointer_cast<t_var>(tk)->get_name(),
                      c_info));
    } else if (tk->get_type() == TK_NUM) {
        res = std::make_shared<tree_const>(
            tk->get_line(), std::dynamic_pointer_cast<t_num>(tk)->get_num());
    }

    return res;
}

bool has_precedence(earit_operation op) {
    return op == DIV || op == MUL || op == MOD;
}

/* Parse arithmetic expression respecting precedence */

std::shared_ptr<tree_node>
parse_arit_expr(std::vector<std::shared_ptr<token>> ts, compile_info &c_info) {
    std::vector<std::shared_ptr<tree_node>> s2;

    earit_operation last_op = ARIT_OPERATION_ENUM_END;

    int len = ts.size();

    /* Group '/' and '*' expressions together and leave '+', '-' and constants
     * in place */
    for (int i = 0; i < len; i++) {
        /* If we have '/' or '*' next and are not currently an operator:
         * continue because we belong to that operator */
        earit_operation next_op = ARIT_OPERATION_ENUM_END;
        for (int j = i + 1; j < len; j++) {
            if (ts[j]->get_type() == TK_ARIT) {
                next_op = std::dynamic_pointer_cast<t_arit>(ts[j])->get_op();
                break;
            }
        }
        if (has_precedence(next_op) && ts[i]->get_type() != TK_ARIT) {
            continue;
        }

        if (ts[i]->get_type() == TK_ARIT) {
            std::shared_ptr<t_arit> op =
                std::dynamic_pointer_cast<t_arit>(ts[i]);
            /* Make new multiplication */
            if (has_precedence(op->get_op())) {
                assert(i > 0 && i + 1 < len);
                assert(ts[i - 1]->get_type() == TK_NUM ||
                       ts[i - 1]->get_type() == TK_VAR);
                assert(ts[i + 1]->get_type() == TK_NUM ||
                       ts[i + 1]->get_type() == TK_VAR);

                if (has_precedence(last_op)) {
                    /* If we follow another multiplication:
                     * incorporate previous into ourselves and replace that
                     * previous one in the array */
                    std::shared_ptr<tree_arit> new_arit =
                        std::make_shared<tree_arit>(
                            op->get_line(), s2.back(),
                            node_from_var_or_const(ts[i + 1], c_info),
                            op->get_op());
                    s2.back() = new_arit;
                } else {
                    s2.push_back(std::make_shared<tree_arit>(
                        op->get_line(),
                        node_from_var_or_const(ts[i - 1], c_info),
                        node_from_var_or_const(ts[i + 1], c_info),
                        op->get_op()));
                }
            } else {
                s2.push_back(std::make_shared<tree_arit>(
                    op->get_line(), nullptr, nullptr, op->get_op()));
            }
            last_op = op->get_op();
        } else {
            /* If we follow a multiplication:
             * we are already part of a multiplication and don't need us in the
             * array */
            if (has_precedence(last_op))
                continue;
            s2.push_back(node_from_var_or_const(ts[i], c_info));
        }
    }
    if (s2.size() == 1) {
        return s2[0];
    }

    std::shared_ptr<tree_arit> root = nullptr;
    std::shared_ptr<tree_arit> current = nullptr;

    /* Parse everything into a tree */
    for (size_t i = 0; i < s2.size(); i++) {
        if (s2[i]->get_type() == T_ARIT) {
            std::shared_ptr<tree_arit> arit =
                std::dynamic_pointer_cast<tree_arit>(s2[i]);
            if (arit->get_arit() == ADD || arit->get_arit() == SUB) {
                if (!root) {
                    root = std::make_shared<tree_arit>(
                        arit->get_line(), s2[i - 1], nullptr, arit->get_arit());
                    current = root;
                } else {
                    std::shared_ptr<tree_arit> new_cur =
                        std::make_shared<tree_arit>(arit->get_line(), s2[i - 1],
                                                    nullptr, arit->get_arit());
                    current->right = new_cur;
                    current = new_cur;
                }

                if (i + 1 == s2.size() - 1) {
                    current->right = s2[i + 1];
                }
            }
        }
    }

    return root;
}

std::shared_ptr<tree_cmp>
parse_condition(std::vector<std::shared_ptr<token>> ts, size_t &i,
                compile_info &c_info) {
    std::shared_ptr<tree_node> left;
    std::shared_ptr<tree_node> right;

    std::shared_ptr<token> next;
    int nxt_tkn = i;

    int operator_i = -1;

    std::shared_ptr<t_cmp> comparator = nullptr;

    std::shared_ptr<tree_cmp> res;

    while ((next = ts[nxt_tkn++])->get_type() != TK_EOL) {
        if (next->get_type() == TK_CMP) {
            c_info.err.on_false(comparator == nullptr, "Found two operators\n");
            comparator = std::dynamic_pointer_cast<t_cmp>(next);
            operator_i = nxt_tkn;
        }
    }
    c_info.err.on_true(
        operator_i == 1,
        "Expected constant, variable or arithmetic expression\n");

    if (operator_i != -1) {
        left = parse_arit_expr(slice(ts, i, operator_i - 1), c_info);
        right = parse_arit_expr(slice(ts, operator_i, nxt_tkn - 1), c_info);

        res = std::make_shared<tree_cmp>(ts[0]->get_line(), left, right,
                                         comparator->get_cmp());
    } else {
        left = parse_arit_expr(slice(ts, i, nxt_tkn - 1), c_info);
        res = std::make_shared<tree_cmp>(ts[0]->get_line(), left, nullptr,
                                         CMP_OPERATION_ENUM_END);
    }

    i = nxt_tkn - 1;

    return res;
}

std::shared_ptr<tree_if>
parse_condition_to_if(std::vector<std::shared_ptr<token>> ts, size_t &i,
                      std::shared_ptr<tree_body> root, compile_info &c_info,
                      bool is_elif) {
    std::shared_ptr<tree_cmp> condition = parse_condition(ts, i, c_info);
    return std::make_shared<tree_if>(
        ts[i]->get_line(), condition,
        std::make_shared<tree_body>(ts[i]->get_line(), root, c_info), is_elif);
}

std::shared_ptr<tree_while>
parse_condition_to_while(std::vector<std::shared_ptr<token>> ts, size_t &i,
                         std::shared_ptr<tree_body> root,
                         compile_info &c_info) {
    std::shared_ptr<tree_cmp> condition = parse_condition(ts, i, c_info);
    return std::make_shared<tree_while>(
        ts[i]->get_line(), condition,
        std::make_shared<tree_body>(ts[i]->get_line(), root, c_info));
}

std::shared_ptr<tree_body>
tokens_to_ast(std::vector<std::shared_ptr<token>> tokens,
              compile_info &c_info) {
    std::shared_ptr<tree_body> root =
        std::make_shared<tree_body>(tokens[0]->get_line(), nullptr, c_info);
    std::shared_ptr<tree_body> saved_root = root;

    std::shared_ptr<tree_if> current_if = nullptr;

    std::stack<std::shared_ptr<tree_node>> blk_stk;

    for (size_t i = 0; i < tokens.size(); i++) {
        c_info.err.set_line(tokens[i]->get_line());
        switch (tokens[i]->get_type()) {
        case TK_KEY:
        {
            std::shared_ptr<t_key> key =
                std::dynamic_pointer_cast<t_key>(tokens[i]);
            switch (key->get_key()) {
            case K_IF:
            {
                std::shared_ptr<tree_if> new_if =
                    parse_condition_to_if(tokens, ++i, root, c_info, false);
                root->children.push_back(new_if);
                root = new_if->body;

                current_if = new_if;

                blk_stk.push(new_if);
                break;
            }
            case K_ELIF:
            {
                c_info.err.on_true(current_if == nullptr, "Unexpected elif\n");

                std::shared_ptr<tree_if> new_if =
                    parse_condition_to_if(tokens, ++i, root, c_info, true);

                current_if->elif = new_if;
                root = new_if->body;

                current_if = new_if;
                break;
            }
            case K_ELSE:
            {
                c_info.err.on_true(current_if == nullptr, "Unexpected else");
                c_info.err.on_false(tokens[i + 1]->get_type() == TK_EOL,
                                    "Else accepts no arguments");

                std::shared_ptr<tree_else> new_else =
                    std::make_shared<tree_else>(
                        tokens[i]->get_line(),
                        std::make_shared<tree_body>(tokens[i]->get_line(), root,
                                                    c_info));

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
                std::shared_ptr<tree_func> func = std::make_shared<tree_func>(
                    key->get_line(), key_func_map.at(key->get_key()));
                root->children.push_back(func);

                switch (func->get_func()) {
                case PUTCHAR:
                    c_info.req_libs[LIB_PUTCHAR] = true;
                    break;
                default:
                    break;
                }

                i += 1;

                size_t next_sep;
                while ((next_sep = next_of_type_on_line(tokens, i, TK_SEP)) <
                       tokens.size()) {
                    switch (tokens[i]->get_type()) {
                    case TK_LSTR:
                    {
                        c_info.err.on_true(
                            (next_sep - i) > 2,
                            "Excess tokens after string argument\n");
                        func->args.push_back(std::make_shared<tree_lstr>(
                            tokens[i]->get_line(),
                            std::dynamic_pointer_cast<t_lstr>(tokens[i])->ts,
                            c_info));
                        break;
                    }
                    case TK_NUM:
                    case TK_VAR:
                    {
                        std::vector<std::shared_ptr<token>> slc =
                            slice(tokens, i, next_sep);

                        func->args.push_back(parse_arit_expr(slc, c_info));
                        break;
                    }
                    default:
                        c_info.err.error(
                            "Unexpected argument to function: %d\n",
                            tokens[i]->get_type());
                        break;
                    }
                    i = next_sep + 1;
                }

                break;
            }
            case K_END:
                c_info.err.on_true(root->parent == nullptr, "Unexpected end\n");
                // c_info.err.on_true(current_if, t->line + 1, "Unexpected
                // end\n");
                if (!blk_stk.empty()) {
                    switch (blk_stk.top()->get_type()) {
                    case T_IF:
                        root = std::dynamic_pointer_cast<tree_if>(blk_stk.top())
                                   ->body->parent;
                        blk_stk.pop();

                        if (!blk_stk.empty() &&
                            blk_stk.top()->get_type() == T_IF)
                            current_if = std::dynamic_pointer_cast<tree_if>(
                                blk_stk.top());
                        break;
                    case T_WHILE:
                        root =
                            std::dynamic_pointer_cast<tree_while>(blk_stk.top())
                                ->body->parent;
                        blk_stk.pop();
                        break;
                    default:
                        c_info.err.error("Exiting invalid block\n");
                        break;
                    }
                } else
                    root = root->parent;
                break;
            case K_WHILE:
            {
                std::shared_ptr<tree_while> new_while =
                    parse_condition_to_while(tokens, ++i, root, c_info);
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

std::shared_ptr<tree_node> get_last_if(std::shared_ptr<tree_if> if_node) {
    std::shared_ptr<tree_node> res = if_node;

    for (;; res = std::dynamic_pointer_cast<tree_if>(res)->elif) {
        if (res->get_type() == T_ELSE ||
            std::dynamic_pointer_cast<tree_if>(res)->elif == nullptr) {
            return res;
        }
    }

    return nullptr;
}

void error_on_undefined(std::shared_ptr<tree_var> var_id,
                        struct compile_info &c_info) {
    c_info.err.on_false(c_info.known_vars[var_id->get_var_id()].second,
                        "Variable '%s' is undefined at this time\n",
                        c_info.known_vars[var_id->get_var_id()].first.c_str());
}

void tree_to_dot(std::shared_ptr<tree_body> root, std::string fn,
                 compile_info &c_info) {
    std::fstream out;
    out.open(fn, std::ios::out);

    out << "digraph AST {\n";

    int node = 0;

    int tbody_id = root->get_body_id();

    tree_to_dot_core(std::dynamic_pointer_cast<tree_node>(root), node,
                     tbody_id, 0, out, c_info);

    out << "}\n";

    out.close();
}

void tree_to_dot_core(std::shared_ptr<tree_node> root, int& node, int& tbody_id,
                      int parent_body_id, std::fstream &dot,
                      compile_info &c_info) {
    switch (root->get_type()) {
    case T_BODY:
    {
        std::shared_ptr<tree_body> body =
            std::dynamic_pointer_cast<tree_body>(root);
        tbody_id = body->get_body_id();
        dot << "\tNode_" << tbody_id << "[label=\"body "
            << tbody_id << "\"]\n";
        for (auto child : body->children) {
            tree_to_dot_core(child, node, tbody_id, body->get_body_id(), dot,
                             c_info);
        }
        break;
    }
    case T_IF:
    {
        std::shared_ptr<tree_if> t_if =
            std::dynamic_pointer_cast<tree_if>(root);

        dot << "\tNode_" << ++node << "[label=\"if\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body > if\"]\n";

        int s_node = node;

        tree_to_dot_core(t_if->condition, node, tbody_id, parent_body_id, dot,
                         c_info);

        dot << "\tNode_" << s_node << " -> Node_" << tbody_id + 1
            << " [label=\"if > body\"]\n";
        tree_to_dot_core(t_if->body, node, tbody_id, parent_body_id, dot,
                         c_info);

        if (t_if->elif)
            tree_to_dot_core(t_if->elif, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_ELSE:
    {
        std::shared_ptr<tree_else> t_else =
            std::dynamic_pointer_cast<tree_else>(root);

        dot << "\tNode_" << ++node << " [label=\"else\"]\n";
        dot << "\tNode_" << node << " -> Node_" << tbody_id + 1
            << " [label=\"else > body\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body > else\"]\n";
        tree_to_dot_core(t_else->body, node, tbody_id, parent_body_id, dot,
                         c_info);
        break;
    }
    case T_ELIF:
    {
        std::shared_ptr<tree_if> t_elif =
            std::dynamic_pointer_cast<tree_if>(root);

        dot << "\tNode_" << ++node << " [label=\"elif\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body > elif\"]\n";

        int s_node = node;

        tree_to_dot_core(t_elif->condition, node, tbody_id, parent_body_id, dot,
                         c_info);

        dot << "\tNode_" << s_node << " -> Node_" << tbody_id + 1
            << " [label=\"elif > body\"]\n";
        tree_to_dot_core(t_elif->body, node, tbody_id, parent_body_id, dot,
                         c_info);

        if (t_elif->elif)
            tree_to_dot_core(t_elif->elif, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_FUNC:
    {
        std::shared_ptr<tree_func> func =
            std::dynamic_pointer_cast<tree_func>(root);

        dot << "\tNode_" << ++node << " [label=\""
            << func_str_map.at(func->get_func()) << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"func\"]\n";

        int s_node = node;
        for (auto arg : func->args) {
            tree_to_dot_core(arg, node, tbody_id, s_node, dot, c_info);
        }
        break;
    }
    case T_CMP:
    {
        std::shared_ptr<tree_cmp> cmp =
            std::dynamic_pointer_cast<tree_cmp>(root);

        dot << "\tNode_" << ++node << " [label=\"cmp\"]\n";
        dot << "\tNode_" << node - 1 << " -> Node_" << node
            << " [label=\"cmp\"]\n";
        dot << "\tNode_" << ++node << " [label=\""
            << cmp_str_map.at(cmp->get_cmp()) << "\"]\n";
        dot << "\tNode_" << node - 1 << " -> Node_" << node
            << " [label=\"cond\"]\n";

        int s_node = node;

        if (cmp->left)
            tree_to_dot_core(cmp->left, node, tbody_id, s_node, dot, c_info);
        if (cmp->right)
            tree_to_dot_core(cmp->right, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_CONST:
    {
        std::shared_ptr<tree_const> num =
            std::dynamic_pointer_cast<tree_const>(root);
        dot << "\tNode_" << ++node << " [label=\"" << num->get_value()
            << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"const\"]\n";
        break;
    }
    case T_VAR:
    {
        std::shared_ptr<tree_var> var =
            std::dynamic_pointer_cast<tree_var>(root);
        dot << "\tNode_" << ++node << " [label=\"" << var->get_var_id()
            << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"var\"]\n";
        break;
    }
    case T_STR:
    {
        std::shared_ptr<tree_str> string =
            std::dynamic_pointer_cast<tree_str>(root);
        dot << "\tNode_" << ++node << " [label=\"" << string->get_str_id()
            << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"str\"]\n";
        break;
    }
    case T_ARIT:
    {
        std::shared_ptr<tree_arit> arit =
            std::dynamic_pointer_cast<tree_arit>(root);

        dot << "\tNode_" << ++node << " [label=\""
            << arit_str_map.at(arit->get_arit()) << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"arit\"]\n";

        int s_node = node;

        tree_to_dot_core(arit->left, node, tbody_id, s_node, dot, c_info);
        tree_to_dot_core(arit->right, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_WHILE:
    {
        std::shared_ptr<tree_while> t_while =
            std::dynamic_pointer_cast<tree_while>(root);

        dot << "\tNode_" << ++node << " [label=\"while\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body > while\"]\n";

        int s_node = node;

        tree_to_dot_core(t_while->condition, node, tbody_id, parent_body_id,
                         dot, c_info);

        dot << "\tNode_" << s_node << " -> Node_" << tbody_id + 1
            << " [label=\"while > body\"]\n";

        tree_to_dot_core(t_while->body, node, tbody_id, parent_body_id, dot,
                         c_info);
        break;
    }
    case T_LSTR:
    {
        std::shared_ptr<tree_lstr> lstr =
            std::dynamic_pointer_cast<tree_lstr>(root);

        dot << "\tNode_" << ++node << " [label=\"lstring\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"lstring\"]\n";

        int s_node = node;
        for (auto format : lstr->format) {
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
