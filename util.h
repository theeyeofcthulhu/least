#ifndef UTIL_H_
#define UTIL_H_

#include <string>
#include <vector>
#include <utility>
#include <array>

#include "dictionary.h"
#include "error.h"

class token;
enum token_type : int;

class compile_info{
public:
    std::vector<std::pair<std::string, bool>> known_vars;
    std::vector<std::string> known_string;

    std::array<bool, LIB_ENUM_END> req_libs = {0};

    std::string filename;

    error_handler err;

    int get_next_body_id(){
        return body_id++;
    }

    compile_info(std::string pfilename) : filename(pfilename), err(pfilename)
    {}
private:
    int body_id = NODE_ARR_SZ;
};

std::vector<std::string> split(std::string str, char delim);
std::string read_source_code(std::string filename, compile_info& c_info);
std::string get_next_word(std::string str, int index, size_t& spc_idx);
size_t next_of_type_on_line(std::vector<token*> ts, size_t start, token_type ty);

template<typename T> std::vector<T> slice(std::vector<T> v, int start=0, int end=-1);

#endif // UTIL_H_
