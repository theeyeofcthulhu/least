#include "lexer.hpp"

#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "dictionary.hpp"
#include "error.hpp"
#include "lstring.hpp"
#include "util.hpp"

/* TODO: better alertion of wrong termination of function calls, along the lines
 * of error: function calls must be terminated with a ';' */

namespace lexer {

/*
 * Checks if s contains a banned character; errors out if that
 * is the case
 */
void checkbanned(const std::string& s, CompileInfo& c_info);

const std::map<std::string, keyword> key_map {
    std::make_pair("print", K_PRINT),
    std::make_pair("exit", K_EXIT),
    std::make_pair("if", K_IF),
    std::make_pair("elif", K_ELIF),
    std::make_pair("else", K_ELSE),
    std::make_pair("while", K_WHILE),
    std::make_pair("end", K_END),
    std::make_pair("int", K_INT),
    std::make_pair("str", K_STR),
    std::make_pair("read", K_READ),
    std::make_pair("set", K_SET),
    std::make_pair("putchar", K_PUTCHAR),
    std::make_pair("add", K_ADD),
    std::make_pair("sub", K_SUB),
    std::make_pair("break", K_BREAK),
    std::make_pair("continue", K_CONT),
    std::make_pair("time", K_TIME),
    std::make_pair("getuid", K_GETUID),
};

const std::map<std::string, cmp_op> cmp_map {
    std::make_pair("==", EQUAL),
    std::make_pair("!=", NOT_EQUAL),
    std::make_pair("<", LESS),
    std::make_pair("<=", LESS_OR_EQ),
    std::make_pair(">", GREATER),
    std::make_pair(">=", GREATER_OR_EQ),
};

const std::map<std::string, arit_op> arit_map {
    std::make_pair("+", ADD),
    std::make_pair("-", SUB),
    std::make_pair("%", MOD),
    std::make_pair("/", DIV),
    std::make_pair("*", MUL),
};

const std::map<std::string, log_op> log_map {
    std::make_pair("&&", AND),
    std::make_pair("||", OR),
};

const std::map<token_type, std::string> token_str_map {
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
    std::make_pair(lexer::TK_CALL, "call"),
    std::make_pair(lexer::TK_EOL, "eol"),
    std::make_pair(lexer::TK_INV, "inv"),
};

void checkbanned(const std::string& s, CompileInfo& c_info)
{
    for (char c : s)
        c_info.err.on_true((isdigit(c) || !isascii(c) || ispunct(c)),
            "Invalid character in variable name: '%'\n", s);
}

void debug_tokens(const std::vector<std::shared_ptr<Token>>& ts)
{
    std::cout << "----- DEBUG INFO FOR TOKENS -----\n";
    for (const auto& tk : ts) {
        std::cout << tk->get_line() << ": " << token_str_map.at(tk->get_type()) << '\n';
    }
    std::cout << "---------------------------------\n";
}

bool has_next_arg(const std::vector<std::shared_ptr<Token>>& ts, size_t& len)
{
    while ((ts[len]->get_type() != lexer::TK_SEP) && (ts[len]->get_type() != lexer::TK_EOL))
        len += 1;

    return ts[len]->get_type() == lexer::TK_SEP;
}

std::vector<std::shared_ptr<Token>> do_lex(const std::string& source,
    CompileInfo& c_info,
    bool no_set_line)
{
    std::vector<std::shared_ptr<Token>> tokens;

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
            if (line[j] == ' ') {
                continue;
            } else if (line[j] == '\"' || line[j] == '\'') {
                /* Parse string literal or character constant */

                char quote = line[j]; /* Could be '"' or ''' */
                std::string united;

                size_t k;
                for (k = j + 1; k < line.length(); k++) {
                    if (line[k] == quote && line[k - 1] != '\\') {
                        if (k + 1 < line.length())
                            c_info.err.on_false(std::isspace(line[k + 1]),
                                "Quote not end of word in line: '%'\n", line);
                        united = line.substr(j, k - j + 1);
                        break;
                    }
                }
                j = k; /* Advance current char to end of string */

                c_info.err.on_true(united.empty(),
                    "Could not find end of string or character constant\n");

                std::shared_ptr<Token> parsed;

                if (quote == '\"')
                    parsed = parse_string(united, i, c_info);
                else if (quote == '\'')
                    parsed = parse_char(united, i, c_info);

                tokens.push_back(parsed);

                continue;
            } else if (std::isdigit(line[j])) {
                size_t next_i;
                auto next_word = get_next_word(line, j, next_i);

                /* TODO: do the whole conversion manually, don't
                 * check if every character is a digit and then call stoi()
                 * (stoi() doesn't throw on something like 2312ddfdf, so we need
                 * to have a whole thing...) */
                for (auto new_chr : next_word)
                    c_info.err.on_false(std::isdigit(new_chr),
                        "Expected digit, got: '%' in number '%'\n", new_chr,
                        next_word);

                tokens.push_back(std::make_shared<Num>(i, std::stoi(next_word)));

                j += next_i - 1;

                continue;
            }

            size_t next_i;
            auto next_word = get_next_word(line, j, next_i);

            if (cmp_map.find(next_word) != cmp_map.end()) {
                tokens.push_back(std::make_shared<Cmp>(i, cmp_map.at(next_word)));
            } else if (key_map.find(next_word) != key_map.end()) {
                tokens.push_back(std::make_shared<Key>(i, key_map.at(next_word)));
            } else if (arit_map.find(next_word) != arit_map.end()) {
                tokens.push_back(std::make_shared<Arit>(i, arit_map.at(next_word)));
            } else if (log_map.find(next_word) != log_map.end()) {
                tokens.push_back(std::make_shared<Log>(i, log_map.at(next_word)));
            } else if (next_word == "->") {
                j += next_i + 1;
                auto after = get_next_word(line, j, next_i);

                value_func_id id;
                try {
                    keyword key = key_map.at(after);
                    id = key_vfunc_map.at(key);
                } catch (std::out_of_range& e) {
                    c_info.err.error("Could not convert '%' to evaluable function\n", after);
                }

                tokens.push_back(std::make_shared<Call>(i, id));
            } else if (next_word == ";") {
                tokens.push_back(std::make_shared<Sep>(i));
            } else {
                checkbanned(next_word, c_info);
                tokens.push_back(std::make_shared<Var>(i, next_word));
            }

            j += next_i - 1;
        }
        tokens.push_back(std::make_shared<Eol>(i));
    }

    return tokens;
}

} // namespace lexer
