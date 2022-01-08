#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "ast.hpp"
#include "lexer.hpp"
#include "lstring.hpp"

namespace lexer {

const std::map<char, std::string> str_tokens = {
    std::make_pair('n', "\",0xa,\""),   /* Newline */
    std::make_pair('t', "\",0x9,\""),   /* Tabstop */
    std::make_pair('\\', "\\"),         /* The character '\' */
    std::make_pair('\"', "\",0x22,\""), /* The character '"' */
    std::make_pair('\'', "\",0x27,\""), /* The character "'" */
    std::make_pair('[', "\",0x5B,\""),  /* The character '[' */
    std::make_pair(']', "\",0x5D,\""),  /* The character ']' */
};

const std::map<char, char> str_tokens_char = {
    std::make_pair('n', '\n'),  /* Newline */
    std::make_pair('t', '\t'),  /* Tabstop */
    std::make_pair('\\', '\\'), /* The character '\' */
    std::make_pair('\"', '\"'), /* The character '"' */
    std::make_pair('\'', '\''), /* The character ''' */
    std::make_pair('[', '['),   /* The character '[' */
    std::make_pair(']', ']'),   /* The character ']' */
};

/* Check validity of string and insert escape sequences */
/* Also parse any '[var]' blocks and insert variable tokens */
std::shared_ptr<lstr> parse_string(std::string string, int line,
                                   compile_info &c_info)
{
    std::shared_ptr<lstr> res = std::make_shared<lstr>(line);

    int string_len = string.length();

    c_info.err.on_false(string_len > 2, "String is empty\n");
    assert(string[0] == '\"' && string[string_len - 1] == '\"');

    std::stringstream ss;

    /* Parse string that comes after the instruction */
    for (int i = 1; i < string_len - 1; i++) {
        switch (string[i]) {
        /* Check number of quotes */
        case '\\':
        {
            i++;
            c_info.err.on_true(
                i >= string_len - 1,
                "Reached end of line while trying to parse escape sequence\n");

            try {
                ss << str_tokens.at(string[i]);
            } catch (std::out_of_range &e) {
                c_info.err.error("Could not parse escape sequence: '\\%c'\n",
                                 string[i]);
            }
            break;
        }
        /* We found a format parameter: parse the tokens in it */
        case '[':
        {
            std::string part = ss.str();
            ss.str(std::string()); /* Clear stringstream */

            if (!part.empty())
                res->ts.push_back(std::make_shared<str>(line, part));

            std::string string_end = string.substr(i, std::string::npos);
            size_t next_bracket = string_end.find(']');

            c_info.err.on_true(next_bracket == std::string::npos,
                               "'[' without closing ']'\n");

            std::string inside = string_end.substr(1, next_bracket - 1);

            c_info.err.on_true(inside.find('[') != std::string::npos,
                               "Found '[' inside format argument");

            std::vector<std::shared_ptr<token>> parsed_inside =
                do_lex(inside, c_info, true);

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
    }
    std::string final_str = ss.str();

    if (!final_str.empty())
        res->ts.push_back(std::make_shared<str>(line, final_str));

    c_info.err.on_true(res->ts.empty(),
                       "lstring format has no contents after parse_string\n");

    return res;
}

std::shared_ptr<num> parse_char(std::string string, int line,
                                   compile_info &c_info)
{
    char parsed_char;

    int string_len = string.length();

    c_info.err.on_false(string_len == 3 || string_len == 4,
                       "Could not parse string '%s' as character constant\n",
                       string.c_str());

    assert(string[0] == '\'' && string[string_len-1] == '\'');

    if(string[1] == '\\') {
        c_info.err.on_false(string_len == 4, "Expected another character after '\\'\n");
        try {
            parsed_char = str_tokens_char.at(string[2]);
        } catch (std::out_of_range &e) {
            c_info.err.error("Could not parse escape sequence '\\%c'\n", string[2]);
        }
    } else {
        c_info.err.on_false(string_len == 3,
                            "Too many symbols in character constant %s\n",
                            string.c_str());

        parsed_char = string[1];
    }

    return std::make_shared<num>(line, (int)parsed_char);
}

} // namespace lexer
