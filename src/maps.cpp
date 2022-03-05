#include <cassert>

#include "maps.hpp"
#include "lexer.hpp"
#include "ast.hpp"

void assert_map_sizes()
{
    const int n_tokens = 12;

    assert(lexer::token_type_enum_map.size() == n_tokens);
    assert(lexer::token_str_map.size() == n_tokens);

    const int n_nodes = 15;

    assert(ast::tree_type_enum_map.size() == n_nodes);

    const int n_keys = 19;

    assert(str_key_map.size() == n_keys);
    assert(key_str_map.size() == n_keys);

    const int n_comps = 6;

    assert(cmp_map.size() == n_comps);
    assert(cmp_str_map.size() == n_comps);

    const int n_logs = 2;

    assert(log_map.size() == n_logs);
    assert(log_str_map.size() == n_logs);

    const int n_funcs = 12;

    assert(func_str_map.size() == n_funcs);
    assert(key_func_map.size() == n_funcs);

    const int n_types = 4;

    assert(var_type_str_map.size() == n_types);

    const int n_vfuncs = 2;

    assert(vfunc_str_map.size() == n_vfuncs);
    assert(key_vfunc_map.size() == n_vfuncs);
    assert(vfunc_var_type_map.size() == n_vfuncs);

    const int n_str_tokens = 7;

    assert(str_tokens.size() == n_str_tokens);
    assert(str_tokens_char.size() == n_str_tokens);
}

namespace lexer {

const std::map<const size_t, token_type> token_type_enum_map = {
    std::make_pair(typeid(Key).hash_code(), TK_KEY),
    std::make_pair(typeid(Arit).hash_code(), TK_ARIT),
    std::make_pair(typeid(Cmp).hash_code(), TK_CMP),
    std::make_pair(typeid(Log).hash_code(), TK_LOG),
    std::make_pair(typeid(Str).hash_code(), TK_STR),
    std::make_pair(typeid(Lstr).hash_code(), TK_LSTR),
    std::make_pair(typeid(Num).hash_code(), TK_NUM),
    std::make_pair(typeid(Var).hash_code(), TK_VAR),
    std::make_pair(typeid(Access).hash_code(), TK_ACCESS),
    std::make_pair(typeid(Call).hash_code(), TK_CALL),
    std::make_pair(typeid(Sep).hash_code(), TK_SEP),
    std::make_pair(typeid(Eol).hash_code(), TK_EOL),
};

const std::map<token_type, std::string_view> token_str_map {
    std::make_pair(TK_KEY, "key"),
    std::make_pair(TK_ARIT, "arit"),
    std::make_pair(TK_CMP, "cmp"),
    std::make_pair(TK_LOG, "log"),
    std::make_pair(TK_STR, "str"),
    std::make_pair(TK_LSTR, "lstr"),
    std::make_pair(TK_NUM, "num"),
    std::make_pair(TK_VAR, "var"),
    std::make_pair(TK_ACCESS, "access"),
    std::make_pair(TK_SEP, "sep"),
    std::make_pair(TK_CALL, "call"),
    std::make_pair(TK_EOL, "eol"),
};

} // namespace lexer

namespace ast {

const std::map<const size_t, ts_class> tree_type_enum_map = {
    std::make_pair(typeid(Node).hash_code(), T_BASE),
    std::make_pair(typeid(If).hash_code(), T_IF),
    std::make_pair(typeid(Else).hash_code(), T_ELSE),
    std::make_pair(typeid(While).hash_code(), T_WHILE),
    std::make_pair(typeid(Const).hash_code(), T_CONST),
    std::make_pair(typeid(Cmp).hash_code(), T_CMP),
    std::make_pair(typeid(Log).hash_code(), T_LOG),
    std::make_pair(typeid(Func).hash_code(), T_FUNC),
    std::make_pair(typeid(VFunc).hash_code(), T_VFUNC),
    std::make_pair(typeid(Var).hash_code(), T_VAR),
    std::make_pair(typeid(Access).hash_code(), T_ACCESS),
    std::make_pair(typeid(Body).hash_code(), T_BODY),
    std::make_pair(typeid(Str).hash_code(), T_STR),
    std::make_pair(typeid(Lstr).hash_code(), T_LSTR),
    std::make_pair(typeid(Arit).hash_code(), T_ARIT),
};

} // namespace ast

const std::map<std::string_view, keyword> str_key_map {
    std::make_pair("print", K_PRINT),
    std::make_pair("exit", K_EXIT),
    std::make_pair("if", K_IF),
    std::make_pair("elif", K_ELIF),
    std::make_pair("else", K_ELSE),
    std::make_pair("while", K_WHILE),
    std::make_pair("end", K_END),
    std::make_pair("int", K_INT),
    std::make_pair("str", K_STR),
    std::make_pair("read", K_READ),
    std::make_pair("set", K_SET),
    std::make_pair("putchar", K_PUTCHAR),
    std::make_pair("add", K_ADD),
    std::make_pair("sub", K_SUB),
    std::make_pair("break", K_BREAK),
    std::make_pair("continue", K_CONT),
    std::make_pair("time", K_TIME),
    std::make_pair("getuid", K_GETUID),
    std::make_pair("array", K_ARRAY),
};

const std::map<keyword, std::string_view> key_str_map {
    std::make_pair(K_PRINT, "print"),
    std::make_pair(K_EXIT, "exit"),
    std::make_pair(K_IF, "if"),
    std::make_pair(K_ELIF, "elif"),
    std::make_pair(K_ELSE, "else"),
    std::make_pair(K_WHILE, "while"),
    std::make_pair(K_END, "end"),
    std::make_pair(K_INT, "int"),
    std::make_pair(K_STR, "str"),
    std::make_pair(K_READ, "read"),
    std::make_pair(K_SET, "set"),
    std::make_pair(K_PUTCHAR, "putchar"),
    std::make_pair(K_ADD, "add"),
    std::make_pair(K_SUB, "sub"),
    std::make_pair(K_BREAK, "break"),
    std::make_pair(K_CONT, "continue"),
    std::make_pair(K_TIME, "time"),
    std::make_pair(K_GETUID, "getuid"),
    std::make_pair(K_ARRAY, "array"),
};

const std::map<std::string_view, cmp_op> cmp_map {
    std::make_pair("==", EQUAL),
    std::make_pair("!=", NOT_EQUAL),
    std::make_pair("<", LESS),
    std::make_pair("<=", LESS_OR_EQ),
    std::make_pair(">", GREATER),
    std::make_pair(">=", GREATER_OR_EQ),
};

const std::map<cmp_op, std::string_view> cmp_str_map {
    std::make_pair(EQUAL, "=="),
    std::make_pair(GREATER, ">"),
    std::make_pair(GREATER_OR_EQ, ">="),
    std::make_pair(LESS, "<"),
    std::make_pair(LESS_OR_EQ, "<="),
    std::make_pair(NOT_EQUAL, "!="),
};

const std::map<std::string_view, arit_op> arit_map {
    std::make_pair("+", ADD),
    std::make_pair("-", SUB),
    std::make_pair("%", MOD),
    std::make_pair("/", DIV),
    std::make_pair("*", MUL),
};

const std::map<arit_op, std::string_view> arit_str_map {
    std::make_pair(ADD, "+"),
    std::make_pair(DIV, "/"),
    std::make_pair(MOD, "%"),
    std::make_pair(MUL, "*"),
    std::make_pair(SUB, "-"),
};

const std::map<std::string_view, log_op> log_map {
    std::make_pair("&&", AND),
    std::make_pair("||", OR),
};

const std::map<log_op, std::string_view> log_str_map {
    std::make_pair(AND, "&&"),
    std::make_pair(OR, "||"),
};

const std::map<func_id, std::string_view> func_str_map {
    std::make_pair(F_PRINT, "print"),
    std::make_pair(F_EXIT, "exit"),
    std::make_pair(F_READ, "read"),
    std::make_pair(F_SET, "set"),
    std::make_pair(F_PUTCHAR, "putchar"),
    std::make_pair(F_INT, "int"),
    std::make_pair(F_STR, "str"),
    std::make_pair(F_ADD, "add"),
    std::make_pair(F_SUB, "sub"),
    std::make_pair(F_BREAK, "break"),
    std::make_pair(F_CONT, "continue"),
    std::make_pair(F_ARRAY, "array"),
};

const std::map<keyword, func_id> key_func_map {
    std::make_pair(K_PRINT, F_PRINT),
    std::make_pair(K_EXIT, F_EXIT),
    std::make_pair(K_READ, F_READ),
    std::make_pair(K_SET, F_SET),
    std::make_pair(K_PUTCHAR, F_PUTCHAR),
    std::make_pair(K_INT, F_INT),
    std::make_pair(K_ARRAY, F_ARRAY),
    std::make_pair(K_STR, F_STR),
    std::make_pair(K_ADD, F_ADD),
    std::make_pair(K_SUB, F_SUB),
    std::make_pair(K_BREAK, F_BREAK),
    std::make_pair(K_CONT, F_CONT),
};

const std::map<var_type, std::string_view> var_type_str_map {
    std::make_pair(V_INT, "int"),
    std::make_pair(V_STR, "str"),
    std::make_pair(V_ARR, "array"),
    std::make_pair(V_UNSURE, "untyped"),
};

const std::map<value_func_id, std::string_view> vfunc_str_map {
    std::make_pair(VF_TIME, "time"),
    std::make_pair(VF_GETUID, "getuid"),
};

const std::map<keyword, value_func_id> key_vfunc_map {
    std::make_pair(K_TIME, VF_TIME),
    std::make_pair(K_GETUID, VF_GETUID),
};

const std::map<value_func_id, var_type> vfunc_var_type_map {
    std::make_pair(VF_TIME, V_INT),
    std::make_pair(VF_GETUID, V_INT),
};

const std::map<char, std::string_view> str_tokens {
    std::make_pair('n', "\",0xa,\""),   /* Newline */
    std::make_pair('t', "\",0x9,\""),   /* Tabstop */
    std::make_pair('\\', "\\"),         /* The character '\' */
    std::make_pair('\"', "\",0x22,\""), /* The character '"' */
    std::make_pair('\'', "\",0x27,\""), /* The character "'" */
    std::make_pair('[', "\",0x5B,\""),  /* The character '[' */
    std::make_pair(']', "\",0x5D,\""),  /* The character ']' */
};

const std::map<char, char> str_tokens_char {
    std::make_pair('n', '\n'),  /* Newline */
    std::make_pair('t', '\t'),  /* Tabstop */
    std::make_pair('\\', '\\'), /* The character '\' */
    std::make_pair('\"', '\"'), /* The character '"' */
    std::make_pair('\'', '\''), /* The character ''' */
    std::make_pair('[', '['),   /* The character '[' */
    std::make_pair(']', ']'),   /* The character ']' */
};
