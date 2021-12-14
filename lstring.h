#ifndef LSTRING_H_
#define LSTRING_H_

#include <string>
#include <map>

#include "dictionary.h"

class token;

#define NODE_ARR_SZ 1024

#include "lexer.h"

class lstring{
public:
    std::vector<token*> ts;
};

lstring* parse_string(std::string string, int line, compile_info& c_info);

#endif // LSTRING_H_
