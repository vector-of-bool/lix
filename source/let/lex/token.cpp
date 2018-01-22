#include "token.hpp"

#include <cassert>
#include <cctype>

#ifdef LET_DEBUG_LEXER
#include <iostream>
namespace {
template <typename... Ts> void debug_lexer(Ts&&... ts) {
    (std::cout << ... << ts) << '\n';
}
}
#else
namespace {
template <typename... Ts> void debug_lexer(Ts&&...) {
}
}
#endif

using str_iter = let::string_view::iterator;

std::vector<let::lex::token_view> let::lex::tokenize(const str_iter doc_first,
                                                     const str_iter doc_last) {
    /**
     * Here's how we lex:
     *
     * We're a little state machine. We have two important iterators, and some
     * extra state.
     *
     * - `_last_end` is the iterator representing our currently tokenized
     * position in the document.
     * - `_peek` is an iterator that we move and increment while scanning for a
     *   new token.
     *
     * Once we produce a token, we set `_pos` to `_peek` and update our source
     * location information
     */
    str_iter last_end = doc_first;
    str_iter peek = doc_first;

    /**
     * We update the source location when we consume a token. We start at column
     */
    source_location pos{1, 1};

    /// Here's our accumulator
    std::vector<token_view> tokens_acc;

    /**
     * We keep track of state using this enum to represent our current states,
     * used to jump around in the switch block.
     */
    enum tok_state {
        base,
        word,
        integral,
        decimal,
        whitespace,
        symbol,
    };

    tok_state state = base;

    auto n_to_consume = std::distance(doc_first, doc_last);
    debug_lexer("Lexing ", n_to_consume, " characters of source");

    auto current_peek_string = [&]() -> string_view {
        return {last_end, static_cast<std::size_t>(std::distance(last_end, peek))};
    };

    auto advance_to_peek = [&]() -> source_range {
        auto n_consumed = std::distance(last_end, peek);
        debug_lexer("Consuming ", n_consumed, " characters");
        const auto start = pos;
        while (last_end != peek) {
            pos.col++;
            if (*last_end == '\n') {
                pos.col = 1;
                pos.line++;
            }
            ++last_end;
        }
        const auto end = pos;
        last_end = peek;
        return {start, end};
    };

    auto commit_token = [&](token_type t) {
        auto str = current_peek_string();
        auto range = advance_to_peek();
        debug_lexer("Commit token '", str, "' of type ", int(t), " with range ", range);
        // Nice rhyme
        tokens_acc.emplace_back(str, range, t);
    };

    auto commit_symbol = [&](char c) {
        switch (c) {
#define SYMBOLS                                                                                    \
    X('(', l_paren)                                                                                \
    X(')', r_paren)                                                                                \
    X('[', l_bracket)                                                                              \
    X(']', r_bracket)                                                                              \
    X('.', dot)                                                                                    \
    X('+', plus)                                                                                   \
    X('-', minus)                                                                                  \
    X('/', div)                                                                                    \
    X('*', mul)                                                                                    \
    X(',', comma)                                                                                  \
    X('|', bar)                                                                                    \
    X(';', semicolon)
#define X(character, type)                                                                         \
    case character:                                                                                \
        ++peek;                                                                                    \
        commit_token(token_type::type);                                                            \
        state = base;                                                                              \
        break;
            SYMBOLS
#undef X
#undef SYMBOLS
        default:
            throw std::runtime_error{"Unexpected character in document: " + std::string(&c, 1)};
        }
    };

    while (1) {
        debug_lexer("Lex loop step at ", pos);
        debug_lexer("Current accumulation: '", current_peek_string(), "'");
        if (peek == doc_last) {
            // We've reached the end, but we may have another token to commit
            switch (state) {
            case base:
            case whitespace:
                // Nothing to do
                break;
            case word:
                commit_token(token_type::word);
                break;
            case integral:
                commit_token(token_type::integer);
                break;
            case decimal:
                commit_token(token_type::decimal);
                break;
            case symbol:
                assert(false && "Unreachable: Uncommitted symbol token");
                break;
            }
            // We always add an EOF token
            commit_token(token_type::eof);
            break;
        }
        const auto c = *peek;
        debug_lexer("Next char is '", c, "'");
        switch (state) {
        /**
         * Base case: We start in this state and reset when we consume a token
         */
        case base: {
            if (std::isalpha(c)) {
                // We're consuming a word
                state = word;
                ++peek;
                break;
            } else if (std::isspace(c)) {
                state = whitespace;
                break;
            } else if (std::isdigit(c)) {
                state = integral;
                break;
            } else {
                state = symbol;
                break;
            }
            assert(false && "Unreachable?");
            std::terminate();
        }
        /**
         * Whitespace. Boring. We throw it away, (unless it's a newline. We like those)
         */
        case whitespace: {
            if (!std::isspace(c)) {
                state = base;
                // No more space. We don't commit a token here, though.
                break;
            }
            if (c == '\n') {
                // Newline is a token on its own
                ++peek;
                commit_token(token_type::newline);
            } else {
                ++peek;
            }
            advance_to_peek();
            break;
        }
        /**
         * A word! These start with an alpha and can contain alpha, numbers,
         * underscore, question mark (?), and exclamation mark (!)
         */
        case word: {
            if (std::isalnum(c) || c == '_' || c == '?' || c == '!') {
                // Still in a word. Keep going
                ++peek;
                break;
            } else {
                // We got a token
                commit_token(token_type::word);
                state = base;
                break;
            }
        }
        /**
         * Numbers. These start with a digit and can contain alpha, numbers,
         * or single-quote tick '
         */
        case decimal:
        case integral: {
            if (std::isalnum(c) || c == '\'') {
                // Still a number
                ++peek;
                break;
            } else if (c == '.' && state != decimal) {
                // We're in decimal town now
                ++peek;
                state = decimal;
                break;
            } else {
                // Got that number
                commit_token(state == integral ? token_type::integer : token_type::decimal);
                state = base;
                break;
            }
        }
        /**
         * Parsing a symbol can be kind of crazy
         */
        case symbol: {
            commit_symbol(c);
            break;
        }
            assert(false && "Unreachable?");
            std::terminate();
        }
    }

    return tokens_acc;
}
