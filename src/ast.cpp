#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <string_view>
#include <vector>

#include "ast.hpp"
#include "dictionary.hpp"
#include "lexer.hpp"
#include "maps.hpp"
#include "util.hpp"

namespace ast {

std::shared_ptr<Body> AstContext::make_body(int line, std::shared_ptr<Body> t_parent)
{
    return std::make_shared<Body>(line, t_parent, m_c_info.get_next_body_id());
}

std::shared_ptr<Lstr> AstContext::make_lstr(int line, const std::vector<std::shared_ptr<lexer::Token>>& ts)
{
    std::vector<std::shared_ptr<Node>> format;

    for (const auto& tk : ts) {
        switch (tk->get_type()) {
        case lexer::TK_STR: {
            auto string = LEXER_SAFE_CAST(lexer::Str, tk);
            format.push_back(std::make_shared<Str>(line, m_c_info.check_str(string->get_str())));
            break;
        }
        case lexer::TK_VAR: {
            auto token_var = LEXER_SAFE_CAST(lexer::Var, tk);
            format.push_back(
                std::make_shared<Var>(line, m_c_info.check_var(token_var->get_name())));
            break;
        }
        case lexer::TK_NUM: {
            auto cnst = LEXER_SAFE_CAST(lexer::Num, tk);
            format.push_back(std::make_shared<Const>(line, cnst->get_num()));
            break;
        }
        case lexer::TK_ACCESS: {
            auto access = LEXER_SAFE_CAST(lexer::Access, tk);
            format.push_back(std::make_shared<Access>(line, m_c_info.check_array(access->get_array_name()), parse_arit_expr(access->expr)));
            break;
        }
        default:
            UNREACHABLE();
            break;
        }
    }

    return std::make_shared<Lstr>(line, format);
}

/* Tree node from variable or constant */
std::shared_ptr<ast::Node> AstContext::node_from_numeric_token(std::shared_ptr<lexer::Token> tk)
{
    assert(lexer::could_be_num(tk->get_type()));

    std::shared_ptr<ast::Node> res;

    if (tk->get_type() == lexer::TK_VAR) {
        res = std::make_shared<ast::Var>(
            tk->get_line(), m_c_info.check_var(LEXER_SAFE_CAST(lexer::Var, tk)->get_name()));
    } else if (tk->get_type() == lexer::TK_NUM) {
        res = std::make_shared<ast::Const>(tk->get_line(),
            LEXER_SAFE_CAST(lexer::Num, tk)->get_num());
    } else if (tk->get_type() == lexer::TK_DOUBLE_NUM) {
        res = std::make_shared<ast::DoubleConst>(tk->get_line(),
            LEXER_SAFE_CAST(lexer::DoubleNum, tk)->get_num());
    } else if (tk->get_type() == lexer::TK_COM_CALL) {
        const auto& call = LEXER_SAFE_CAST(lexer::CompleteCall, tk);

        var_type ret_type = vfunc_var_type_map.at(call->get_vfunc());
        m_c_info.err.on_false(ret_type == V_INT, "'{}' does not return an integer",
            vfunc_str_map.at(call->get_vfunc()));

        res = std::make_shared<ast::VFunc>(tk->get_line(), call->get_vfunc(), ret_type);
    } else if (tk->get_type() == lexer::TK_ACCESS) {
        const auto& access = LEXER_SAFE_CAST(lexer::Access, tk);

        res = std::make_shared<ast::Access>(tk->get_line(), m_c_info.check_array(access->get_array_name()), parse_arit_expr(access->expr));
    }

    return res;
}

/* Ensure that the pattern: 'num op num op num ...' is met */
void AstContext::ensure_arit_correctness(const std::vector<std::shared_ptr<lexer::Token>>& ts)
{
    bool expect_operator = false;
    for (size_t i = 0; i < ts.size(); i++) {
        const auto& t = ts[i];

        // TODO: ensure correct number and direction of brackets here?
        if (t->get_type() == lexer::TK_BRACKET)
            continue;

        if (expect_operator) {
            m_c_info.err.on_false(t->get_type() == lexer::TK_ARIT, "Expected arithmetic operator");
        } else {
            m_c_info.err.on_false(lexer::could_be_num(t->get_type()),
                "Expected variable, parenthesis, constant or inline call");
        }

        expect_operator = !expect_operator;
    }
}

/* Parse arithmetic expression respecting precedence and bracket expressions */
std::shared_ptr<Node> AstContext::parse_arit_expr(const std::vector<std::shared_ptr<lexer::Token>>& ts)
{
    ensure_arit_correctness(ts);

    std::vector<std::shared_ptr<Node>> s1;
    std::vector<int> s1_brackets;

    for (size_t i = 0; i < ts.size(); i++) {
        if (ts[i]->get_type() == lexer::TK_BRACKET) {
            auto bracket = LEXER_SAFE_CAST(lexer::Bracket, ts[i]);

            m_c_info.err.on_false(bracket->get_kind() == lexer::Bracket::Kind::Open &&
                                bracket->get_purpose() == lexer::Bracket::Purpose::Math,
                                "Expected opening '('");

            size_t closing_index = i;
            size_t stack = 1;

            // Ignore new pairs in the already existing pair
            for (size_t j = i + 1; j < ts.size(); j++) {
                if (ts[j]->get_type() == lexer::TK_BRACKET) {
                    auto closing = LEXER_SAFE_CAST(lexer::Bracket, ts[j]);
                    if (closing->get_kind() == lexer::Bracket::Kind::Open)
                        stack += 1;
                    else
                        stack -= 1;

                    if (stack == 0) {
                        closing_index = j;
                        break;
                    }
                }
            }

            m_c_info.err.on_true(closing_index == i, "Could not find closing parenthesis");

            s1.push_back(parse_arit_expr(slice(ts, i + 1, closing_index)));
            s1_brackets.push_back(s1.size() - 1);

            i = closing_index;
            continue;
        } else {
            switch(ts[i]->get_type()) {
            case lexer::TK_ARIT: {
                auto arit = LEXER_SAFE_CAST(lexer::Arit, ts[i]);
                s1.push_back(std::make_shared<ast::Arit>(ts[i]->get_line(), nullptr, nullptr, arit->get_op()));
                break;
            }
            case lexer::TK_COM_CALL:
            case lexer::TK_ACCESS:
            case lexer::TK_NUM:
            case lexer::TK_DOUBLE_NUM:
            case lexer::TK_VAR: {
                s1.push_back(node_from_numeric_token(ts[i]));
                break;
            }
            default:
                UNREACHABLE();
                break;
            }
        }
    }

    std::vector<std::shared_ptr<Node>> s2;
    std::vector<int> s2_ignore;

    arit_op last_op = ARIT_OPERATION_ENUM_END;

    /* Group '/' and '*' expressions together and leave '+', '-' and constants
     * in place */
    for (size_t i = 0; i < s1.size(); i++) {
        /* If we have '/' or '*' next and are not currently an operator:
         * continue because we belong to that operator */

        bool is_bracket = HAS(s1_brackets, i);

        arit_op next_op = ARIT_OPERATION_ENUM_END;
        for (size_t j = i + 1; j < s1.size(); j++) {
            if (s1[j]->get_type() == T_ARIT && !HAS(s1_brackets, j)) {
                next_op = AST_SAFE_CAST(ast::Arit, s1[j])->get_arit();
                break;
            }
        }
        /* We are part of the next operation, since it has precedence */
        if (has_precedence(next_op) && (s1[i]->get_type() != ast::T_ARIT || is_bracket)) {
            continue;
        } else if (s1[i]->get_type() == ast::T_ARIT && !is_bracket) {
            auto op = AST_SAFE_CAST(ast::Arit, s1[i]);
            /* Make new multiplication */
            if (has_precedence(op->get_arit())) {
                assert(i > 0 && i + 1 < s1.size());

                if (!has_precedence(last_op)) {
                    /* We follow a +/-, take last */
                    s2.push_back(std::make_shared<Arit>(
                        op->get_line(), s1[i - 1],
                        s1[i + 1], op->get_arit()));
                } else {
                    /* If we follow another multiplication
                     * incorporate previous into ourselves and replace that
                     * previous one in the array */
                    std::shared_ptr<Arit> new_arit = std::make_shared<Arit>(
                        op->get_line(), s2.back(), s1[i + 1],
                        op->get_arit());
                    s2.back() = new_arit;
                }

            } else {
                /* Make empty +/-, filled in stage three */
                s2.push_back(std::make_shared<Arit>(op->get_line(), nullptr, nullptr, op->get_arit()));
            }
            last_op = op->get_arit();
        } else {
            /* If we follow a multiplication:
             * we are already part of a multiplication and don't need us in the
             * array */
            if (has_precedence(last_op))
                continue;

            s2.push_back(s1[i]);
            if (is_bracket)
                s2_ignore.push_back(s2.size() - 1);
        }
    }
    if (s2.size() == 1) {
        return s2[0];
    }

    std::shared_ptr<Arit> root = nullptr;
    std::shared_ptr<Arit> current = nullptr;

    /* In s2, we now have an array of nodes which are either:
     * a number, an empty plus or minus node, or a complete
     * multiplication division. Now we make a tree out of that. */

    /* Parse everything into a tree */
    for (size_t i = 0; i < s2.size(); i++) {
        if (s2[i]->get_type() == T_ARIT) {
            // This is a compiled bracket expression, ignore it
            if (HAS(s2_ignore, i)) {
                continue;
            }

            std::shared_ptr<Arit> cur_arit = AST_SAFE_CAST(Arit, s2[i]);
            if (!has_precedence(cur_arit->get_arit())) {
                if (!root) {
                    root = std::make_shared<Arit>(cur_arit->get_line(), s2[i - 1], nullptr,
                        cur_arit->get_arit());
                    current = root;
                } else {
                    std::shared_ptr<Arit> new_cur = std::make_shared<Arit>(
                        cur_arit->get_line(), s2[i - 1], nullptr, cur_arit->get_arit());
                    current->right = new_cur;
                    current = new_cur;
                }

                if (i + 1 >= s2.size() - 1) {
                    /* If we are the last thing: set our own right to the next
                     * number */
                    m_c_info.err.on_true(i + 1 > s2.size() - 1, "Expected number after operand '{}'",
                        arit_str_map.at(cur_arit->get_arit()));
                    current->right = s2[i + 1];
                } else if (s2[i + 1]->get_type() == T_ARIT) {
                    std::shared_ptr<Arit> next_arit = AST_SAFE_CAST(Arit, s2[i + 1]);
                    m_c_info.err.on_false(has_precedence(next_arit->get_arit()),
                        "+/- followed by another +/-");
                }
            }
        }
    }

    return root;
}

/* Parse logical expression (a && b || c) starting from i and update
 * i to end of line */
std::shared_ptr<Node> AstContext::parse_logical(size_t& i)
{
    size_t last_i, next_i, eol;
    last_i = next_i = i;

    eol = next_of_type_on_line(m_tokens, i, lexer::TK_EOL);

    /* Not a logical operation; pass on to parse_condition */
    if (next_of_type_on_line(m_tokens, i, lexer::TK_LOG) == m_tokens.size()) {
        auto res = parse_condition(slice(m_tokens, i, eol));
        i = eol - 1;

        return res;
    }

    std::shared_ptr<Log> res = nullptr;
    std::shared_ptr<Log> current = nullptr;

    for (; m_tokens[next_i]->get_type() != lexer::TK_EOL; next_i++) {
        if (m_tokens[next_i]->get_type() == lexer::TK_LOG) {
            auto log = LEXER_SAFE_CAST(lexer::Log, m_tokens[next_i]);

            m_c_info.err.on_true(m_tokens[next_i + 1]->get_type() == lexer::TK_EOL || m_tokens[next_i + 1]->get_type() == lexer::TK_LOG,
                "Expected number after '{}'", log_str_map.at(log->get_log()));
            m_c_info.err.on_true(next_i == i, "'{}' not expected at beginning of expression",
                log_str_map.at(log->get_log()));

            auto left = parse_condition(slice(m_tokens, last_i, next_i));

            auto next = std::make_shared<Log>(m_tokens[next_i]->get_line(), left, nullptr, log->get_log());
            if (res == nullptr) {
                res = next;
            } else {
                current->right = next;
            }
            current = next;

            last_i = next_i + 1;
        }

        if (next_of_type_on_line(m_tokens, next_i, lexer::TK_LOG) == m_tokens.size()) {
            current->right = parse_condition(slice(m_tokens, last_i, eol));
            break;
        }
    }

    i = eol - 1;

    return res;
}

/* Parse condition in vector
 * This vector can contain anything, this function only reads from i to the next
 * end of line and updates i towards the end of line */
std::shared_ptr<Cmp> AstContext::parse_condition(const std::vector<std::shared_ptr<lexer::Token>>& ts)
{
    std::shared_ptr<Node> left, right;

    int operator_i = -1;
    size_t i = 0;

    std::shared_ptr<lexer::Cmp> comparator = nullptr;

    std::shared_ptr<Cmp> res;

    /* Iterate to end of line with i_to_eol and save the index of the operator
     * and the operator itself */
    for (; i < ts.size(); i++) {
        auto next = ts[i];

        if (next->get_type() == lexer::TK_CMP) {
            m_c_info.err.on_false(comparator == nullptr, "Found two operators");
            comparator = LEXER_SAFE_CAST(lexer::Cmp, next);
            operator_i = i;
        }
    }
    m_c_info.err.on_true(operator_i == 0, "Expected constant, variable or arithmetic expression");

    if (operator_i != -1) {
        left = parse_arit_expr(slice(ts, 0, operator_i));

        m_c_info.err.on_true((size_t)operator_i + 1 >= i, "Invalid expression");
        right = parse_arit_expr(slice(ts, operator_i + 1, i));

        res = std::make_shared<Cmp>(ts[0]->get_line(), left, right, comparator->get_cmp());
    } else {
        /* Create comparison which is really just one expression, since no
         * comparator was found */
        left = parse_arit_expr(slice(ts, 0, i));
        res = std::make_shared<Cmp>(ts[0]->get_line(), left, nullptr, CMP_OPERATION_ENUM_END);
    }

    return res;
}

/* Wrappers for parse_condition to create if, elif and while */

std::shared_ptr<If> AstContext::parse_condition_to_if(size_t& i, std::shared_ptr<Body> root, bool is_elif)
{
    std::shared_ptr<Node> condition = parse_logical(i);
    return std::make_shared<If>(m_tokens[i]->get_line(), condition,
        make_body(m_tokens[i]->get_line(), root), is_elif);
}

std::shared_ptr<While> AstContext::parse_condition_to_while(size_t& i, std::shared_ptr<Body> root)
{
    std::shared_ptr<Node> condition = parse_logical(i);
    return std::make_shared<While>(m_tokens[i]->get_line(), condition,
        make_body(m_tokens[i]->get_line(), root));
}

/* Parse a vector of tokens to an abstract syntax tree */
std::shared_ptr<Body> AstContext::gen_ast()
{
    std::shared_ptr<Body> root = make_body(m_tokens[0]->get_line(), nullptr);
    std::shared_ptr<Body> saved_root = root;

    std::shared_ptr<If> current_if = nullptr;

    /* Stack of current blocks to go back up the blocks when exiting */
    std::stack<std::shared_ptr<Node>> blk_stk;

    for (size_t i = 0; i < m_tokens.size(); i++) {
        m_c_info.err.set_line(m_tokens[i]->get_line());
        switch (m_tokens[i]->get_type()) {
        case lexer::TK_KEY: {
            auto key = LEXER_SAFE_CAST(lexer::Key, m_tokens[i]);

            switch (key->get_key()) {
            case K_IF: {
                std::shared_ptr<If> new_if = parse_condition_to_if(++i, root, false);
                root->children.push_back(new_if);
                root = new_if->body;

                current_if = new_if;

                blk_stk.push(new_if);
                break;
            }
            case K_ELIF: {
                m_c_info.err.on_true(current_if == nullptr, "Unexpected elif");

                std::shared_ptr<If> new_if = parse_condition_to_if(++i, root, true);

                current_if->elif = new_if;
                root = new_if->body;

                current_if = new_if;
                break;
            }
            case K_ELSE: {
                m_c_info.err.on_true(current_if == nullptr, "Unexpected else");
                m_c_info.err.on_false(m_tokens[i + 1]->get_type() == lexer::TK_EOL,
                    "Else accepts no arguments");

                std::shared_ptr<Else> new_else = std::make_shared<Else>(
                    m_tokens[i]->get_line(),
                    make_body(m_tokens[i]->get_line(), root));

                current_if->elif = new_else;

                root = new_else->body;
                current_if = nullptr;
                break;
            }
            case K_WHILE: {
                std::shared_ptr<While> new_while = parse_condition_to_while(++i, root);
                root->children.push_back(new_while);

                root = new_while->body;

                blk_stk.push(new_while);
                break;
            }
            case K_PRINT:
            case K_EXIT:
            case K_READ:
            case K_SET:
            case K_SETD:
            case K_ADD:
            case K_SUB:
            case K_PUTCHAR:
            case K_INT:
            case K_ARRAY:
            case K_STR:
            case K_BREAK:
            case K_DOUBLE:
            case K_CONT: {
                std::shared_ptr<Func> new_func = std::make_shared<Func>(
                    key->get_line(), key_func_map.at(key->get_key()));
                root->children.push_back(new_func);

                i += 1;

                if (m_tokens[i]->get_type() == lexer::TK_SEP) {
                    break;
                }

                size_t next_sep;
                while ((next_sep = next_of_type_on_line(m_tokens, i, lexer::TK_SEP)) < m_tokens.size()) {
                    switch (m_tokens[i]->get_type()) {
                    case lexer::TK_LSTR: {
                        m_c_info.err.on_true((next_sep - i) > 2, "Excess tokens after string argument");
                        new_func->args.push_back(make_lstr(
                            m_tokens[i]->get_line(),
                            LEXER_SAFE_CAST(lexer::Lstr, m_tokens[i])->ts));
                        break;
                    }
                    case lexer::TK_NUM:
                    case lexer::TK_DOUBLE_NUM:
                    case lexer::TK_VAR:
                    case lexer::TK_ACCESS:
                    case lexer::TK_BRACKET:
                    case lexer::TK_COM_CALL: {
                        std::vector<std::shared_ptr<lexer::Token>> slc = slice(m_tokens, i, next_sep);

                        new_func->args.push_back(parse_arit_expr(slc));
                        break;
                    }
                    default:
                        m_c_info.err.error("Unexpected argument to function: {}",
                            m_tokens[i]->get_type());
                        break;
                    }
                    i = next_sep + 1;
                }

                break;
            }
            case K_END:
                m_c_info.err.on_true(root->parent == nullptr, "Unexpected end");
                if (!blk_stk.empty()) {
                    switch (blk_stk.top()->get_type()) {
                    case T_IF:
                        root = AST_SAFE_CAST(If, blk_stk.top())->body->parent;
                        blk_stk.pop();

                        if (!blk_stk.empty() && blk_stk.top()->get_type() == T_IF)
                            current_if = AST_SAFE_CAST(If, blk_stk.top());
                        break;
                    case T_WHILE:
                        root = AST_SAFE_CAST(While, blk_stk.top())->body->parent;
                        blk_stk.pop();
                        break;
                    default:
                        m_c_info.err.error("Exiting invalid block");
                        break;
                    }
                }
                break;
            default:
            case K_NOKEY:
                UNREACHABLE();
                break;
            }
        }
        case lexer::TK_EOL:
        case lexer::TK_SEP:
            break;
        case lexer::TK_VAR: {
            auto the_var = LEXER_SAFE_CAST(lexer::Var, m_tokens[i]);
            m_c_info.err.error(
                "Unexpected occurence of word expected to be variable: '{}'",
                the_var->get_name());
            break;
        }
        default:
            m_c_info.err.error("Unexpected token with enum value: {}", m_tokens[i]->get_type());
            break;
        }
    }
    m_c_info.err.on_false(root == saved_root, "Unresolved blocks");

    return saved_root;
}

void AstContext::tree_to_dot(std::shared_ptr<Body> root, std::string_view fn)
{
    const std::string temp(fn);

    std::ofstream out(temp);

    fmt::print(out, "digraph AST {{\n");

    int node = 0;

    int tbody_id = root->get_body_id();

    tree_to_dot_core(to_base(root), node, tbody_id, 0, out);

    fmt::print(out, "}}\n");

    out.close();
}

void AstContext::tree_to_dot_core(std::shared_ptr<Node> root, int& node, int& tbody_id, int parent_body_id, std::ofstream& dot)
{
    switch (root->get_type()) {
    case T_BODY: {
        std::shared_ptr<Body> body = AST_SAFE_CAST(Body, root);

        tbody_id = body->get_body_id();
        fmt::print(dot, "\tNode_{}[label=\"body {}\"]\n", tbody_id, tbody_id);
        for (const auto& child : body->children) {
            tree_to_dot_core(child, node, tbody_id, body->get_body_id(), dot);
        }
        break;
    }
    case T_ELSE: {
        std::shared_ptr<Else> t_else = AST_SAFE_CAST(Else, root);

        fmt::print(dot, "\tNode_{} [label=\"else\"]\n", ++node);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"else > body\"]\n", node, tbody_id + 1);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"body > else\"]\n", parent_body_id, node);
        tree_to_dot_core(t_else->body, node, tbody_id, parent_body_id, dot);
        break;
    }
    case T_IF: {
        std::shared_ptr<If> t_if = AST_SAFE_CAST(If, root);

        std::string_view if_name = t_if->is_elif() ? "elif" : "if";

        fmt::print(dot, "\tNode_{}[label=\"{}\"]\n", ++node, if_name);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"body > {}\"]\n", parent_body_id, node, if_name);

        int s_node = node;

        tree_to_dot_core(t_if->condition, node, tbody_id, s_node, dot);

        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"{} > body\"]\n", s_node, tbody_id + 1, if_name);
        tree_to_dot_core(t_if->body, node, tbody_id, parent_body_id, dot);

        if (t_if->elif)
            tree_to_dot_core(t_if->elif, node, tbody_id, s_node, dot);
        break;
    }
    case T_FUNC: {
        std::shared_ptr<Func> t_func = AST_SAFE_CAST(Func, root);

        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, func_str_map.at(t_func->get_func()));
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"func\"]\n", parent_body_id, node);

        int s_node = node;
        for (const auto& arg : t_func->args) {
            tree_to_dot_core(arg, node, tbody_id, s_node, dot);
        }
        break;
    }
    case T_VFUNC: {
        std::shared_ptr<VFunc> t_vfunc = AST_SAFE_CAST(VFunc, root);

        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, vfunc_str_map.at(t_vfunc->get_value_func()));
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"vfunc\"]\n", parent_body_id, node);
        break;
    }
    case T_CMP: {
        std::shared_ptr<Cmp> t_cmp = AST_SAFE_CAST(Cmp, root);

        fmt::print(dot, "\tNode_{} [label=\"cmp\"]\n", ++node);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"cmp\"]\n", parent_body_id, node);

        if (t_cmp->get_cmp() != CMP_OPERATION_ENUM_END) {
            fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, cmp_str_map.at(t_cmp->get_cmp()));
            fmt::print(dot, "\tNode_{} -> Node_{} [label=\"cond\"]\n", node - 1, node);
        }

        int s_node = node;

        if (t_cmp->left)
            tree_to_dot_core(t_cmp->left, node, tbody_id, s_node, dot);
        if (t_cmp->right)
            tree_to_dot_core(t_cmp->right, node, tbody_id, s_node, dot);
        break;
    }
    case T_LOG: {
        std::shared_ptr<Log> log = AST_SAFE_CAST(Log, root);

        fmt::print(dot, "\tNode_{} [label=\"log\"]\n", ++node);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"log\"]\n", parent_body_id, node);
        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, log_str_map.at(log->get_log()));
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"log\"]\n", node - 1, node);

        int s_node = node;

        if (log->left) {
            tree_to_dot_core(log->left, node, tbody_id, s_node, dot);
        }
        if (log->right) {
            tree_to_dot_core(log->right, node, tbody_id, s_node, dot);
        }

        break;
    }
    case T_CONST: {
        std::shared_ptr<Const> num = AST_SAFE_CAST(Const, root);
        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, num->get_value());
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"const\"]\n", parent_body_id, node);
        break;
    }
    case T_DOUBLE_CONST: {
        std::shared_ptr<DoubleConst> num = AST_SAFE_CAST(DoubleConst, root);
        fmt::print(dot, "\tNode_{} [label=\"{:.6f}\"]\n", ++node, num->get_value());
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"const\"]\n", parent_body_id, node);
        break;
    }
    case T_VAR: {
        std::shared_ptr<Var> t_var = AST_SAFE_CAST(Var, root);
        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, t_var->get_var_id());
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"var\"]\n", parent_body_id, node);
        break;
    }
    case T_ACCESS: {
        std::shared_ptr<Access> t_access = AST_SAFE_CAST(Access, root);

        fmt::print(dot, "\tNode_{} [label=\"access\"]\n", ++node);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"access\"]\n", parent_body_id, node);

        int s_node = node;

        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, t_access->get_array_id());
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"array-id\"]\n", node - 1, node);

        tree_to_dot_core(t_access->index, node, tbody_id, s_node, dot);
        break;
    }
    case T_STR: {
        std::shared_ptr<Str> string = AST_SAFE_CAST(Str, root);
        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, string->get_str_id());
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"str\"]\n", parent_body_id, node);
        break;
    }
    case T_ARIT: {
        std::shared_ptr<Arit> t_arit = AST_SAFE_CAST(Arit, root);

        fmt::print(dot, "\tNode_{} [label=\"{}\"]\n", ++node, arit_str_map.at(t_arit->get_arit()));
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"arit\"]\n", parent_body_id, node);

        int s_node = node;

        tree_to_dot_core(t_arit->left, node, tbody_id, s_node, dot);
        tree_to_dot_core(t_arit->right, node, tbody_id, s_node, dot);
        break;
    }
    case T_WHILE: {
        std::shared_ptr<While> t_while = AST_SAFE_CAST(While, root);

        fmt::print(dot, "\tNode_{} [label=\"while\"]\n", ++node);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"body > while\"]\n", parent_body_id, node);

        int s_node = node;

        tree_to_dot_core(t_while->condition, node, tbody_id, s_node, dot);

        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"while > body\"]\n", s_node, tbody_id + 1);

        tree_to_dot_core(t_while->body, node, tbody_id, parent_body_id, dot);
        break;
    }
    case T_LSTR: {
        std::shared_ptr<Lstr> t_lstr = AST_SAFE_CAST(Lstr, root);

        fmt::print(dot, "\tNode_{} [label=\"lstring\"]\n", ++node);
        fmt::print(dot, "\tNode_{} -> Node_{} [label=\"lstring\"]\n", parent_body_id, node);

        int s_node = node;
        for (const auto& format : t_lstr->format) {
            tree_to_dot_core(format, node, tbody_id, s_node, dot);
        }
        break;
    }
    case T_INT_GENERAL:
    case T_DOUBLE_GENERAL:
    case T_BASE:
    default: {
        UNREACHABLE();
        break;
    }
    }
}

std::shared_ptr<Node> get_last_if(std::shared_ptr<If> if_node)
{
    std::shared_ptr<Node> res = if_node;

    for (;; res = AST_SAFE_CAST(If, res)->elif) {
        if (res->get_type() == T_ELSE || AST_SAFE_CAST(If, res)->elif == nullptr) {
            return res;
        }
    }

    return nullptr;
}

} // namespace ast
