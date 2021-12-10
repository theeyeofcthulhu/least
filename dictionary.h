#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#define NODE_ARR_SZ 1024

typedef enum {
PRINT,
UPRINT,
FPRINT,
EXIT,
READ,
SET,
PUTCHAR,
INT,
}efunc;

typedef enum {
IF,
ELIF,
ELSE,
NO_CONDITIONAL,
}econditional;

/* Structure for organizing comparison operations */
typedef enum {
EQUAL,
NOT_EQUAL,
LESS,
LESS_OR_EQ,
GREATER,
GREATER_OR_EQ,
CMP_OPERATION_ENUM_END,
}ecmp_operation;

/* Structure for organizing arithmetic operations */
typedef enum {
ADD,
SUB,
MOD,
DIV,
MUL,
ARIT_OPERATION_ENUM_END,
}earit_operation;

typedef enum {
AND,
OR,
LOGICAL_OPS_END,
}elogical_operation;

typedef enum {
K_PRINT,
K_UPRINT,
K_FPRINT,
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
K_PUTCHAR,
K_NOKEY,
}ekeyword;

enum LIBRARY_FILES{
LIB_UPRINT,
LIB_PUTCHAR,
LIB_ENUM_END,
};

extern char* library_files[LIB_ENUM_END];

#endif // DICTIONARY_H_
