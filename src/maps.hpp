#ifndef MAPS_H_
#define MAPS_H_

#include <map>
#include <string_view>

#include "dictionary.hpp"

namespace lexer {
enum token_type : int;

extern const std::map<const size_t, token_type> token_type_enum_map;
extern const std::map<token_type, std::string_view> token_str_map;
}

namespace ast {
enum ts_class : int;

extern const std::map<const size_t, ts_class> tree_type_enum_map;
}

extern const std::map<std::string_view, keyword> str_key_map;
extern const std::map<keyword, std::string_view> key_str_map;

extern const std::map<std::string_view, cmp_op> cmp_map;
extern const std::map<cmp_op, std::string_view> cmp_str_map;

extern const std::map<std::string_view, arit_op> arit_map;
extern const std::map<arit_op, std::string_view> arit_str_map;

extern const std::map<std::string_view, log_op> log_map;
extern const std::map<log_op, std::string_view> log_str_map;

extern const std::map<func_id, std::string_view> func_str_map;
extern const std::map<keyword, func_id> key_func_map;

extern const std::map<var_type, std::string_view> var_type_str_map;

extern const std::map<value_func_id, std::string_view> vfunc_str_map;
extern const std::map<keyword, value_func_id> key_vfunc_map;
extern const std::map<value_func_id, var_type> vfunc_var_type_map;

extern const std::map<char, std::string_view> str_tokens;
extern const std::map<char, char> str_tokens_char;

#endif // MAPS_H_
