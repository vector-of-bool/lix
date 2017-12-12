#include "token.hpp"

#include <cassert>
#include <cctype>

using str_iter = std::string_view::iterator;

namespace {

using namespace let::ast;

struct tokenizer {
    enum class state {
        base,
        bareword,
        numeric_start,
        hex_number,
        octal_number,
        binary_number,
        point_number,
    };
    str_iter _acc_first;
    str_iter _acc_last = _acc_first;
    const str_iter _last;
    std::vector<let::ast::token_view> _acc;
    state _st = state::base;

    source_location start = { 1, 1 };
    source_location end = { 1, 1 };

    tokenizer(str_iter first, str_iter last)
        : _acc_first(first)
        , _last(last) {}


    std::vector<let::ast::token_view> tokenize() {
        while (_acc_first != _last) {
            _st = _tok_iter();
        }
        return std::move(_acc);
    }

    state _tok_iter() {
        switch (_st) {
        case state::base:
            return _tok_base();
        case state::bareword:
            return _tok_bareword();
        case state::numeric_start:
            return _tok_numeric_start();
        }
        // assert(false && "Unreachable");
    }

    state _tok_base() {
        if (std::isalpha(*_acc_first)) {
            return state::bareword;
        } else if (std::isspace(*_acc_first)) {
            start.col++;
            if (*_acc_first == '\n') {
                ++_acc_last;
                end.line = start.line + 1;
                end.col = 1;
                return _commit_token();
            }
            end = start;
            ++_acc_last;
            ++_acc_first;
            return _st;
        } else if (std::isdigit(*_acc_first)) {
            return state::numeric_start;
        }
        assert(false && "Unreachable");
        std::terminate();
    }

    state _tok_bareword() {
        end.col++;
        ++_acc_last;
        if (_acc_last == _last) {
            // End of file
            return _commit_token();
        }
        if (!std::isalnum(*_acc_last)) {
            // End of bare word
            return _commit_token();
        }
        // Another alphanumeric
        return state::bareword;
    }

    state _tok_numeric_start() {
        end.col++;
        ++_acc_last;
        if (_acc_last == _last) {
            return _commit_token();
        }
        if (std::isalpha(*_acc_last) && std::distance(_acc_first, _acc_last) == 1) {
            // Suffix
            switch (*_acc_last) {
            case 'f':
                return state::hex_number;
            case 'c':
                return state::octal_number;
            case 'b':
                return state::binary_number;
            default:
                assert(false && "TODO: THROW REAL ERRORS HERE");
                std::terminate();
            }
        }
        if (*_acc_last == '\'') {
            // Digit separator
            return _st;
        }
        if (*_acc_last == '.') {
            return state::point_number;
        }
        if (std::isdigit(*_acc_last)) {
            return _st;
        }
        return _commit_token();
    }

    state _commit_token() {
        auto ptr = std::addressof(*_acc_first);
        std::size_t len = std::distance(_acc_first, _acc_last);
        token_view tok{ std::string_view{ ptr, len }, { start, end } };
        _acc.emplace_back(std::move(tok));
        _acc_first = _acc_last;
        start = end;
        return state::base;
    }
};

}  // namespace


std::vector<let::ast::token_view> let::ast::tokenize(str_iter first, str_iter last) {
    tokenizer t{ first, last };
    return t.tokenize();
}