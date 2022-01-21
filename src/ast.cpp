#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <stack>
#include <string>
#include <vector>

#include "ast.hpp"
#include "lexer.hpp"
#include "util.hpp"

namespace ast {

void tree_to_dot_core(std::shared_ptr<ast::Node> root, int &node, int &tbody_id,
                      int parent_body_id, std::fstream &dot,
                      CompileInfo &c_info);
std::shared_ptr<Cmp>
parse_condition(const std::vector<std::shared_ptr<lexer::Token>> &ts, CompileInfo &c_info);

std::shared_ptr<Node>
parse_logical(const std::vector<std::shared_ptr<lexer::Token>> &ts, size_t &i,
              CompileInfo &c_info);

const std::map<keyword, func_id> key_func_map = {
    std::make_pair(K_PRINT, F_PRINT),     std::make_pair(K_EXIT, F_EXIT),
    std::make_pair(K_READ, F_READ),       std::make_pair(K_SET, F_SET),
    std::make_pair(K_PUTCHAR, F_PUTCHAR), std::make_pair(K_INT, F_INT),
    std::make_pair(K_STR, F_STR),         std::make_pair(K_ADD, F_ADD),
    std::make_pair(K_SUB, F_SUB),         std::make_pair(K_BREAK, F_BREAK),
    std::make_pair(K_CONT, F_CONT),
};

const std::map<value_func_id, var_type> vfunc_var_type_map = {
    std::make_pair(VF_TIME, V_INT),
    std::make_pair(VF_GETUID, V_INT),
};

/* Maps for converting enum values to strings */

const std::map<cmp_op, std::string> cmp_str_map = {
    std::make_pair(EQUAL, "=="),         std::make_pair(GREATER, ">"),
    std::make_pair(GREATER_OR_EQ, ">="), std::make_pair(LESS, "<"),
    std::make_pair(LESS_OR_EQ, "<="),    std::make_pair(NOT_EQUAL, "!="),
    std::make_pair(CMP_OPERATION_ENUM_END, "no operation"),
};

const std::map<log_op, std::string> log_str_map = {
    std::make_pair(AND, "&&"),         std::make_pair(OR, "||"),
    std::make_pair(LOGICAL_OPS_END, "no log"),
};

const std::map<arit_op, std::string> arit_str_map = {
    std::make_pair(ADD, "+"), std::make_pair(DIV, "/"),
    std::make_pair(MOD, "%"), std::make_pair(MUL, "*"),
    std::make_pair(SUB, "-"),
};

const std::map<keyword, std::string> key_str_map{
    std::make_pair(K_PRINT, "print"), std::make_pair(K_EXIT, "exit"),
    std::make_pair(K_IF, "if"),       std::make_pair(K_ELIF, "elif"),
    std::make_pair(K_ELSE, "else"),   std::make_pair(K_WHILE, "while"),
    std::make_pair(K_END, "end"),     std::make_pair(K_INT, "int"),
    std::make_pair(K_STR, "str"),     std::make_pair(K_READ, "read"),
    std::make_pair(K_SET, "set"),     std::make_pair(K_PUTCHAR, "putchar"),
    std::make_pair(K_ADD, "add"),     std::make_pair(K_SUB, "sub"),
    std::make_pair(K_BREAK, "break"), std::make_pair(K_CONT, "continue"),
};

Body::Body(int line, std::shared_ptr<Body> t_parent, CompileInfo &c_info)
    : Node(line), parent(t_parent)
{
    m_body_id = c_info.get_next_body_id();
}

Lstr::Lstr(int line, const std::vector<std::shared_ptr<lexer::Token>> &ts,
    CompileInfo &c_info)
    : Node(line)
{
    for (const auto &tk : ts) {
        switch (tk->get_type()) {
        case lexer::TK_STR:
        {
            auto string = lexer::safe_cast<lexer::Str>(tk);
            format.push_back(std::make_shared<Str>(
                line, c_info.check_str(string->get_str())));
            break;
        }
        case lexer::TK_VAR:
        {
            auto token_var = lexer::safe_cast<lexer::Var>(tk);
            format.push_back(std::make_shared<Var>(
                line, c_info.check_var(token_var->get_name())));
            break;
        }
        case lexer::TK_NUM:
        {
            auto cnst = lexer::safe_cast<lexer::Num>(tk);
            format.push_back(std::make_shared<Const>(line, cnst->get_num()));
            break;
        }
        default:
            assert(false);
            break;
        }
    }
}

FunctionSpec::FunctionSpec(
    const std::string &t_name, size_t t_len,
    const std::vector<ts_class> &t_types, const std::vector<var_type> &t_info,
    const std::vector<std::pair<size_t, var_type>> &t_def)
    : name(t_name), exp_arg_len(t_len), types(t_types), info(t_info),
      define(t_def)
{}

/* Give information about how a correct function call looks like and check for
 * it */
void check_correct_function_call(const FunctionSpec &spec,
                                 const std::vector<std::shared_ptr<Node>> &args,
                                 CompileInfo &c_info)
{
    /* Caller can provide arguments which will be defined by this function
     * We define them so we don't error out later */
    if (!spec.define.empty()) {
        for (const auto &d : spec.define) {
            assert(d.first < spec.exp_arg_len);

            c_info.err.on_false(args[d.first]->get_type() == T_VAR,
                                "Argument % to '%' expected to be variable\n",
                                d.first, spec.name);
            auto t_var = safe_cast<Var>(args[d.first]);

            c_info.err.on_true(c_info.known_vars[t_var->get_var_id()].defined,
                               "Argument % to '%' expected to be undefined\n",
                               d.first, spec.name);
            c_info.known_vars[safe_cast<Var>(args[d.first])->get_var_id()]
                .defined = true;
            c_info.known_vars[safe_cast<Var>(args[d.first])->get_var_id()]
                .type = d.second;
        }
    }

    auto info_it = spec.info.begin();

    c_info.err.on_false(args.size() == spec.exp_arg_len,
                        "Expected % arguments to function '%', got %\n",
                        spec.exp_arg_len, spec.name, args.size());
    assert(spec.types.size() == spec.exp_arg_len);

    for (size_t i = 0; i < args.size(); i++) {
        const auto &arg = args[i];

        if (spec.types[i] == T_NUM_GENERAL) {
            /* NUM_GENERAL means that arg has to evaluate to a number */
            c_info.err.on_false(
                arg->get_type() == T_VAR || arg->get_type() == T_CONST ||
                    arg->get_type() == T_ARIT || arg->get_type() == T_VFUNC,
                "Argument % to '%' has to evaluate to a number\n", i, spec.name);

            /* If we are var: check that we are int */
            if (arg->get_type() == T_VAR) {
                auto t_var = safe_cast<Var>(args[i]);
                auto v_info = c_info.known_vars[t_var->get_var_id()];

                c_info.err.on_false(v_info.defined,
                                    "Var '%' is undefined at this time\n",
                                    v_info.name);
                c_info.err.on_false(
                    v_info.type == V_INT,
                    "Argument % to '%' has to have type '%' but has '%'\n", i,
                    spec.name, var_type_str_map.at(V_INT),
                    var_type_str_map.at(v_info.type));
            } else if (arg->get_type() == T_VFUNC) {
                auto vfunc = safe_cast<VFunc>(args[i]);
                c_info.err.on_false(
                    vfunc->get_return_type() == V_INT,
                    "Argument % to '%' has to evaluate to a number\n"
                    "Got '%' returning '%'\n",
                    i, spec.name, vfunc_str_map.at(vfunc->get_value_func()),
                    var_type_str_map.at(vfunc->get_return_type()));
            }
        } else {
            c_info.err.on_false(arg->get_type() == spec.types[i],
                                "Argument % to function '%' is wrong type\n", i,
                                spec.name);
        }

        /* If we are a var: check the provided 'info' type information if the
         * type is correct */
        if (arg->get_type() == T_VAR && spec.types[i] != T_NUM_GENERAL) {
            auto t_var = safe_cast<Var>(args[i]);
            auto v_info = c_info.known_vars[t_var->get_var_id()];

            c_info.err.on_true(info_it == spec.info.end() || spec.info.empty(),
                               "Could not parse arguments to function '%'\n",
                               spec.name);

            c_info.err.on_false(v_info.defined,
                                "Var '%' is undefined at this time\n",
                                v_info.name);
            c_info.err.on_false(v_info.type == *info_it.base(),
                                "Argument % to '%' has to have type '%' but has '%'\n", i,
                                v_info.name, var_type_str_map.at(v_info.type),
                                var_type_str_map.at(*info_it.base()));

            info_it++;
        }
    }
}

/* Tree node from variable or constant */
std::shared_ptr<ast::Node>
node_from_numeric_token(std::shared_ptr<lexer::Token> tk, CompileInfo &c_info)
{
    assert(tk->get_type() == lexer::TK_VAR || tk->get_type() == lexer::TK_NUM ||
           tk->get_type() == lexer::TK_CALL);

    std::shared_ptr<ast::Node> res;

    if (tk->get_type() == lexer::TK_VAR) {
        res = std::make_shared<ast::Var>(
            tk->get_line(),
            c_info.check_var(lexer::safe_cast<lexer::Var>(tk)->get_name()));
    } else if (tk->get_type() == lexer::TK_NUM) {
        res = std::make_shared<ast::Const>(
            tk->get_line(), lexer::safe_cast<lexer::Num>(tk)->get_num());
    } else if (tk->get_type() == lexer::TK_CALL) {
        const auto &call = lexer::safe_cast<lexer::Call>(tk);

        var_type ret_type = vfunc_var_type_map.at(call->get_value_func());
        c_info.err.on_false(ret_type == V_INT,
                            "'%' does not return an integer\n",
                            vfunc_str_map.at(call->get_value_func()));

        res = std::make_shared<ast::VFunc>(tk->get_line(), call->get_value_func(), ret_type);
    }

    return res;
}

/* Ensure that the pattern: 'num op num op num ...' is met */
void ensure_arit_correctness(const std::vector<std::shared_ptr<lexer::Token>> &ts,
                             CompileInfo &c_info)
{
    bool expect_operator = false;
    for (size_t i = 0; i < ts.size(); i++) {
        const auto &t = ts[i];

        if (expect_operator) {
            c_info.err.on_false(t->get_type() == lexer::TK_ARIT, "Expected arithmetic operator\n");
        } else {
            c_info.err.on_false(t->get_type() == lexer::TK_NUM ||
                                t->get_type() == lexer::TK_VAR ||
                                t->get_type() == lexer::TK_CALL, "Expected variable, constant or inline call\n");
        }

        expect_operator = !expect_operator;
    }
}

/* Parse arithmetic expression respecting precedence */

std::shared_ptr<Node>
parse_arit_expr(const std::vector<std::shared_ptr<lexer::Token>> &ts,
                CompileInfo &c_info)
{
    ensure_arit_correctness(ts, c_info);

    std::vector<std::shared_ptr<Node>> s2;

    arit_op last_op = ARIT_OPERATION_ENUM_END;

    int len = ts.size();

    /* Group '/' and '*' expressions together and leave '+', '-' and constants
     * in place */
    for (int i = 0; i < len; i++) {
        /* If we have '/' or '*' next and are not currently an operator:
         * continue because we belong to that operator */
        arit_op next_op = ARIT_OPERATION_ENUM_END;
        for (int j = i + 1; j < len; j++) {
            if (ts[j]->get_type() == lexer::TK_ARIT) {
                next_op = lexer::safe_cast<lexer::Arit>(ts[j])->get_op();
                break;
            }
        }
        /* We are part of the next operation, since it has precedence */
        if (has_precedence(next_op) && ts[i]->get_type() != lexer::TK_ARIT) {
            continue;
        }

        if (ts[i]->get_type() == lexer::TK_ARIT) {
            auto op = safe_cast<lexer::Arit>(ts[i]);
            /* Make new multiplication */
            if (has_precedence(op->get_op())) {
                assert(i > 0 && i + 1 < len);
                c_info.err.on_false(ts[i - 1]->get_type() == lexer::TK_NUM ||
                                    ts[i - 1]->get_type() == lexer::TK_CALL ||
                                        ts[i - 1]->get_type() == lexer::TK_VAR,
                                    "Expected number before '%' operator\n",
                                    arit_str_map.at(op->get_op()));
                c_info.err.on_false(ts[i + 1]->get_type() == lexer::TK_NUM ||
                                    ts[i + 1]->get_type() == lexer::TK_CALL ||
                                        ts[i + 1]->get_type() == lexer::TK_VAR,
                                    "Expected number after '%' operator\n",
                                    arit_str_map.at(op->get_op()));

                if (has_precedence(last_op)) {
                    /* If we follow another multiplication:
                     * incorporate previous into ourselves and replace that
                     * previous one in the array */
                    std::shared_ptr<Arit> new_arit = std::make_shared<Arit>(
                        op->get_line(), s2.back(),
                        node_from_numeric_token(ts[i + 1], c_info),
                        op->get_op());
                    s2.back() = new_arit;
                } else {
                    /* We follow a +/-, hence we can freely take the last number
                     */
                    s2.push_back(std::make_shared<Arit>(
                        op->get_line(),
                        node_from_numeric_token(ts[i - 1], c_info),
                        node_from_numeric_token(ts[i + 1], c_info),
                        op->get_op()));
                }
            } else {
                /* Make empty +/-, filled in stage two */
                s2.push_back(std::make_shared<Arit>(op->get_line(), nullptr,
                                                    nullptr, op->get_op()));
            }
            last_op = op->get_op();
        } else {
            /* If we follow a multiplication:
             * we are already part of a multiplication and don't need us in the
             * array */
            if (has_precedence(last_op))
                continue;
            s2.push_back(node_from_numeric_token(ts[i], c_info));
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
            std::shared_ptr<Arit> cur_arit = safe_cast<Arit>(s2[i]);
            if (!has_precedence(cur_arit->get_arit())) {
                if (!root) {
                    root =
                        std::make_shared<Arit>(cur_arit->get_line(), s2[i - 1],
                                               nullptr, cur_arit->get_arit());
                    current = root;
                } else {
                    std::shared_ptr<Arit> new_cur =
                        std::make_shared<Arit>(cur_arit->get_line(), s2[i - 1],
                                               nullptr, cur_arit->get_arit());
                    current->right = new_cur;
                    current = new_cur;
                }

                if (i + 1 >= s2.size() - 1) {
                    /* If we are the last thing: set our own right to the next
                     * number */
                    c_info.err.on_true(i + 1 > s2.size() - 1,
                                       "Expected number after operand '%'\n",
                                       arit_str_map.at(cur_arit->get_arit()));
                    current->right = s2[i + 1];
                } else if (s2[i + 1]->get_type() == T_ARIT) {
                    std::shared_ptr<Arit> next_arit =
                        safe_cast<Arit>(s2[i + 1]);
                    c_info.err.on_false(has_precedence(next_arit->get_arit()),
                                        "+/- followed by another +/-\n");
                }
            }
        }
    }

    return root;
}

/* Parse logical expression (a && b || c) starting from i and update
 * i to end of line */
std::shared_ptr<Node>
parse_logical(const std::vector<std::shared_ptr<lexer::Token>> &ts, size_t &i,
              CompileInfo &c_info)
{
    size_t last_i, next_i, eol;
    last_i = next_i = i;

    eol = next_of_type_on_line(ts, i, lexer::TK_EOL);

    if(next_of_type_on_line(ts, i, lexer::TK_LOG) == ts.size()) {
        auto res = parse_condition(slice(ts, i, eol), c_info);
        i = eol - 1;

        return res;
    }

    std::shared_ptr<Log> res = nullptr;
    std::shared_ptr<Log> current = nullptr;

    for (; ts[next_i]->get_type() != lexer::TK_EOL; next_i++) {
        if (ts[next_i]->get_type() == lexer::TK_LOG) {
            auto log = safe_cast<lexer::Log>(ts[next_i]);

            c_info.err.on_true(ts[next_i+1]->get_type() == lexer::TK_EOL ||
                               ts[next_i+1]->get_type() == lexer::TK_LOG,
                               "Expected number after '%'\n", log_str_map.at(log->get_log()));
            c_info.err.on_true(next_i == i, "'%' not expected at beginning of expression\n",
                               log_str_map.at(log->get_log()));

            auto left = parse_condition(slice(ts, last_i, next_i), c_info);

            auto next = std::make_shared<Log>(ts[next_i]->get_line(), left, nullptr, log->get_log());
            if (res == nullptr) {
                res = next;
            } else {
                current->right = next;
            }
            current = next;

            last_i = next_i + 1;
        }

        if (next_of_type_on_line(ts, next_i, lexer::TK_LOG) == ts.size()) {
            current->right = parse_condition(slice(ts, last_i, eol), c_info);
            break;
        }
    }

    i = eol - 1;

    return res;
}

/* Parse condition in vector
 * This vector can contain anything, this function only reads from i to the next
 * end of line and updates i towards the end of line */
std::shared_ptr<Cmp>
parse_condition(const std::vector<std::shared_ptr<lexer::Token>> &ts, CompileInfo &c_info)
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
            c_info.err.on_false(comparator == nullptr, "Found two operators\n");
            comparator = lexer::safe_cast<lexer::Cmp>(next);
            operator_i = i;
        }
    }
    c_info.err.on_true(
        operator_i == 0,
        "Expected constant, variable or arithmetic expression\n");

    if (operator_i != -1) {
        left = parse_arit_expr(slice(ts, 0, operator_i), c_info);

        c_info.err.on_true((size_t)operator_i + 1 >= i, "Invalid expression\n");
        right = parse_arit_expr(slice(ts, operator_i + 1, i), c_info);

        res = std::make_shared<Cmp>(ts[0]->get_line(), left, right,
                                    comparator->get_cmp());
    } else {
        /* Create comparison which is really just one expression, since no
         * comparator was found */
        left = parse_arit_expr(slice(ts, 0, i), c_info);
        res = std::make_shared<Cmp>(ts[0]->get_line(), left, nullptr,
                                    CMP_OPERATION_ENUM_END);
    }

    return res;
}

/* Wrappers for parse_condition to create if, elif and while */

std::shared_ptr<If>
parse_condition_to_if(const std::vector<std::shared_ptr<lexer::Token>> &ts, size_t &i,
                      std::shared_ptr<Body> root, CompileInfo &c_info,
                      bool is_elif)
{
    std::shared_ptr<Node> condition = parse_logical(ts, i, c_info);
    return std::make_shared<If>(
        ts[i]->get_line(), condition,
        std::make_shared<Body>(ts[i]->get_line(), root, c_info), is_elif);
}

std::shared_ptr<While>
parse_condition_to_while(const std::vector<std::shared_ptr<lexer::Token>> &ts,
                         size_t &i, std::shared_ptr<Body> root,
                         CompileInfo &c_info)
{
    std::shared_ptr<Node> condition = parse_logical(ts, i, c_info);
    return std::make_shared<While>(
        ts[i]->get_line(), condition,
        std::make_shared<Body>(ts[i]->get_line(), root, c_info));
}

/* Parse a vector of tokens to an abstract syntax tree */
std::shared_ptr<Body> gen_ast(const std::vector<std::shared_ptr<lexer::Token>> &tokens,
                              CompileInfo &c_info)
{
    std::shared_ptr<Body> root =
        std::make_shared<Body>(tokens[0]->get_line(), nullptr, c_info);
    std::shared_ptr<Body> saved_root = root;

    std::shared_ptr<If> current_if = nullptr;

    /* Stack of current blocks to go back up the blocks when exiting */
    std::stack<std::shared_ptr<Node>> blk_stk;

    for (size_t i = 0; i < tokens.size(); i++) {
        c_info.err.set_line(tokens[i]->get_line());
        switch (tokens[i]->get_type()) {
        case lexer::TK_KEY:
        {
            auto key = safe_cast<lexer::Key>(tokens[i]);

            switch (key->get_key()) {
            case K_IF:
            {
                std::shared_ptr<If> new_if =
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

                std::shared_ptr<If> new_if =
                    parse_condition_to_if(tokens, ++i, root, c_info, true);

                current_if->elif = new_if;
                root = new_if->body;

                current_if = new_if;
                break;
            }
            case K_ELSE:
            {
                c_info.err.on_true(current_if == nullptr, "Unexpected else");
                c_info.err.on_false(tokens[i + 1]->get_type() == lexer::TK_EOL,
                                    "Else accepts no arguments");

                std::shared_ptr<Else> new_else = std::make_shared<Else>(
                    tokens[i]->get_line(),
                    std::make_shared<Body>(tokens[i]->get_line(), root,
                                           c_info));

                current_if->elif = new_else;

                root = new_else->body;
                current_if = nullptr;
                break;
            }
            case K_WHILE:
            {
                std::shared_ptr<While> new_while =
                    parse_condition_to_while(tokens, ++i, root, c_info);
                root->children.push_back(new_while);

                root = new_while->body;

                blk_stk.push(new_while);
                break;
            }
            case K_PRINT:
            case K_EXIT:
            case K_READ:
            case K_SET:
            case K_ADD:
            case K_SUB:
            case K_PUTCHAR:
            case K_INT:
            case K_STR:
            case K_BREAK:
            case K_CONT:
            {
                std::shared_ptr<Func> new_func = std::make_shared<Func>(
                    key->get_line(), key_func_map.at(key->get_key()));
                root->children.push_back(new_func);

                i += 1;

                if (tokens[i]->get_type() == lexer::TK_SEP) {
                    break;
                }

                size_t next_sep;
                while ((next_sep = next_of_type_on_line(
                            tokens, i, lexer::TK_SEP)) < tokens.size()) {
                    switch (tokens[i]->get_type()) {
                    case lexer::TK_LSTR:
                    {
                        c_info.err.on_true(
                            (next_sep - i) > 2,
                            "Excess tokens after string argument\n");
                        new_func->args.push_back(std::make_shared<Lstr>(
                            tokens[i]->get_line(),
                            lexer::safe_cast<lexer::Lstr>(tokens[i])->ts,
                            c_info));
                        break;
                    }
                    case lexer::TK_NUM:
                    case lexer::TK_VAR:
                    case lexer::TK_CALL:
                    {
                        std::vector<std::shared_ptr<lexer::Token>> slc =
                            slice(tokens, i, next_sep);

                        new_func->args.push_back(parse_arit_expr(slc, c_info));
                        break;
                    }
                    default:
                        c_info.err.error("Unexpected argument to function: %\n",
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
                        root = safe_cast<If>(blk_stk.top())->body->parent;
                        blk_stk.pop();

                        if (!blk_stk.empty() &&
                            blk_stk.top()->get_type() == T_IF)
                            current_if = safe_cast<If>(blk_stk.top());
                        break;
                    case T_WHILE:
                        root = safe_cast<While>(blk_stk.top())->body->parent;
                        blk_stk.pop();
                        break;
                    default:
                        c_info.err.error("Exiting invalid block\n");
                        break;
                    }
                }
                break;
            default:
            case K_NOKEY:
                c_info.err.error("Invalid instruction\n");
                break;
            }
        }
        case lexer::TK_EOL:
        case lexer::TK_SEP:
            break;
        case lexer::TK_VAR:
        {
            auto the_var = lexer::safe_cast<lexer::Var>(tokens[i]);
            c_info.err.error(
                "Unexpected occurence of word expected to be variable: '%'\n",
                the_var->get_name());
            break;
        }
        default:
            c_info.err.error("Unexpected token with enum value: %\n",
                             tokens[i]->get_type());
            break;
        }
    }
    c_info.err.on_false(root == saved_root, "Unresolved blocks\n");

    return saved_root;
}

std::shared_ptr<Node> get_last_if(std::shared_ptr<If> if_node)
{
    std::shared_ptr<Node> res = if_node;

    for (;; res = safe_cast<If>(res)->elif) {
        if (res->get_type() == T_ELSE || safe_cast<If>(res)->elif == nullptr) {
            return res;
        }
    }

    return nullptr;
}

void tree_to_dot(std::shared_ptr<Body> root, std::string fn,
                 CompileInfo &c_info)
{
    std::fstream out;
    out.open(fn, std::ios::out);

    out << "digraph AST {\n";

    int node = 0;

    int tbody_id = root->get_body_id();

    tree_to_dot_core(to_base(root), node, tbody_id, 0, out, c_info);

    out << "}\n";

    out.close();
}

void tree_to_dot_core(std::shared_ptr<Node> root, int &node, int &tbody_id,
                      int parent_body_id, std::fstream &dot,
                      CompileInfo &c_info)
{
    switch (root->get_type()) {
    case T_BODY:
    {
        std::shared_ptr<Body> body = safe_cast<Body>(root);

        tbody_id = body->get_body_id();
        dot << "\tNode_" << tbody_id << "[label=\"body " << tbody_id << "\"]\n";
        for (const auto &child : body->children) {
            tree_to_dot_core(child, node, tbody_id, body->get_body_id(), dot,
                             c_info);
        }
        break;
    }
    case T_ELSE:
    {
        std::shared_ptr<Else> t_else = safe_cast<Else>(root);

        dot << "\tNode_" << ++node << " [label=\"else\"]\n";
        dot << "\tNode_" << node << " -> Node_" << tbody_id + 1
            << " [label=\"else > body\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body > else\"]\n";
        tree_to_dot_core(t_else->body, node, tbody_id, parent_body_id, dot,
                         c_info);
        break;
    }
    case T_IF:
    {
        std::shared_ptr<If> t_if = safe_cast<If>(root);

        const std::string if_name = t_if->is_elif() ? "elif" : "if";

        dot << "\tNode_" << ++node << "[label=\"" << if_name << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body >" << if_name << "\"]\n";

        int s_node = node;

        tree_to_dot_core(t_if->condition, node, tbody_id, s_node, dot,
                         c_info);

        dot << "\tNode_" << s_node << " -> Node_" << tbody_id + 1
            << " [label=\"" << if_name << " > body\"]\n";
        tree_to_dot_core(t_if->body, node, tbody_id, parent_body_id, dot,
                         c_info);

        if (t_if->elif)
            tree_to_dot_core(t_if->elif, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_FUNC:
    {
        std::shared_ptr<Func> t_func = safe_cast<Func>(root);

        dot << "\tNode_" << ++node << " [label=\""
            << func_str_map.at(t_func->get_func()) << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"func\"]\n";

        int s_node = node;
        for (const auto &arg : t_func->args) {
            tree_to_dot_core(arg, node, tbody_id, s_node, dot, c_info);
        }
        break;
    }
    case T_VFUNC:
    {
        std::shared_ptr<VFunc> t_vfunc = safe_cast<VFunc>(root);

        dot << "\tNode_" << ++node << " [label=\""
            << vfunc_str_map.at(t_vfunc->get_value_func()) << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"vfunc\"]\n";
        break;
    }
    case T_CMP:
    {
        std::shared_ptr<Cmp> t_cmp = safe_cast<Cmp>(root);

        dot << "\tNode_" << ++node << " [label=\"cmp\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"cmp\"]\n";
        dot << "\tNode_" << ++node << " [label=\""
            << cmp_str_map.at(t_cmp->get_cmp()) << "\"]\n";
        dot << "\tNode_" << node - 1 << " -> Node_" << node
            << " [label=\"cond\"]\n";

        int s_node = node;

        if (t_cmp->left)
            tree_to_dot_core(t_cmp->left, node, tbody_id, s_node, dot, c_info);
        if (t_cmp->right)
            tree_to_dot_core(t_cmp->right, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_LOG:
    {
        std::shared_ptr<Log> log = safe_cast<Log>(root);

        dot << "\tNode_" << ++node << " [label=\"log\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"log\"]\n";
        dot << "\tNode_" << ++node << " [label=\""
            << log_str_map.at(log->get_log()) << "\"]\n";
        dot << "\tNode_" << node - 1 << " -> Node_" << node
            << " [label=\"log\"]\n";

        int s_node = node;

        if (log->left) {
            tree_to_dot_core(log->left, node, tbody_id, s_node, dot, c_info);
        }
        if (log->right) {
            tree_to_dot_core(log->right, node, tbody_id, s_node, dot, c_info);
        }

        break;
    }
    case T_CONST:
    {
        std::shared_ptr<Const> num = safe_cast<Const>(root);
        dot << "\tNode_" << ++node << " [label=\"" << num->get_value()
            << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"const\"]\n";
        break;
    }
    case T_VAR:
    {
        std::shared_ptr<Var> t_var = safe_cast<Var>(root);
        dot << "\tNode_" << ++node << " [label=\"" << t_var->get_var_id()
            << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"var\"]\n";
        break;
    }
    case T_STR:
    {
        std::shared_ptr<Str> string = safe_cast<Str>(root);
        dot << "\tNode_" << ++node << " [label=\"" << string->get_str_id()
            << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"str\"]\n";
        break;
    }
    case T_ARIT:
    {
        std::shared_ptr<Arit> t_arit = safe_cast<Arit>(root);

        dot << "\tNode_" << ++node << " [label=\""
            << arit_str_map.at(t_arit->get_arit()) << "\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"arit\"]\n";

        int s_node = node;

        tree_to_dot_core(t_arit->left, node, tbody_id, s_node, dot, c_info);
        tree_to_dot_core(t_arit->right, node, tbody_id, s_node, dot, c_info);
        break;
    }
    case T_WHILE:
    {
        std::shared_ptr<While> t_while = safe_cast<While>(root);

        dot << "\tNode_" << ++node << " [label=\"while\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"body > while\"]\n";

        int s_node = node;

        tree_to_dot_core(t_while->condition, node, tbody_id, s_node,
                         dot, c_info);

        dot << "\tNode_" << s_node << " -> Node_" << tbody_id + 1
            << " [label=\"while > body\"]\n";

        tree_to_dot_core(t_while->body, node, tbody_id, parent_body_id, dot,
                         c_info);
        break;
    }
    case T_LSTR:
    {
        std::shared_ptr<Lstr> t_lstr = safe_cast<Lstr>(root);

        dot << "\tNode_" << ++node << " [label=\"lstring\"]\n";
        dot << "\tNode_" << parent_body_id << " -> Node_" << node
            << " [label=\"lstring\"]\n";

        int s_node = node;
        for (const auto &format : t_lstr->format) {
            tree_to_dot_core(format, node, tbody_id, s_node, dot, c_info);
        }
        break;
    }
    case T_NUM_GENERAL:
    case T_BASE:
    default:
    {
        c_info.err.error("Invalid tree node\n");
        break;
    }
    }
}

} // namespace ast
