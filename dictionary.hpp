#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <map>
#include <string>

enum func_id {
    F_PRINT,
    F_EXIT,
    F_READ,
    F_SET,
    F_ADD,
    F_PUTCHAR,
    F_INT,
    F_STR,
};

enum conditional {
    IF,
    ELIF,
    ELSE,
    NO_CONDITIONAL,
};

/* Structure for organizing comparison operations */
enum cmp_op {
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_OR_EQ,
    GREATER,
    GREATER_OR_EQ,
    CMP_OPERATION_ENUM_END,
};

/* Structure for organizing arithmetic operations */
enum arit_op {
    ADD,
    SUB,
    MOD,
    DIV,
    MUL,
    ARIT_OPERATION_ENUM_END,
};

enum log_op {
    AND,
    OR,
    LOGICAL_OPS_END,
};

enum keyword {
    K_PRINT,
    K_EXIT,
    K_IF,
    K_ELIF,
    K_ELSE,
    K_WHILE,
    K_END,
    K_INT,
    K_STR,
    K_READ,
    K_SET,
    K_ADD,
    K_PUTCHAR,
    K_NOKEY,
};

enum LIBRARY_FILES {
    LIB_UPRINT,
    LIB_PUTCHAR,
    LIB_ENUM_END,
};

enum var_type {
    V_INT,
    V_STR,
    V_UNSURE,
};

extern std::string library_files[LIB_ENUM_END];
extern std::map<var_type, std::string> var_type_str_map;

#endif // DICTIONARY_H_
