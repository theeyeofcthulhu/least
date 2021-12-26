#include "dictionary.hpp"

const std::string library_files[LIB_ENUM_END] = {
    "lib/uprint.o",
    "lib/putchar.o",
};

const std::map<var_type, std::string> var_type_str_map = {
    std::make_pair(V_INT, "int"),
    std::make_pair(V_STR, "str"),
    std::make_pair(V_UNSURE, "untyped"),
};

const std::map<func_id, std::string> func_str_map = {
    std::make_pair(F_PRINT, "print"),     std::make_pair(F_EXIT, "exit"),
    std::make_pair(F_READ, "read"),       std::make_pair(F_SET, "set"),
    std::make_pair(F_PUTCHAR, "putchar"), std::make_pair(F_INT, "int"),
    std::make_pair(F_STR, "str"),         std::make_pair(F_ADD, "add"),
    std::make_pair(F_SUB, "sub"),
};
