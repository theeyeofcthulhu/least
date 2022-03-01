#ifndef SEMANTICS_H_
#define SEMANTICS_H_

#include <memory>
#include <vector>

#include "ast.hpp"
#include "dictionary.hpp"

class CompileInfo;

namespace semantic {
/*
 * Specifies a correct call to a function
 */
struct FunctionSpec {
    std::string_view name;                           /* The name of the function */
    size_t exp_arg_len;                              /* The number of arguments */
    std::vector<ast::ts_class> types;                /* The types of every argument */
    std::vector<var_type> info;                      /* The types of every T_VAR argument */
    std::vector<std::pair<size_t, var_type>> define; /* What T_VARS are defined by this function */
    FunctionSpec(std::string_view t_name,
        size_t t_len,
        const std::vector<ast::ts_class>& t_types,
        const std::vector<var_type>& t_info,
        const std::vector<std::pair<size_t, var_type>>& t_def)
        : name(t_name)
        , exp_arg_len(t_len)
        , types(t_types)
        , info(t_info)
        , define(t_def)
    {
    }
};

void semantic_analysis(std::shared_ptr<ast::Node> root, CompileInfo& c_info);

/*
 * Check if the supplied args comply with spec
 */
void check_correct_function_call(const FunctionSpec& spec,
    const std::vector<std::shared_ptr<ast::Node>>& args,
    CompileInfo& c_info);
}

#endif // SEMANTICS_H_
