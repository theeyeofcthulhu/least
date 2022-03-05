#include <charconv>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "dictionary.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include "lstring.hpp"
#include "maps.hpp"
#include "util.hpp"

/* TODO: better alertion of wrong termination of function calls, along the lines
 * of error: function calls must be terminated with a ';' */

namespace lexer {

/*
 * Checks if s contains a banned character; errors out if that
 * is the case
 */
void checkbanned(std::string_view s, CompileInfo& c_info);

void checkbanned(std::string_view s, CompileInfo& c_info)
{
    for (char c : s) {
        c_info.err.on_true((isdigit(c) || !isascii(c) || ispunct(c)),
            "Invalid character in variable name: '{}'", s);
    }
}

void debug_tokens(const std::vector<std::shared_ptr<Token>>& ts)
{
    fmt::print("----- DEBUG INFO FOR TOKENS -----\n");
    for (const auto& tk : ts) {
        fmt::print("{}: {}\n", tk->get_line(), token_str_map.at(tk->get_type()));
    }
    fmt::print("---------------------------------\n");
}

bool has_next_arg(const std::vector<std::shared_ptr<Token>>& ts, size_t& len)
{
    while ((ts[len]->get_type() != lexer::TK_SEP) && (ts[len]->get_type() != lexer::TK_EOL))
        len += 1;

    return ts[len]->get_type() == lexer::TK_SEP;
}

std::vector<std::shared_ptr<Token>> do_lex(std::string_view source,
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
        c_info.err.on_true(words.empty(), "Could not split line into words");

        for (size_t j = 0; j < line.length(); j++) {
            if (line[j] == ' ') {
                continue;
            } else if (line[j] == '\"' || line[j] == '\'') {
                /* Parse string literal or character constant */

                char quote = line[j]; /* Could be '"' or ''' */
                std::string_view united;

                size_t k;
                for (k = j + 1; k < line.length(); k++) {
                    if (line[k] == quote && line[k - 1] != '\\') {
                        if (k + 1 < line.length()) {
                            c_info.err.on_false(std::isspace(line[k + 1]),
                                "Quote not end of word in line: '{}'", line);
                        }
                        united = line.substr(j, k - j + 1);
                        break;
                    }
                }
                j = k; /* Advance current char to end of string */

                c_info.err.on_true(united.empty(),
                    "Could not find end of string or character constant");

                std::shared_ptr<Token> parsed;

                /* NOTE: overloaded function? */
                if (quote == '\"')
                    parsed = parse_string(united, i, c_info);
                else if (quote == '\'')
                    parsed = parse_char(united, i, c_info);

                tokens.push_back(parsed);

                continue;
            } else if (std::isdigit(line[j])) {
                size_t next_i;
                std::string_view next_word = get_next_word(line, j, next_i);
                int result;

                const char* last = next_word.data() + next_word.size();

                /* '0' base makes it possible to convert hexadecimal, decimal and octal numbers alike */
                auto [ptr, ec] = std::from_chars(next_word.data(), last, result);

                /* The end of the word was not reached or the beginning not left: conversion failed */
                c_info.err.on_true(ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range || ptr != last, "Could not convert '{}' to an integer", next_word);

                tokens.push_back(std::make_shared<Num>(i, result));

                j += next_i - 1;

                continue;
            }

            size_t next_i;
            auto next_word = get_next_word(line, j, next_i);

            if (cmp_map.find(next_word) != cmp_map.end()) {
                tokens.push_back(std::make_shared<Cmp>(i, cmp_map.at(next_word)));
            } else if (str_key_map.find(next_word) != str_key_map.end()) {
                tokens.push_back(std::make_shared<Key>(i, str_key_map.at(next_word)));
            } else if (arit_map.find(next_word) != arit_map.end()) {
                tokens.push_back(std::make_shared<Arit>(i, arit_map.at(next_word)));
            } else if (log_map.find(next_word) != log_map.end()) {
                tokens.push_back(std::make_shared<Log>(i, log_map.at(next_word)));
            } else if (next_word == "->") {
                j += next_i + 1;
                auto after = get_next_word(line, j, next_i);

                /* FIXME: this should not be part of lexical analysis */
                value_func_id id;
                try {
                    keyword key = str_key_map.at(after);
                    id = key_vfunc_map.at(key);
                } catch (std::out_of_range& e) {
                    c_info.err.error("Could not convert '{}' to evaluable function", after);
                }

                tokens.push_back(std::make_shared<Call>(i, id));
            } else if (next_word == ";") {
                tokens.push_back(std::make_shared<Sep>(i));
            } else if (size_t open, close; (open = next_word.find('{') != std::string_view::npos) && (close = next_word.find('}') != std::string_view::npos)) {
                c_info.err.on_false(next_word.ends_with('}'), "Expected '}}' at the end of word '{}'", next_word);

                std::vector<std::shared_ptr<Token>> parsed_inside = do_lex(next_word.substr(open + 1, close - open + 1), c_info, true);

                c_info.err.on_true(parsed_inside.empty(),
                    "Could not parse format parameter to tokens");
                /* Remove eol from end */
                parsed_inside.pop_back();

                tokens.push_back(std::make_shared<Access>(i, next_word.substr(0, open), parsed_inside));
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
