#include "dictionary.hpp"

#include "ast.hpp"

std::string library_files[LIB_ENUM_END] = {
    "lib/uprint.o",
    "lib/putchar.o",
};

std::map<var_type, std::string> var_type_str_map = {
    std::make_pair(V_INT, "int"),
    std::make_pair(V_STR, "str"),
    std::make_pair(V_UNSURE, "untyped"),
};
