#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <optional>
#include <charconv>
#include <sstream>

#include "dictionary.hpp"
#include "error.hpp"
#include "lexer.hpp"
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

void debug_tokens(const std::vector<std::shared_ptr<Token>>& ts)
{
    fmt::print("----- DEBUG INFO FOR TOKENS -----\n");
    for (const auto& tk : ts) {
        fmt::print("{}: {}\n", tk->get_line(), token_str_map.at(tk->get_type()));
    }
    fmt::print("---------------------------------\n");
}

// Variables must start with a letter,
// afterwards, letters, numbers, underscores are allowed.
void LexContext::check_correct_var_name(std::string_view s)
{
    c_info.err.on_false(isalpha(s[0]), "Variables must begin with a letter: '{}'", s);

    // We can always do s.substr(1), even when s only has one character, because
    // it just return an empty view, which will just not start the for loop,
    // instead of raising the usual std::out_of_range.
    for (char c : s.substr(1)) {
        c_info.err.on_false(isalpha(c) || isdigit(c) || c == '_',
                           "Invalid character '{}' in variable name: '{}'", c, s);
    }
}

size_t LexContext::find_next_word_ending_char(std::string_view line)
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

void LexContext::remove_leading_space(std::string_view& sv)
{
    auto index_it = std::find_if(sv.begin(), sv.end(), [](char c) {
        return !isspace(c);
    });

    if (index_it == sv.begin())
        return;

    size_t index = index_it - sv.begin();
    sv.remove_prefix(index);
}

std::pair<std::string_view, size_t> LexContext::extract_string(std::string_view line)
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

std::optional<std::pair<std::string_view, size_t>> LexContext::extract_symbol_beginning(std::string_view sv)
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

std::optional<std::string_view> LexContext::next_word(std::string_view& line)
{
    if (line.empty())
        return std::nullopt;

    std::string_view word;
    size_t next_terminator;
    std::optional<std::pair<std::string_view, size_t>> symbol;

    remove_leading_space(line);

    if (line.starts_with('"')) {
        std::tie(word, next_terminator) = extract_string(line);
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

/* Check validity of string and insert escape sequences */
/* Also parse any '[var]' blocks and insert variable tokens */
std::shared_ptr<Lstr> LexContext::parse_string(std::string_view string, int line)
{
    std::shared_ptr<Lstr> res = std::make_shared<Lstr>(line);

    int string_len = string.length();

    c_info.err.on_false(string_len > 2, "String is empty");
    assert(string[0] == '\"' && string[string_len - 1] == '\"');

    std::stringstream ss;

    /* Parse string that comes after the instruction */
    for (int i = 1; i < string_len - 1; i++) {
        switch (string[i]) {
        /* Check number of quotes */
        case '\\': {
            i++;
            c_info.err.on_true(i >= string_len - 1,
                "Reached end of line while trying to parse escape sequence");

            try {
                ss << str_tokens.at(string[i]);
            } catch (std::out_of_range& e) {
                c_info.err.error("Could not parse escape sequence: '\\{}'", string[i]);
            }
            break;
        }
        /* We found a format parameter: parse the tokens in it */
        case '[': {
            std::string part = ss.str();
            ss.str(std::string()); /* Clear stringstream */

            if (!part.empty())
                res->ts.push_back(std::make_shared<Str>(line, part));

            std::string_view string_end = string.substr(i, std::string_view::npos);
            size_t next_bracket = string_end.find(']');

            c_info.err.on_true(next_bracket == std::string_view::npos, "'[' without closing ']'");

            std::string_view inside = string_end.substr(1, next_bracket - 1);

            c_info.err.on_true(inside.find('[') != std::string_view::npos,
                "Found '[' inside format argument");

            LexContext lexer_inside(inside, c_info, true);
            auto parsed_inside = lexer_inside.lex_and_get_tokens();

            c_info.err.on_true(parsed_inside.empty(),
                "Could not parse format parameter to tokens");

            /* Remove eol from end */
            assert(parsed_inside.back()->get_type() == TK_EOL);
            parsed_inside.pop_back();

            for (const auto& tk : parsed_inside) {
                c_info.err.on_false(lexer::could_be_num(tk->get_type()),
                    "Only variables, numbers and operators are allowed inside a format parameter");
                res->ts.push_back(tk);
            }

            i += next_bracket;

            break;
        }
        case ']': {
            c_info.err.error("Unexpected closing ']'");
            break;
        }
        default: {
            ss << string[i];
            break;
        }
        }
    }
    std::string final_str = ss.str();

    if (!final_str.empty())
        res->ts.push_back(std::make_shared<Str>(line, final_str));

    c_info.err.on_true(res->ts.empty(), "lstring format has no contents after parse_string");

    return res;
}

size_t LexContext::find_closing_bracket(Bracket::Purpose purp, size_t after_open)
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

std::shared_ptr<Num> LexContext::parse_char(std::string_view string, int line)
{
    char parsed_char;

    int string_len = string.length();

    c_info.err.on_false(string_len == 3 || string_len == 4,
        "Could not parse string '{}' as character constant", string);

    assert(string[0] == '\'' && string[string_len - 1] == '\'');

    if (string[1] == '\\') {
        c_info.err.on_false(string_len == 4, "Expected another character after '\\'");
        try {
            parsed_char = str_tokens_char.at(string[2]);
        } catch (std::out_of_range& e) {
            c_info.err.error("Could not parse escape sequence '\\{}'", string[2]);
        }
    } else {
        c_info.err.on_false(string_len == 3, "Too many symbols in character constant {}", string);

        parsed_char = string[1];
    }

    return std::make_shared<Num>(line, (int)parsed_char);
}

std::shared_ptr<Token> LexContext::token_from_word(std::string_view word, int line)
{
    if (word.starts_with('"')) {
        return parse_string(word, line);
    } else if (word.starts_with('\'')) {
        return parse_char(word, line);
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
        check_correct_var_name(word);
        return std::make_shared<Var>(line, word);
    }
}

void LexContext::consolidate()
{
    consolidate(tokens);
}

/* Consolidates array accesses and VFunc calls into those respective objects.
 * Tail recursive, because after modifying the vector,
 * the indexes are going to get messed up. */
void LexContext::consolidate(std::vector<std::shared_ptr<Token>>& p_tokens)
{
    for (size_t i = 0; i < p_tokens.size(); i++) {
        const auto& tk = p_tokens[i];

        c_info.err.set_line(tk->get_line());

        if (tk->get_type() == TK_BRACKET) {
            auto brack = LEXER_SAFE_CAST(Bracket, tk);
            if (brack->get_purpose() == Bracket::Purpose::Access) {
                c_info.err.on_true(i == 0 || p_tokens[i - 1]->get_type() != TK_VAR, "'{' not following variable");
                auto var = LEXER_SAFE_CAST(Var, p_tokens[i - 1]);

                c_info.err.on_true(brack->get_kind() == Bracket::Kind::Close, "Unexpected closing '}'");

                size_t index = find_closing_bracket(Bracket::Purpose::Access, i + 1);

                /* NOTE: is there a better way to extract a range of tokens from the original
                 * vector and move it into the new vector? */
                std::vector<std::shared_ptr<Token>> extract(p_tokens.begin() + i + 1, p_tokens.begin() + index - 1);
                consolidate(extract); /* Make sure nested accesses don't get overlooked */

                p_tokens.erase(p_tokens.begin() + i - 1, p_tokens.begin() + index);
                p_tokens.insert(p_tokens.begin() + i - 1, std::make_shared<Access>(brack->get_line(), var->get_name(), extract));

                consolidate();
                return;
            }
        } else if (tk->get_type() == TK_CALL) {
            c_info.err.on_true(i == p_tokens.size() - 1, "No more p_tokens after '->'");
            c_info.err.on_false(p_tokens[i + 1]->get_type() == TK_KEY, "No key after '->'");

            auto key = LEXER_SAFE_CAST(Key, p_tokens[i + 1]);
            value_func_id vfunc;
            try {
                vfunc = key_vfunc_map.at(key->get_key());
            } catch (std::out_of_range& e) {
                c_info.err.error("Key '{}' not convertible to evaluable function", key_str_map.at(key->get_key()));
            }

            p_tokens.erase(p_tokens.begin() + i, p_tokens.begin() + i + 2);
            p_tokens.insert(p_tokens.begin() + i, std::make_shared<CompleteCall>(key->get_line(), vfunc));

            consolidate();
            return;
        }
    }
}

std::vector<std::shared_ptr<Token>> LexContext::lex_and_get_tokens()
{
    auto lines = split(source_code, '\n');

    for (size_t i = 0; i < lines.size(); i++) {
        auto line = lines[i];
        if (line.empty())
            continue;

        /* Caller can optionally provide no_set_line; this disables setting the
         * line on the error object when only calling to extract tokens from a
         * single line */
        if (!m_no_set_line)
            c_info.err.set_line(i);

        std::optional<std::string_view> word;

        while((word = next_word(line))) {
            tokens.push_back(token_from_word(*word, i));
        }

        tokens.push_back(std::make_shared<Eol>(i));
    }

    consolidate();

    return tokens;
}

} // namespace lexer
