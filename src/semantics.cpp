#include <cassert>
#include <memory>

#include "ast.hpp"
#include "error.hpp"
#include "maps.hpp"
#include "semantics.hpp"
#include "util.hpp"

namespace semantic {

static inline bool is_single_number(std::shared_ptr<ast::Node> nd);
static inline bool is_int(std::shared_ptr<ast::Node> nd);
static inline bool is_double(std::shared_ptr<ast::Node> nd);
/* Check if the supplied args comply with spec */
void check_correct_function_call(const FunctionSpec& spec,
    const std::vector<std::shared_ptr<ast::Node>>& args,
    CompileInfo& c_info);
var_type check_arit_types(std::shared_ptr<ast::Arit> arit, CompileInfo &c_info, var_type type = V_UNSURE);

/* How each function needs to be called */
const std::map<func_id, FunctionSpec> func_spec_map = {
    std::make_pair<func_id, FunctionSpec>(F_PRINT, { "print", 1, { ast::T_LSTR }, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_EXIT, { "exit", 1, { ast::T_INT_GENERAL }, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_READ, { "read", 1, { ast::T_VAR }, { V_STR }, {} }),
    // TODO overloaded functions like set
    std::make_pair<func_id, FunctionSpec>(F_SET, { "set", 2, { ast::T_IN_MEMORY, ast::T_INT_GENERAL }, { V_INT }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_SETD, { "setd", 2, { ast::T_IN_MEMORY, ast::T_DOUBLE_GENERAL }, { V_DOUBLE }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_ADD, { "add", 2, { ast::T_IN_MEMORY, ast::T_INT_GENERAL }, { V_INT }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_SUB, { "sub", 2, { ast::T_IN_MEMORY, ast::T_INT_GENERAL }, { V_INT }, {} }),
    std::make_pair<func_id, FunctionSpec>(F_BREAK, { "break", 0, {}, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_CONT, { "continue", 0, {}, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_PUTCHAR, { "putchar", 1, { ast::T_INT_GENERAL }, {}, {} }),
    std::make_pair<func_id, FunctionSpec>(F_DOUBLE, { "double", 2, { ast::T_VAR, ast::T_DOUBLE_GENERAL }, { V_DOUBLE }, { { 0, V_DOUBLE } } }),
    std::make_pair<func_id, FunctionSpec>(F_ARRAY, { "array", 2, { ast::T_VAR, ast::T_CONST }, { V_ARR }, { { 0, V_ARR } } }),
    std::make_pair<func_id, FunctionSpec>(F_STR, { "str", 1, { ast::T_VAR }, { V_STR }, { { 0, V_STR } } }),
};

const std::map<func_id, std::vector<FunctionSpec>> overloaded_func_spec_map = {
    std::make_pair<func_id, std::vector<FunctionSpec>>(F_INT, { 
                { "int", 2, { ast::T_VAR, ast::T_INT_GENERAL }, { V_INT }, { { 0, V_INT } } },
                { "int", 1, { ast::T_VAR }, { V_INT }, { { 0, V_INT } } },
            }),
};

static inline bool is_single_number(std::shared_ptr<ast::Node> nd)
{
    return nd->get_type() == ast::T_CONST || nd->get_type() == ast::T_DOUBLE_CONST || nd->get_type() == ast::T_VAR
                             || nd->get_type() == ast::T_ACCESS || nd->get_type() == ast::T_VFUNC;
}

static inline bool is_int(std::shared_ptr<ast::Node> nd)
{
    return nd->get_type() == ast::T_CONST || nd->get_type() == ast::T_VAR || nd->get_type() == ast::T_ACCESS || nd->get_type() == ast::T_VFUNC || nd->get_type() == ast::T_ARIT;
}

// TODO: array doubles
static inline bool is_double(std::shared_ptr<ast::Node> nd)
{
    return nd->get_type() == ast::T_DOUBLE_CONST || nd->get_type() == ast::T_VAR || nd->get_type() == ast::T_VFUNC || nd->get_type() == ast::T_ARIT;
}

size_t get_correct_overload(const std::vector<FunctionSpec>& specs,
        const std::vector<std::shared_ptr<ast::Node>>& args)
{
    for (size_t s = 0; s < specs.size(); s++) {
        const auto& spec = specs[s];
        
        if (spec.exp_arg_len != args.size())
            continue;

        bool found = true;
        for (size_t i = 0; i < args.size(); i++) {
            const auto& arg = args[i];
            auto type = spec.types[i];

            // TODO: check vars?
            switch(type) {
            case ast::T_INT_GENERAL: {
                if (!is_int(arg)) {
                    found = false;
                }
                break;
            }
            case ast::T_DOUBLE_GENERAL: {
                if (!is_double(arg)) {
                    found = false;
                }
                break;
            }
            case ast::T_IN_MEMORY: {
                if (!(arg->get_type() == ast::T_VAR || arg->get_type() == ast::T_ACCESS)) {
                    found = false;
                }
                break;
            }
            default: {
                if (arg->get_type() != spec.types[i]) {
                    found = false;
                }
                break;
            }
            }

            if (!found)
                break;
        }

        if (found) {
            return s;
        }
    }

    // check_correct_function_call will throw errors
    // TODO: better errors here?
    return 0;
}

/* Give information about how a correct function call looks like and check for
 * it */
void check_correct_function_call(const FunctionSpec& spec,
    const std::vector<std::shared_ptr<ast::Node>>& args,
    CompileInfo& c_info)
{
    c_info.err.on_false(args.size() == spec.exp_arg_len,
        "Expected {} arguments to function '{}', got {}", spec.exp_arg_len,
        spec.name, args.size());

    /* Caller can provide arguments which will be defined by this function
     * We define them so we don't error out later */
    if (!spec.define.empty()) {
        for (const auto& d : spec.define) {
            assert(d.first < spec.exp_arg_len);

            c_info.err.on_false(args[d.first]->get_type() == ast::T_VAR,
                "Argument {} to '{}' expected to be variable", d.first, spec.name);
            auto t_var = AST_SAFE_CAST(ast::Var, args[d.first]);

            c_info.err.on_true(c_info.known_vars[t_var->get_var_id()].defined,
                "Argument {} to '{}' expected to be undefined", d.first, spec.name);
            c_info.known_vars[t_var->get_var_id()].defined = true;
            c_info.known_vars[t_var->get_var_id()].type = d.second;

            if (d.second == V_ARR) {
                c_info.known_vars[t_var->get_var_id()].arrayness = VarInfo::Arrayness::Yes;
            }
        }
    }

    auto info_it = spec.info.begin();

    assert(spec.types.size() == spec.exp_arg_len);

    for (size_t i = 0; i < args.size(); i++) {
        const auto& arg = args[i];

        if (spec.types[i] == ast::T_INT_GENERAL) {
            c_info.err.on_false(is_int(arg),
                "Argument {} to '{}' has to evaluate to an integer", i, spec.name);

            /* If we are var: check that we are int */
            if (arg->get_type() == ast::T_VAR) {
                auto t_var = AST_SAFE_CAST(ast::Var, args[i]);

                c_info.error_on_undefined(t_var);
                c_info.error_on_wrong_type(t_var, V_INT);
            } else if (arg->get_type() == ast::T_VFUNC) {
                auto vfunc = AST_SAFE_CAST(ast::VFunc, args[i]);
                c_info.err.on_false(vfunc->get_return_type() == V_INT,
                    "Argument {} to '{}' has to evaluate to an integer"
                    "Got '{}' returning '{}'",
                    i, spec.name, vfunc_str_map.at(vfunc->get_value_func()),
                    var_type_str_map.at(vfunc->get_return_type()));
            } else if (arg->get_type() == ast::T_ARIT) {
                auto arit = AST_SAFE_CAST(ast::Arit, args[i]);
                c_info.err.on_false(check_arit_types(arit, c_info) == V_INT,
                                    "Argument {} to '{}' has to evaluate to an integer", i, spec.name);
            }
        } else if (spec.types[i] == ast::T_DOUBLE_GENERAL) {
            c_info.err.on_false(is_double(arg),
                "Argument {} to '{}' has to evaluate to a double", i, spec.name);

            /* If we are var: check that we are double */
            if (arg->get_type() == ast::T_VAR) {
                auto t_var = AST_SAFE_CAST(ast::Var, args[i]);

                c_info.error_on_undefined(t_var);
                c_info.error_on_wrong_type(t_var, V_DOUBLE);
            } else if (arg->get_type() == ast::T_VFUNC) {
                assert(false && "double vfuncs not implemented yet");
                // auto vfunc = AST_SAFE_CAST(ast::VFunc, args[i]);
                // c_info.err.on_false(vfunc->get_return_type() == V_DOUBLE,
                //     "Argument {} to '{}' has to evaluate to a double"
                //     "Got '{}' returning '{}'",
                //     i, spec.name, vfunc_str_map.at(vfunc->get_value_func()),
                //     var_type_str_map.at(vfunc->get_return_type()));
            } else if (arg->get_type() == ast::T_ARIT) {
                auto arit = AST_SAFE_CAST(ast::Arit, args[i]);
                c_info.err.on_false(check_arit_types(arit, c_info) == V_DOUBLE,
                                    "Argument {} to '{}' has to evaluate to a double", i, spec.name);
            }
        } else if (spec.types[i] == ast::T_IN_MEMORY) {
            c_info.err.on_false(arg->get_type() == ast::T_VAR || arg->get_type() == ast::T_ACCESS, "Argument {} to '{}' has to have a memory address", i, spec.name);

            if (arg->get_type() == ast::T_VAR) {
                auto t_var = AST_SAFE_CAST(ast::Var, args[i]);
                c_info.error_on_undefined(t_var);
                // TODO: do we need this error check?
                /* c_info.err.on_false(
                    v_info.type == V_INT, "Argument {} to '{}' has to have type '{}' but has '{}'", i,
                    spec.name, var_type_str_map.at(V_INT), var_type_str_map.at(v_info.type)); */
            } else if (arg->get_type() == ast::T_ACCESS) {
                auto access = AST_SAFE_CAST(ast::Access, args[i]);
                auto v_info = c_info.known_vars[access->get_array_id()];

                c_info.err.on_false(v_info.defined, "Var '{}' is undefined at this time",
                    v_info.name);
                c_info.err.on_false(
                    v_info.type == V_ARR, "Argument {} to '{}' has to have type '{}' but has '{}'", i,
                    spec.name, var_type_str_map.at(V_ARR), var_type_str_map.at(v_info.type));
            }
        } else {
            c_info.err.on_false(arg->get_type() == spec.types[i],
                "Argument {} to function '{}' is wrong type", i, spec.name);
        }

        /* If we are a var: check the provided 'info' type information if the
         * type is correct */
        if (arg->get_type() == ast::T_VAR && spec.types[i] != ast::T_INT_GENERAL && spec.types[i] != ast::T_DOUBLE_GENERAL) {
            auto t_var = AST_SAFE_CAST(ast::Var, args[i]);

            c_info.err.on_true(info_it == spec.info.end() || spec.info.empty(),
                "Could not parse arguments to function '{}'", spec.name);

            c_info.error_on_undefined(t_var);
            c_info.error_on_wrong_type(t_var, *info_it.base());

            info_it++;
        }
    }
}

/* Check usage of undefined variables and incorrect function calls, etc. */
void semantic_analysis(std::shared_ptr<ast::Node> root, CompileInfo& c_info)
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
        semantic_analysis(t_if->body, c_info);

        if (t_if->elif)
            semantic_analysis(t_if->elif, c_info);
        break;
    }
    case ast::T_FUNC: {
        std::shared_ptr<ast::Func> t_func = AST_SAFE_CAST(ast::Func, root);

        if (func_spec_map.contains(t_func->get_func())) {
            check_correct_function_call(func_spec_map.at(t_func->get_func()), t_func->args, c_info);
        } else {
            const auto& specs = overloaded_func_spec_map.at(t_func->get_func());
            size_t spec_index = get_correct_overload(specs, t_func->args);
            t_func->overload_id = spec_index;
            check_correct_function_call(specs[spec_index], t_func->args, c_info);
        }

        for (const auto& arg : t_func->args) {
            semantic_analysis(arg, c_info);
        }

        switch (t_func->get_func()) {
        case F_INT: {
            auto t_var = AST_SAFE_CAST(ast::Var, t_func->args[0]);
            c_info.known_vars[t_var->get_var_id()].stack_offset = c_info.get_stack_size_and_append(1);
            break;
        }
        case F_DOUBLE: {
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

        // TODO: type error checking

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
    case ast::T_DOUBLE_CONST:
    case ast::T_CONST: {
        break;
    }
    case ast::T_VAR: {
        std::shared_ptr<ast::Var> t_var = AST_SAFE_CAST(ast::Var, root);

        c_info.err.on_false(c_info.known_vars[t_var->get_var_id()].defined, "Variable '{}' is undefined at this time", c_info.known_vars[t_var->get_var_id()].name);
        break;
    }
    case ast::T_ACCESS: {
        std::shared_ptr<ast::Access> t_access = AST_SAFE_CAST(ast::Access, root);

        c_info.err.on_false(c_info.known_vars[t_access->get_array_id()].arrayness == VarInfo::Arrayness::Yes, "Variable '{}' is not an array", c_info.known_vars[t_access->get_array_id()].name);
        c_info.err.on_false(c_info.known_vars[t_access->get_array_id()].defined, "Array '{}' is undefined at this time", c_info.known_vars[t_access->get_array_id()].name);

        semantic_analysis(t_access->index, c_info);
        break;
    }
    case ast::T_STR: {
        break;
    }
    case ast::T_ARIT: {
        std::shared_ptr<ast::Arit> t_arit = AST_SAFE_CAST(ast::Arit, root);

        // TODO: do not check arits twice
        // i.e.: put checked arits into an array
        check_arit_types(t_arit, c_info);

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
    case ast::T_INT_GENERAL:
    case ast::T_DOUBLE_GENERAL:
    case ast::T_BASE:
    default: {
        UNREACHABLE();
        break;
    }
    }
}

var_type get_number_type(std::shared_ptr<ast::Node> nd, CompileInfo &c_info)
{
    if (nd->get_type() == ast::T_CONST) {
        return V_INT;
    } else if (nd->get_type() == ast::T_DOUBLE_CONST) {
        return V_DOUBLE;
    } else if (nd->get_type() == ast::T_VAR) {
        const VarInfo &var = c_info.known_vars[AST_SAFE_CAST(ast::Var, nd)->get_var_id()];

        c_info.err.on_false(var.defined, "Variable '{}' is undefined at this time", var.name);
        c_info.err.on_false(var.type == V_INT || var.type == V_DOUBLE, "Expected int or double");

        return var.type;
    } else if (nd->get_type() == ast::T_ACCESS) {
        // TODO: implement double arrays here
        return V_INT;
    } else if (nd->get_type() == ast::T_VFUNC) {
        auto vfunc = AST_SAFE_CAST(ast::VFunc, nd);

        return vfunc_var_type_map.at(vfunc->get_value_func());
    } else if (nd->get_type() == ast::T_ARIT) {
        auto arit = AST_SAFE_CAST(ast::Arit, nd);

        return check_arit_types(arit, c_info);
    }

    UNREACHABLE();
    return V_UNSURE;
}

/* Checks if all types are equal and returns the type */

// TODO: only have this function called once for every expression
var_type check_arit_types(std::shared_ptr<ast::Arit> arit, CompileInfo &c_info, var_type type)
{
    if (type == V_UNSURE) {
        if (is_single_number(arit->left))
            type = get_number_type(arit->left, c_info);
        else if (is_single_number(arit->right))
            type = get_number_type(arit->right, c_info);
    }

    if (type != V_UNSURE) {
        if (is_single_number(arit->left)) {
            var_type ltype = get_number_type(arit->left, c_info);
            c_info.err.on_true(ltype != type, "Type mismatch: '{}' and '{}'", var_type_str_map.at(ltype), var_type_str_map.at(type));
        } else {
            check_arit_types(AST_SAFE_CAST(ast::Arit, arit->left), c_info, type);
        }
        if (is_single_number(arit->right)) {
            var_type rtype = get_number_type(arit->right, c_info);
            c_info.err.on_true(rtype != type, "Type mismatch: '{}' and '{}'", var_type_str_map.at(type), var_type_str_map.at(rtype));
        } else {
            check_arit_types(AST_SAFE_CAST(ast::Arit, arit->right), c_info, type);
        }
    } else {
        // Try to get the type from recursive call
        type = check_arit_types(AST_SAFE_CAST(ast::Arit, arit->left), c_info, type);
        if (type == V_UNSURE)
            type = check_arit_types(AST_SAFE_CAST(ast::Arit, arit->right), c_info, type);
        else
            check_arit_types(AST_SAFE_CAST(ast::Arit, arit->right), c_info, type);
    }

    return type;
}

} // namespace semantic
