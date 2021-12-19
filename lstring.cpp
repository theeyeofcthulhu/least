#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "ast.h"
#include "error.h"
#include "lstring.h"

const std::map<char, std::string> str_tokens = {
    std::make_pair('n', "\",0xa,\""),   /* Newline */
    std::make_pair('t', "\",0x9,\""),   /* Tabstop */
    std::make_pair('\\', "\\"),         /* The character '\' */
    std::make_pair('\"', "\",0x22,\""), /* The character '"' */
    std::make_pair('\\', "\",0x27,\""), /* The character "'" */
    std::make_pair('[', "\",0x5B,\""),  /* The character '[' */
    std::make_pair(']', "\",0x5D,\""),  /* The character ']' */
};

/* Check validity of string and insert escape sequences */
/* Also parse any '[var]' blocks and insert variable tokens */
std::shared_ptr<t_lstr> parse_string(std::string string, int line,
                                     compile_info &c_info) {
    std::shared_ptr<t_lstr> res = std::make_shared<t_lstr>(line);

    int string_len = string.length();

    int n_quotes = 0;

    std::stringstream ss;

    /* Parse string that comes after the instruction */
    for (int i = 0; i < string_len; i++) {
        switch (string[i]) {
        /* Check number of quotes */
        case '"':
        {
            if (n_quotes == 0)
                c_info.err.on_false(
                    (i + 1) < string_len,
                    "Reached end of line while parsing first '\"' in string\n");

            n_quotes++;
            c_info.err.on_true(
                n_quotes > 2,
                "Found more than two '\"' while parsing string\n");
            if (n_quotes == 2)
                c_info.err.on_false(
                    (i + 1) == string_len,
                    "Expected end of string after second quotation\n");
            break;
        }
        case '\\':
        {
            i++;
            c_info.err.on_true(
                i >= string_len - 1,
                "Reached end of line while trying to parse escape sequence\n");
            auto expanded = str_tokens.find(string[i]);

            c_info.err.on_true(expanded == str_tokens.end(),
                               "Could not parse escape sequence: '\\%c'",
                               string[i]);

            ss << str_tokens.at(string[i]);
            break;
        }
        /* We found a format parameter: parse the tokens in it */
        case '[':
        {
            std::string part = ss.str();
            ss.str(std::string());

            if (!part.empty())
                res->ts.push_back(std::make_shared<t_str>(line, part));

            std::string string_end = string.substr(i, std::string::npos);
            size_t next_bracket = string_end.find(']');

            c_info.err.on_true(next_bracket == std::string::npos,
                               "'[' without closing ']'\n");

            std::string inside = string_end.substr(1, next_bracket - 1);

            c_info.err.on_true(inside.find('[') != std::string::npos,
                               "Found '[' inside format argument");

            std::vector<std::shared_ptr<token>> parsed_inside =
                lex_source(inside, c_info);

            c_info.err.on_true(parsed_inside.empty(),
                               "Could not parse format parameter to tokens\n");
            c_info.err.on_true(
                parsed_inside.size() <= 0 || parsed_inside.size() >= 3,
                "No or more than one token in format parameter\n");

            /* Remove eol from end */
            parsed_inside.pop_back();

            for (auto tk : parsed_inside) {
                c_info.err.on_false(
                    tk->get_type() == TK_ARIT || tk->get_type() == TK_VAR ||
                        tk->get_type() == TK_NUM || tk->get_type() == TK_EOL,
                    "Only variables, numbers and operators are allowed inside "
                    "a format parameter\n");
                res->ts.push_back(tk);
            }

            i += next_bracket;

            break;
        }
        case ']':
        {
            c_info.err.error("Unexpected closing ']'\n");
            break;
        }
        default:
        {
            ss << string[i];
            break;
        }
        }
        c_info.err.on_true(string[i] != '"' && n_quotes == 0,
                           "First '\"' not at beginning of parsed sequence, "
                           "instead found '%c'\n",
                           string[i]);
    }
    c_info.err.on_false(n_quotes == 2,
                        "Did not find two '\"' while parsing string\n");

    std::string final_str = ss.str();

    if (!final_str.empty())
        res->ts.push_back(std::make_shared<t_str>(line, final_str));

    return res;
}
