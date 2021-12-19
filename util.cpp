#include <cassert>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.h"
#include "lexer.h"
#include "util.h"

/* Get the next word starting from index and store the next space
 * in spc_idx */
std::string get_next_word(std::string str, int index, size_t &spc_idx) {
    std::string substr = str.substr(index, std::string::npos);

    spc_idx = substr.find(' ');

    if (spc_idx == std::string::npos)
        spc_idx = substr.size();

    std::string res = substr.substr(0, spc_idx);
    return res;
}

std::vector<std::string> split(std::string str, char delim) {
    std::vector<std::string> res;

    std::stringstream lines(str);

    std::string ln;

    while (std::getline(lines, ln, delim)) {
        res.push_back(ln);
    }

    return res;
}

/* Memory map a file and return the contents */
std::string read_source_code(std::string filename, compile_info &c_info) {
    int input_file = open(filename.c_str(), O_RDONLY);
    c_info.err.on_true(input_file < 0, "Could not open file '%s'\n",
                       filename.c_str());

    struct stat file_stat;
    c_info.err.on_true((fstat(input_file, &file_stat)) < 0,
                       "Could not stat file '%s'\n", filename.c_str());

    int input_size = file_stat.st_size;

    char *c_res = (char *)mmap(nullptr, input_size, PROT_READ, MAP_PRIVATE,
                               input_file, 0);
    c_info.err.on_true(c_res == MAP_FAILED, "Failed to map file '%s'\n",
                       filename.c_str());

    std::string result = c_res;

    close(input_file);

    return result;
}

/* Get the index of the next token of type 'ty' on line.
 * Return ts.size() on failure. */
size_t next_of_type_on_line(std::vector<std::shared_ptr<token>> ts,
                            size_t start, token_type ty) {
    for (size_t i = start; i < ts.size(); i++) {
        if (ts[i]->get_type() == TK_EOL) {
            return ts.size();
        } else if (ts[i]->get_type() == ty) {
            return i;
        }
    }

    return ts.size();
}
