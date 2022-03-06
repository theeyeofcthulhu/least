#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <optional>
#include <charconv>

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

Bracket::Bracket(int line, BracketTemplate templ)
    : Token(line)
    , m_purpose(templ.purpose)
    , m_kind(templ.kind)
{
}

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

size_t find_next_word_ending_char(std::string_view line)
{
    auto index = std::find_if(line.begin(), line.end(), [](char c) {
        return std::find(word_ending_chars.begin(), word_ending_chars.end(), c) != word_ending_chars.end();
    });

    if (index == line.end()) {
        return std::string_view::npos;
    } else {
        return index - line.begin();
    }
}

void remove_leading_space(std::string_view& sv)
{
    auto index_it = std::find_if(sv.begin(), sv.end(), [](char c) {
        return !isspace(c);
    });

    if (index_it == sv.begin())
        return;

    size_t index = index_it - sv.begin();
    sv.remove_prefix(index);
}

std::pair<std::string_view, size_t> extract_string(std::string_view line, CompileInfo& c_info)
{
    assert(line.starts_with('"'));

    char last_char = '\0';

    auto begin = line.begin() + 1;

    auto it = std::find_if(begin, line.end(), [&](char c) mutable {
        bool ret = c == '"' && last_char != '\\';

        last_char = c;
        return ret;
    });

    c_info.err.on_true(it == line.end(), "Unterminated string-literal {}", line);

    return std::make_pair(line.substr(0, (it + 1) - (begin - 1)), (it - line.begin() + 1));
}

static std::optional<std::pair<std::string_view, size_t>> extract_symbol_beginning(std::string_view sv)
{
    auto length1 = sv.substr(0, 1);
    auto length2 = sv.substr(0, 2);

    if (str_symbol_map.find(length1) != str_symbol_map.end()) {
        return std::make_optional(std::make_pair(length1, 1));
    } else if (str_symbol_map.find(length2) != str_symbol_map.end()) {
        return std::make_optional(std::make_pair(length2, 2));
    }

    return std::nullopt;
}

std::optional<std::string_view> next_word(std::string_view& line, CompileInfo& c_info)
{
    if (line.empty())
        return std::nullopt;

    std::string_view word;
    size_t next_terminator;
    std::optional<std::pair<std::string_view, size_t>> symbol;

    remove_leading_space(line);

    if (line.starts_with('"')) {
        std::tie(word, next_terminator) = extract_string(line, c_info);
        goto out_ret;
    }

    if ((symbol = extract_symbol_beginning(line))) {
        std::tie(word, next_terminator) = *symbol;
        goto out_ret;
    }

    next_terminator = find_next_word_ending_char(line);

    word = line.substr(0, next_terminator);

out_ret:
    line.remove_prefix(next_terminator <= line.size() ? next_terminator : line.size());

    return std::make_optional(word);
}

std::shared_ptr<Token> token_from_word(std::string_view word, int line, CompileInfo& c_info)
{
    if (word.starts_with('"')) {
        return parse_string(word, line, c_info);
    } else if (word.starts_with('\'')) {
        return parse_char(word, line, c_info);
    } else if (std::isdigit(word[0])) {
        int result;
        const char* last = word.data() + word.size();

        /* '0' base makes it possible to convert hexadecimal, decimal and octal numbers alike */
        auto [ptr, ec] = std::from_chars(word.data(), last, result);

        /* The end of the word was not reached or the beginning not left: conversion failed */
        c_info.err.on_true(ec == std::errc::invalid_argument || ec == std::errc::result_out_of_range || ptr != last, "Could not convert '{}' to an integer", word);

        return std::make_shared<Num>(line, result);
    } else if (word == ";") {
        return std::make_shared<Sep>(line);
    } else if (word == "->") {
        return std::make_shared<Call>(line);
    } else if (cmp_map.find(word) != cmp_map.end()) {
        return std::make_shared<Cmp>(line, cmp_map.at(word));
    } else if (str_key_map.find(word) != str_key_map.end()) {
        return std::make_shared<Key>(line, str_key_map.at(word));
    } else if (arit_map.find(word) != arit_map.end()) {
        return std::make_shared<Arit>(line, arit_map.at(word));
    } else if (log_map.find(word) != log_map.end()) {
        return std::make_shared<Log>(line, log_map.at(word));
    } else if (bracket_map.find(word) != bracket_map.end()) {
        return std::make_shared<Bracket>(line, bracket_map.at(word));
    } else {
        checkbanned(word, c_info);
        return std::make_shared<Var>(line, word);
    }
}

size_t find_closing_bracket(const std::vector<std::shared_ptr<Token>>& tokens, Bracket::Purpose purp, size_t after_open, CompileInfo& c_info)
{
    size_t count, i;

    for (count = 1, i = after_open; i < tokens.size() && count >= 1 && tokens[i]->get_type() != TK_EOL; i++) {
        if (tokens[i]->get_type() == TK_BRACKET) {
            auto brack = LEXER_SAFE_CAST(Bracket, tokens[i]);
            if (brack->get_purpose() == purp) {
                if (brack->get_kind() == Bracket::Kind::Open)
                    count++;
                else
                    count--;
            }
        }
    }

    c_info.err.on_false(count == 0, "Unclosed bracket");

    return i;
}

/* Consolidates array accesses and VFunc calls */
void consolidate(std::vector<std::shared_ptr<Token>>& tokens, CompileInfo& c_info)
{
    for (size_t i = 0; i < tokens.size(); i++) {
        const auto& tk = tokens[i];

        c_info.err.set_line(tk->get_line());

        if (tk->get_type() == TK_BRACKET) {
            auto brack = LEXER_SAFE_CAST(Bracket, tk);
            if (brack->get_purpose() == Bracket::Purpose::Access) {
                c_info.err.on_true(i == 0 || tokens[i - 1]->get_type() != TK_VAR, "'{' not following variable");
                auto var = LEXER_SAFE_CAST(Var, tokens[i - 1]);

                c_info.err.on_true(brack->get_kind() == Bracket::Kind::Close, "Unexpected closing '}'");

                size_t index = find_closing_bracket(tokens, Bracket::Purpose::Access, i + 1, c_info);

                std::vector<std::shared_ptr<Token>> extract(tokens.begin() + i + 1, tokens.begin() + index - 1);
                consolidate(extract, c_info); /* Make sure nested accesses don't get overlooked */

                tokens.erase(tokens.begin() + i - 1, tokens.begin() + index);
                tokens.insert(tokens.begin() + i - 1, std::make_shared<Access>(brack->get_line(), var->get_name(), extract));

                consolidate(tokens, c_info);
                return;
            }
        } else if (tk->get_type() == TK_CALL) {
            c_info.err.on_true(i == tokens.size() - 1, "No more tokens after '->'");
            c_info.err.on_false(tokens[i + 1]->get_type() == TK_KEY, "No key after '->'");

            auto key = LEXER_SAFE_CAST(Key, tokens[i + 1]);
            value_func_id vfunc;
            try {
                vfunc = key_vfunc_map.at(key->get_key());
            } catch (std::out_of_range& e) {
                c_info.err.error("Key '{}' not convertible to evaluable function", key_str_map.at(key->get_key()));
            }

            tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
            tokens.insert(tokens.begin() + i, std::make_shared<CompleteCall>(key->get_line(), vfunc));

            consolidate(tokens, c_info);
            return;
        }
    }
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

        std::optional<std::string_view> word;

        while((word = next_word(line, c_info))) {
            tokens.push_back(token_from_word(*word, i, c_info));
        }

        tokens.push_back(std::make_shared<Eol>(i));
    }

    consolidate(tokens, c_info);

    return tokens;
}

} // namespace lexer
