#include <iostream>
#include <cassert>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cstdio>
#include <unistd.h>

#include "util.h"
#include "error.h"
#include "lexer.h"

// /* Unite an array of strings into a single string with the elements separated by spaces */
// char* unite(char** src, int off, int len){
//     char* out;
//     int out_len = 0;

//     for(int i = off; i < len + off; i++)
//         out_len += strlen(src[i]);

//     /* Account for spaces */
//     out_len += len - 1;

//     out = malloc((out_len + 1) * sizeof(char));
//     memset(out, '\0', out_len + 1);

//     for(int i = off; i < len + off - 1; i++){
//         strcat(out, src[i]);
//         strcat(out, " ");
//     }
//     strcat(out, src[len + off - 1]);

//     return out;
// }

// /* Separate src by spaces (gobble up duplicate spaces) into array of strings.
//  * Store length of array in dest_len. */
// char** sepbyspc(char* src, int* dest_len){
//     if(!src)
//         return NULL;

//     int src_len = strlen(src);

//     if(src_len == 0)
//         return NULL;

//     char* src_cpy = malloc((src_len + 1) * sizeof(char));
//     strcpy(src_cpy, src);

//     char** dest;
//     *dest_len = 0;

//     for(int i = 0, repeat = 0; i < src_len; i++){
//         if(src_cpy[i] == ' ' && !repeat){
//             *dest_len += 1;
//             repeat = 1;
//         }else if(!(src_cpy[i] == ' ')){
//             repeat = 0;
//         }
//     }
//     *dest_len += 1;

//     dest = malloc((*dest_len + 1) * sizeof(char*));

//     char* src_word;
//     char* src_copy_ptr = src_cpy;
//     /* control_copy_ptr becomes NULL after first iteration since every call to strtok after the first one
//      * which is supposed to operate on the same string has to be with NULL as str. */
//     for(int i = 0; i < *dest_len; src_copy_ptr = NULL, i++){
//         src_word = strtok(src_copy_ptr, " ");
//         if(!src_word)
//             break;
//         int src_word_len = strlen(src_word);

//         dest[i] = malloc((src_word_len + 1) * sizeof(char));
//         strcpy(dest[i], src_word);
//     }

//     free(src_cpy);

//     return dest;
// }

/* Get the next word starting from index and store the next space
 * in spc_idx */
std::string get_next_word(std::string str, int index, size_t& spc_idx){
    std::string substr = str.substr(index, std::string::npos);

    spc_idx = substr.find(' ');

    if(spc_idx == std::string::npos)
        spc_idx = substr.size();

    std::string res = substr.substr(0, spc_idx);
    return res;
}

std::vector<std::string> split(std::string str, char delim){
    std::vector<std::string> res;

    std::stringstream lines(str);

    std::string ln;

    while(std::getline(lines, ln, delim)){
        res.push_back(ln);
    }

    return res;
}

/* Memory map a file and return the contents */
std::string read_source_code(std::string filename, compile_info& c_info){
    int input_file = open(filename.c_str(), O_RDONLY);
    c_info.err.on_true(input_file < 0, "Could not open file '%s'\n", filename.c_str());

    struct stat file_stat;
    c_info.err.on_true((fstat(input_file, &file_stat)) < 0, "Could not stat file '%s'\n", filename.c_str());

    int input_size = file_stat.st_size;

    char* c_res = (char*)mmap(NULL, input_size, PROT_READ, MAP_PRIVATE, input_file, 0);
    c_info.err.on_true(c_res == MAP_FAILED, "Failed to map file '%s'\n", filename.c_str());

    std::string result = c_res;

    close(input_file);

    return result;
}

/* Get the index of the next token of type 'ty' on line.
 * Return ts.size() on failure. */
size_t next_of_type_on_line(std::vector<token*> ts, size_t start, token_type ty){
    for(size_t i = start; i < ts.size(); i++){
        if(ts[i]->get_type() == TK_EOL){
            return ts.size();
        }else if(ts[i]->get_type() == ty){
            return i;
        }
    }

    return ts.size();
}
