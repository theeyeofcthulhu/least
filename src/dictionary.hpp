#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include <map>
#include <string>

enum func_id {
    F_PRINT,
    F_EXIT,
    F_READ,
    F_SET,
    F_SETD,
    F_ADD,
    F_SUB,
    F_BREAK,
    F_CONT,
    F_PUTCHAR,
    F_INT,
    F_DOUBLE,
    F_ARRAY,
    F_STR,
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
    K_DOUBLE,
    K_STR,
    K_READ,
    K_SET,
    K_SETD,
    K_ADD,
    K_SUB,
    K_TIME,
    K_GETUID,
    K_BREAK,
    K_CONT,
    K_ARRAY,
    K_PUTCHAR,
    K_NOKEY,
};

enum var_type {
    V_INT,
    V_DOUBLE,
    V_INT_OR_DOUBLE,
    V_STR,
    V_ARR,
    V_UNSURE,
};

#endif // DICTIONARY_H_
