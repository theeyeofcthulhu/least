#include <memory>
#include <cassert>

#include "ast.hpp"
#include "semantics.hpp"
#include "error.hpp"
#include "util.hpp"

namespace semantic {

/* How each function needs to be called */
const std::map<func_id, FunctionSpec> func_spec_map = {
    std::make_pair<func_id, FunctionSpec>(F_PRINT, { "print", 1, { ast::T_LSTR }, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_EXIT, { "exit", 1, { ast::T_NUM_GENERAL }, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_READ, { "read", 1, { ast::T_VAR }, { V_STR }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_SET, { "set", 2, { ast::T_IN_MEMORY, ast::T_NUM_GENERAL }, { V_INT }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_ADD, { "add", 2, { ast::T_IN_MEMORY, ast::T_NUM_GENERAL }, { V_INT }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_SUB, { "sub", 2, { ast::T_IN_MEMORY, ast::T_NUM_GENERAL }, { V_INT }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_BREAK, { "break", 0, {}, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_CONT, { "continue", 0, {}, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_PUTCHAR, { "putchar", 1, { ast::T_NUM_GENERAL }, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_INT, { "int", 2, { ast::T_VAR, ast::T_NUM_GENERAL }, { V_INT }, { { 0, V_INT } } }),
    std::make_pair<func_id, FunctionSpec>(F_ARRAY, { "array", 2, { ast::T_VAR, ast::T_CONST }, { V_ARR }, { { 0, V_ARR } } }),
    std::make_pair<func_id, FunctionSpec>(F_STR, { "str", 1, { ast::T_VAR }, { V_STR }, { { 0, V_STR } } }),
};

/* Give information about how a correct function call looks like and check for
 * it */
void check_correct_function_call(const FunctionSpec& spec,
    const std::vector<std::shared_ptr<ast::Node>>& args,
    CompileInfo& c_info)
{
    /* Caller can provide arguments which will be defined by this function
     * We define them so we don't error out later */
    if (!spec.define.empty()) {
        for (const auto& d : spec.define) {
            assert(d.first < spec.exp_arg_len);

            c_info.err.on_false(args[d.first]->get_type() == ast::T_VAR,
                "Argument % to '%' expected to be variable\n", d.first, spec.name);
            auto t_var = AST_SAFE_CAST(ast::Var, args[d.first]);

            c_info.err.on_true(c_info.known_vars[t_var->get_var_id()].defined,
                "Argument % to '%' expected to be undefined\n", d.first, spec.name);
            c_info.known_vars[t_var->get_var_id()].defined = true;
            c_info.known_vars[t_var->get_var_id()].type = d.second;

            if (d.second == V_ARR) {
                c_info.known_vars[t_var->get_var_id()].arrayness = VarInfo::Arrayness::Yes;
            }
        }
    }

    auto info_it = spec.info.begin();

    c_info.err.on_false(args.size() == spec.exp_arg_len,
        "Expected % arguments to function '%', got %\n", spec.exp_arg_len,
        spec.name, args.size());
    assert(spec.types.size() == spec.exp_arg_len);

    for (size_t i = 0; i < args.size(); i++) {
        const auto& arg = args[i];

        if (spec.types[i] == ast::T_NUM_GENERAL) {
            /* NUM_GENERAL means that arg has to evaluate to a number */
            c_info.err.on_false(arg->get_type() == ast::T_VAR || arg->get_type() == ast::T_CONST || arg->get_type() == ast::T_ARIT || arg->get_type() == ast::T_VFUNC,
                "Argument % to '%' has to evaluate to a number\n", i, spec.name);

            /* If we are var: check that we are int */
            if (arg->get_type() == ast::T_VAR) {
                auto t_var = AST_SAFE_CAST(ast::Var, args[i]);
                auto v_info = c_info.known_vars[t_var->get_var_id()];

                c_info.err.on_false(v_info.defined, "Var '%' is undefined at this time\n",
                    v_info.name);
                c_info.err.on_false(
                    v_info.type == V_INT, "Argument % to '%' has to have type '%' but has '%'\n", i,
                    spec.name, var_type_str_map.at(V_INT), var_type_str_map.at(v_info.type));
            } else if (arg->get_type() == ast::T_VFUNC) {
                auto vfunc = AST_SAFE_CAST(ast::VFunc, args[i]);
                c_info.err.on_false(vfunc->get_return_type() == V_INT,
                    "Argument % to '%' has to evaluate to a number\n"
                    "Got '%' returning '%'\n",
                    i, spec.name, vfunc_str_map.at(vfunc->get_value_func()),
                    var_type_str_map.at(vfunc->get_return_type()));
            }
        } else if (spec.types[i] == ast::T_IN_MEMORY) {
            c_info.err.on_false(arg->get_type() == ast::T_VAR || arg->get_type() == ast::T_ACCESS, "Argument % to '%' has to have a memory address\n", i, spec.name);

            /* If we are var: check that we are int */
            if (arg->get_type() == ast::T_VAR) {
                auto t_var = AST_SAFE_CAST(ast::Var, args[i]);
                auto v_info = c_info.known_vars[t_var->get_var_id()];

                c_info.err.on_false(v_info.defined, "Var '%' is undefined at this time\n",
                    v_info.name);
                c_info.err.on_false(
                    v_info.type == V_INT, "Argument % to '%' has to have type '%' but has '%'\n", i,
                    spec.name, var_type_str_map.at(V_INT), var_type_str_map.at(v_info.type));
            } else if (arg->get_type() == ast::T_ACCESS) {
                auto access = AST_SAFE_CAST(ast::Access, args[i]);
                auto v_info = c_info.known_vars[access->get_array_id()];

                c_info.err.on_false(v_info.defined, "Var '%' is undefined at this time\n",
                    v_info.name);
                c_info.err.on_false(
                    v_info.type == V_ARR, "Argument % to '%' has to have type '%' but has '%'\n", i,
                    spec.name, var_type_str_map.at(V_ARR), var_type_str_map.at(v_info.type));
            }
        } else {
            c_info.err.on_false(arg->get_type() == spec.types[i],
                "Argument % to function '%' is wrong type\n", i, spec.name);
        }

        /* If we are a var: check the provided 'info' type information if the
         * type is correct */
        if (arg->get_type() == ast::T_VAR && spec.types[i] != ast::T_NUM_GENERAL) {
            auto t_var = AST_SAFE_CAST(ast::Var, args[i]);
            auto v_info = c_info.known_vars[t_var->get_var_id()];

            c_info.err.on_true(info_it == spec.info.end() || spec.info.empty(),
                "Could not parse arguments to function '%'\n", spec.name);

            c_info.err.on_false(v_info.defined, "Var '%' is undefined at this time\n", v_info.name);
            c_info.err.on_false(v_info.type == *info_it.base(),
                "Argument % to '%' has to have type '%' but has '%'\n", i,
                v_info.name, var_type_str_map.at(v_info.type),
                var_type_str_map.at(*info_it.base()));

            info_it++;
        }
    }
}

void semantic_analysis(std::shared_ptr<ast::Node> root, CompileInfo &c_info)
{
    c_info.err.set_line(root->get_line());

    switch (root->get_type()) {
    case ast::T_BODY: {
        std::shared_ptr<ast::Body> body = AST_SAFE_CAST(ast::Body, root);

        for (const auto& child : body->children) {
            semantic_analysis(child, c_info);
        }
        break;
    }
    case ast::T_ELSE: {
        std::shared_ptr<ast::Else> t_else = AST_SAFE_CAST(ast::Else, root);

        semantic_analysis(t_else->body, c_info);
        break;
    }
    case ast::T_IF: {
        std::shared_ptr<ast::If> t_if = AST_SAFE_CAST(ast::If, root);

        semantic_analysis(t_if->condition, c_info);

        if (t_if->elif)
            semantic_analysis(t_if->elif, c_info);
        break;
    }
    case ast::T_FUNC: {
        std::shared_ptr<ast::Func> t_func = AST_SAFE_CAST(ast::Func, root);

        check_correct_function_call(func_spec_map.at(t_func->get_func()), t_func->args, c_info);
        for (const auto& arg : t_func->args) {
            semantic_analysis(arg, c_info);
        }

        switch (t_func->get_func()) {
            case F_INT: {
                auto t_var = AST_SAFE_CAST(ast::Var, t_func->args[0]);
                c_info.known_vars[t_var->get_var_id()].stack_offset = c_info.get_stack_size_and_append(1);
                break;
            }
            case F_ARRAY: {
                auto t_var = AST_SAFE_CAST(ast::Var, t_func->args[0]);
                auto t_size = AST_SAFE_CAST(ast::Const, t_func->args[1]);

                c_info.known_vars[t_var->get_var_id()].stack_offset = c_info.get_stack_size_and_append(t_size->get_value());
                c_info.known_vars[t_var->get_var_id()].stack_units = t_size->get_value();
                break;
            }
            default:
                break;
        }
        break;
    }
    case ast::T_VFUNC: {
        std::shared_ptr<ast::VFunc> t_vfunc = AST_SAFE_CAST(ast::VFunc, root);

        /* NOTE: do we need to do anything? */
        break;
    }
    case ast::T_CMP: {
        std::shared_ptr<ast::Cmp> t_cmp = AST_SAFE_CAST(ast::Cmp, root);

        if (t_cmp->left)
            semantic_analysis(t_cmp->left, c_info);
        if (t_cmp->right)
            semantic_analysis(t_cmp->right, c_info);
        break;
    }
    case ast::T_LOG: {
        std::shared_ptr<ast::Log> log = AST_SAFE_CAST(ast::Log, root);

        if (log->left)
            semantic_analysis(log->left, c_info);
        if (log->right)
            semantic_analysis(log->right, c_info);

        break;
    }
    case ast::T_CONST: {
        break;
    }
    case ast::T_VAR: {
        std::shared_ptr<ast::Var> t_var = AST_SAFE_CAST(ast::Var, root);

        c_info.err.on_false(c_info.known_vars[t_var->get_var_id()].defined, "Variable '%' is undefined at this time\n", c_info.known_vars[t_var->get_var_id()].name);
        break;
    }
    case ast::T_ACCESS: {
        std::shared_ptr<ast::Access> t_access = AST_SAFE_CAST(ast::Access, root);

        c_info.err.on_false(c_info.known_vars[t_access->get_array_id()].arrayness == VarInfo::Arrayness::Yes, "Variable '%' is not an array\n", c_info.known_vars[t_access->get_array_id()].name);
        c_info.err.on_false(c_info.known_vars[t_access->get_array_id()].defined, "Array '%' is undefined at this time\n", c_info.known_vars[t_access->get_array_id()].name);

        semantic_analysis(t_access->index, c_info);
        break;
    }
    case ast::T_STR: {
        break;
    }
    case ast::T_ARIT: {
        std::shared_ptr<ast::Arit> t_arit = AST_SAFE_CAST(ast::Arit, root);

        semantic_analysis(t_arit->left, c_info);
        semantic_analysis(t_arit->right, c_info);
        break;
    }
    case ast::T_WHILE: {
        std::shared_ptr<ast::While> t_while = AST_SAFE_CAST(ast::While, root);

        semantic_analysis(t_while->condition, c_info);
        semantic_analysis(t_while->body, c_info);
        break;
    }
    case ast::T_LSTR: {
        std::shared_ptr<ast::Lstr> t_lstr = AST_SAFE_CAST(ast::Lstr, root);

        for (const auto& format : t_lstr->format) {
            semantic_analysis(format, c_info);
        }
        break;
    }
    case ast::T_NUM_GENERAL:
    case ast::T_BASE:
    default: {
        c_info.err.error("Invalid tree node\n");
        break;
    }
    }
}

} // namespace semantic
