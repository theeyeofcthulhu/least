#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dictionary.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include "lstring.hpp"
#include "util.hpp"

namespace lexer {

const std::map<std::string, keyword> key_map{
    std::make_pair("print", K_PRINT), std::make_pair("exit", K_EXIT),
    std::make_pair("if", K_IF),       std::make_pair("elif", K_ELIF),
    std::make_pair("else", K_ELSE),   std::make_pair("while", K_WHILE),
    std::make_pair("end", K_END),     std::make_pair("int", K_INT),
    std::make_pair("str", K_STR),     std::make_pair("read", K_READ),
    std::make_pair("set", K_SET),     std::make_pair("putchar", K_PUTCHAR),
    std::make_pair("add", K_ADD),
};

const std::map<std::string, cmp_op> cmp_map{
    std::make_pair("==", EQUAL),  std::make_pair("!=", NOT_EQUAL),
    std::make_pair("<", LESS),    std::make_pair("<=", LESS_OR_EQ),
    std::make_pair(">", GREATER), std::make_pair(">=", GREATER_OR_EQ),
};

const std::map<std::string, arit_op> arit_map{
    std::make_pair("+", ADD), std::make_pair("-", SUB),
    std::make_pair("%", MOD), std::make_pair("/", DIV),
    std::make_pair("*", MUL),
};

const std::map<token_type, std::string> token_str_map{
    std::make_pair(lexer::TK_KEY, "key"),
    std::make_pair(lexer::TK_ARIT, "arit"),
    std::make_pair(lexer::TK_CMP, "cmp"),
    std::make_pair(lexer::TK_LOG, "log"),
    std::make_pair(lexer::TK_STR, "str"),
    std::make_pair(lexer::TK_LSTR, "lstr"),
    std::make_pair(lexer::TK_CHAR, "char"),
    std::make_pair(lexer::TK_NUM, "num"),
    std::make_pair(lexer::TK_VAR, "var"),
    std::make_pair(lexer::TK_SEP, "sep"),
    std::make_pair(lexer::TK_EOL, "eol"),
    std::make_pair(lexer::TK_INV, "inv"),
};

void checkbanned(std::string s, compile_info &c_info)
{
    for (char c : s)
        c_info.err.on_true((isdigit(c) || !isascii(c) || ispunct(c)),
                           "Invalid character in variable name: '%s'\n",
                           s.c_str());
}

bool has_next_arg(std::vector<std::shared_ptr<token>> ts, size_t &len)
{
    while ((ts[len]->get_type() != lexer::TK_SEP) &&
           (ts[len]->get_type() != lexer::TK_EOL))
        len += 1;

    return ts[len]->get_type() == lexer::TK_SEP;
}

void debug_tokens(std::vector<std::shared_ptr<token>> ts)
{
    std::cout << "----- DEBUG INFO FOR TOKENS -----\n";
    for (auto tk : ts) {
        std::cout << tk->get_line() << ": " << token_str_map.at(tk->get_type())
                  << '\n';
    }
    std::cout << "---------------------------------\n";
}

std::vector<std::shared_ptr<token>>
do_lex(std::string source, compile_info &c_info, bool no_set_line)
{
    std::vector<std::shared_ptr<token>> tokens;

    auto lines = split(source, '\n');

    for (size_t i = 0; i < lines.size(); i++) {
        auto line = lines[i];
        if (line.empty())
            continue;

        /* Caller can optionally provide no_set_line; this disables setting the
         * line on the error object when only calling to extract tokens from a
         * single line */
        if (!no_set_line)
            c_info.err.set_line(i);

        auto words = split(line, ' ');
        c_info.err.on_true(words.empty(), "Could not split line into words\n");

        for (size_t j = 0; j < line.length(); j++) {
            if (line[j] == ' ')
                continue;
            if (line[j] == '\"') {
                std::string united;

                size_t k;
                for (k = j + 1; k < line.length(); k++) {
                    if (line[k] == '\"' && line[k - 1] != '\\') {
                        if (k + 1 < line.length())
                            c_info.err.on_false(
                                std::isspace(line[k + 1]),
                                "Quote not end of word in line: '%s'\n",
                                line.c_str());
                        united = line.substr(j, k - j + 1);
                        break;
                    }
                }
                j = k; /* Advance current char to end of string */

                c_info.err.on_true(united.empty(),
                                   "Could not find end of string\n");

                std::shared_ptr<lstr> parsed = parse_string(united, i, c_info);

                tokens.push_back(parsed);

                continue;
            } else if (std::isdigit(line[j])) {
                size_t next_i;
                auto next_word = get_next_word(line, j, next_i);

                for (auto new_chr : next_word)
                    c_info.err.on_false(
                        std::isdigit(new_chr),
                        "Expected digit, got: '%c' in number '%s'\n", new_chr,
                        next_word.c_str());

                tokens.push_back(
                    std::make_shared<num>(i, std::stoi(next_word)));

                j += next_i - 1;

                continue;
            }

            size_t next_i;
            auto next_word = get_next_word(line, j, next_i);

            if (cmp_map.find(next_word) != cmp_map.end()) {
                tokens.push_back(
                    std::make_shared<cmp>(i, cmp_map.at(next_word)));
            } else if (key_map.find(next_word) != key_map.end()) {
                tokens.push_back(
                    std::make_shared<key>(i, key_map.at(next_word)));
            } else if (arit_map.find(next_word) != arit_map.end()) {
                tokens.push_back(
                    std::make_shared<arit>(i, arit_map.at(next_word)));
            } else if (next_word == ";") {
                tokens.push_back(std::make_shared<sep>(i));
            } else {
                checkbanned(next_word, c_info);
                tokens.push_back(std::make_shared<var>(i, next_word));
            }

            j += next_i - 1;
        }
        tokens.push_back(std::make_shared<eol>(i));
    }

    return tokens;
}

} // namespace lexer
