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
    F_SUB,
    F_BREAK,
    F_CONT,
    F_PUTCHAR,
    F_INT,
    F_STR,
    F_CALL,
};

enum value_func_id {
    VF_TIME,
    VF_GETUID,
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
    K_SUB,
    K_TIME,
    K_GETUID,
    K_BREAK,
    K_CONT,
    K_PUTCHAR,
    K_NOKEY,
};

enum var_type {
    V_INT,
    V_STR,
    V_UNSURE,
};

extern const std::map<var_type, std::string> var_type_str_map;
extern const std::map<func_id, std::string> func_str_map;
extern const std::map<value_func_id, std::string> vfunc_str_map;
extern const std::map<keyword, value_func_id> key_vfunc_map;

#endif  // DICTIONARY_H_
