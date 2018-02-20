#include "parse.hpp"

#include <iostream>
#include <optional>
#include <sstream>
#include <stack>

using namespace let;
using namespace let::ast;

#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/tracer.hpp>

#ifndef NDEBUG
// #define DEBUG_PARSER 1
#define DEBUG_PARSER 0
#else
#define DEBUG_PARSER 0
#endif

namespace pegtl = tao::pegtl;

namespace {

#define KW(x) TAOCPP_PEGTL_KEYWORD(x)
#define STR(x) TAOCPP_PEGTL_STRING(x)

using namespace pegtl;

template <typename Rule>
struct is_logging : std::false_type {};
template <typename Rule>
struct is_restoring : std::false_type {};
// Specialize to give rules names:
template <typename Rule>
struct rule_name;

/**
 * Macro marks a rule to be logged by our controller
 */
#define MARK_LOGGED(rule)                                                                          \
    template <>                                                                                    \
    struct is_logging<rule> : std::true_type {};                                                   \
    template <>                                                                                    \
    struct rule_name<rule> {                                                                       \
        static constexpr const char* const name = #rule;                                           \
    };                                                                                             \
    static_assert(true)

/**
 * Macro marks a rule to save and restore the node state on backtracking
 */
#define MARK_RESTORING(rule)                                                                       \
    template <>                                                                                    \
    struct is_restoring<rule> : std::true_type {};                                                 \
    static_assert(true)

/**
 * Our pegtl action type.
 */
template <typename Rule>
struct action : pegtl::nothing<Rule> {};

/**
 * Helper macro for defining actions for rules. Just types the boilerplate.
 */
#define ACTION(type)                                                                               \
    template <>                                                                                    \
    struct action<type> {                                                                          \
        template <typename In>                                                                     \
        static inline void apply(In&& in, parser_state& st);                                       \
    };                                                                                             \
    template <typename In>                                                                         \
    void action<type>::apply(In&& in[[maybe_unused]], parser_state& st[[maybe_unused]])

struct _dummy;  // <-- Just to fix some highlighting errors in VSCode

template <typename Rule>
struct fail_message;

#define SET_ERROR_MESSAGE(rule, message)                                                           \
    template <>                                                                                    \
    struct fail_message<rule> {                                                                    \
        static std::string string() { return #message; }                                           \
    };                                                                                             \
    static_assert(true)

/**
 * An action type used by our custom controller that lets rules perform
 * arbitrary actions when entering/existing their parse state. No distinction is
 * made between backtracking out of the rule and suceeeding the rule.
 */
template <typename Rule>
struct inout_action {
    template <typename... States>
    static void enter(States&&...) {}
    template <typename... States>
    static void exit(States&&...) {}
};

class det_parser_error : std::exception {
    pegtl::position _position;
    std::string     _message;

public:
    det_parser_error(pegtl::position pos, std::string message)
        : _position(pos)
        , _message(message) {}

    std::string            message() const { return _message; }
    const pegtl::position& position() const { return _position; }
};

/**
 * Parser state is kept while we parse over a document. It keeps track of
 * several things while running. Reach the docs on each member to find out more.
 */
class parser_state {
private:
    /**
     * The node stack. We parse akin to a push-down automata. We push down when
     * we parse important nodes, and pop and transform as we go.
     */
    std::stack<node> _nodes;

    /**
     * The restore stack. We use a custom controller with pegtl that will push
     * and pop as we traverse rules in the stack.
     *
     * When we enter a rule marked for save/restore (via MARK_RESTORING), we
     * push an integer that represents the size of the node stack (above) to
     * create a restore state.
     *
     * If the rules succeeds, we pop from the restore stack and discard the
     * save state.
     *
     * If the rule fails, we unwind the node list to the size specified at the
     * top of the restore stack and then pop the restore stack.
     */
    std::stack<std::size_t> _restore_points;

    /**
     * The unmatched count is used to keep proper associativity with `do`
     * blocks. When we begin parsing a function call, we increment the unmatched
     * level, and decrement it when we exit the function call parse.
     *
     * A do block will only be parsed if the unmatched state is equal to one,
     * which indicates that we are parsing a top-level function call.
     *
     * When we begin parsing a do-block, we push a new unmatched state down so
     * that calls within the do-block will be able to consume a nested to block.
     */
    std::vector<int> _unmatched_state = {0};

    /**
     * This is only used for logging parse actions, when we want to debug
     */
    int  _indent_level = 0;
    void _indent() {
        for (auto i = 0; i < _indent_level; ++i) {
            std::cerr.write("|   ", 4);
        }
    }

public:
    /**
     * Push a new node onto the stack
     */
    void push(node n) {
#if DEBUG_PARSER
        _indent();
        std::cerr << "push: " << n << '\n';
#endif
        _nodes.push(std::move(n));
    }

    /**
     * Pop the current node from the stack and return it
     */
    node pop() {
        assert(!_nodes.empty());
        auto ret = std::move(_nodes.top());
#if DEBUG_PARSER
        _indent();
        std::cerr << "pop: " << ret << '\n';
#endif
        _nodes.pop();
        return ret;
    }

    /**
     * Get the top node of the stack as a list node. This is used when we are
     * parsing function call arguments and we are appending expressions to be
     * passed as arguments to a function
     */
    const ast::list& arglist() {
        assert(!_nodes.empty());
        auto ret = _nodes.top().as_list();
        assert(!!ret && "Expected to find a list node for arguments");
        return *ret;
    }

    /**
     * Push a new argument to the current argument list
     */
    void push_to_list(node n) {
#if DEBUG_PARSER
        _indent();
        std::cerr << "Push argument: " << n << '\n';
#endif
        auto args      = pop();
        auto node_list = std::move(std::move(args).as_list()->nodes);
        node_list.push_back(std::move(n));
        push(ast::node(ast::list(std::move(node_list))));
        // arglist().nodes.push_back(std::move(n));
#if DEBUG_PARSER
        _indent();
        std::cerr << "Arguments acc: " << _nodes.top() << '\n';
#endif
    }

    /**
     * Check that we have parsed down to one exact expression node, and return
     * that node. The parser_state becomes unusable after this call
     */
    node finish() {
        assert(_nodes.size() > 0 && "Finished with no nodes (expected exactly one)");
        if (_nodes.size() != 1) {
            std::cerr << "FATAL!\n";
            std::cerr << "Parser stack contents: \n";
            auto count = _nodes.size();
            for (auto i = 0u; i < count; ++i) {
                std::cerr << "  [" << i << "] " << _nodes.top() << '\n';
                _nodes.pop();
            }
            assert(false && "Finished with more than one node in the parser stack");
            std::terminate();
        }
        return std::move(_nodes.top());
    }

    void push_restore() {
        // Save the current node stack size so that we might restore it in
        // case of failure
        _restore_points.push(_nodes.size());
    }

    void commit_restore() {
        // We commit by discarding our restore state
        _restore_points.pop();
    }

    void restore() {
        // Rewind any bad nodes we've pushed
        while (_nodes.size() > _restore_points.top()) {
            _nodes.pop();
        }
        assert(_nodes.size() == _restore_points.top() && "Bad state restore on parser rewind");
        _restore_points.pop();
    }

    /**
     * Logging functions for debugging the grammar
     */
    void log_enter(const char* name) {
        _indent();
        std::cerr << "[" << name << "] start\n";
        ++_indent_level;
    }

    void log_success(const char* name) {
        --_indent_level;
        _indent();
        std::cerr << "[" << name << "] success\n";
    }

    void log_fail(const char* name) {
        --_indent_level;
        _indent();
        std::cerr << "[" << name << "] fail\n";
    }

    /**
     * Control the level of unmatched function calls. Used for matching do-blocks.
     */
    void push_unmatched_state() { _unmatched_state.push_back(0); }

    void pop_unmatched_state() { _unmatched_state.pop_back(); }

    int& unmatched_state() {
        assert(!_unmatched_state.empty());
        return _unmatched_state.back();
    }
};

struct comment_no_nl : seq<one<'#'>, star<not_one<'\n'>>> {};

using hspace = seq<star<blank>, opt<comment_no_nl>>;

/**
 * Rule that consumes at least one horizontal space
 */
struct rhs {
    using analyze_t = pegtl::analysis::generic<pegtl::analysis::rule_type::ANY>;
    template <typename Input>
    static bool match(Input& in) {
        auto           ptr        = in.current();
        auto           to_check   = in.size();
        std::ptrdiff_t n_consumed = 0;
        while (to_check-- && std::isspace(*ptr) && *ptr != '\n') {
            ++n_consumed;
            ++ptr;
        }
        in.bump(n_consumed);
        return n_consumed != 0;
    }
};

/**
 * Rule that consumes as much whitespace as possible.
 */
struct ws {
    using analyze_t = pegtl::analysis::generic<pegtl::analysis::rule_type::OPT>;
    template <typename Input>
    static bool match(Input& in) {
        auto ptr        = in.current();
        auto to_check   = in.size();
        int  n_consumed = 0;
        while (to_check--) {
            if (std::isspace(*ptr)) {
                ++n_consumed;
                ++ptr;
            } else if (*ptr == '#') {
                ++n_consumed;
                ++ptr;
                while (to_check-- && *ptr != '\n') {
                    ++n_consumed;
                    ++ptr;
                }
                // Consume final newline:
                ++n_consumed;
                ++ptr;
            } else {
                break;
            }
        }
        in.bump(n_consumed);
        return true;
    }
};

/**
 * Rule that consumes as much whitespace as possible (at least one).
 */
struct rws {
    using analyze_t = pegtl::analysis::generic<pegtl::analysis::rule_type::ANY>;
    template <typename Input>
    static bool match(Input& in) {
        auto ptr        = in.current();
        auto to_check   = in.size();
        int  n_consumed = 0;
        while (to_check--) {
            if (std::isspace(*ptr)) {
                ++n_consumed;
                ++ptr;
            } else if (*ptr == '#') {
                ++n_consumed;
                ++ptr;
                while (to_check-- && *ptr != '\n') {
                    ++n_consumed;
                    ++ptr;
                }
                // Consume final newline:
                ++n_consumed;
                ++ptr;
            } else {
                break;
            }
        }
        in.bump(n_consumed);
        return n_consumed != 0;
    }
};

/**
 * A meta-rule that only matches when we are parsing a one-level function call.
 * (IE, we are a top-level function call, not a function call which is itself
 *  an argument to a function call)
 */
struct meta_in_l1_unmatched : success {};
template <>
struct action<meta_in_l1_unmatched> {
    template <typename Input>
    static bool apply(Input&&, parser_state& st) {
        return st.unmatched_state() == 1;
    }
};

// Forward-declare the single-expression type
struct single_ex;

/**
 * We define a very small set of keywords, a-la elixir
 */
#define DEFINE_KEYWORD(kw)                                                                         \
    struct kw_##kw : KW(#kw) {                                                                     \
        static constexpr const char* const spelling = #kw;                                         \
    };                                                                                             \
    static_assert(true)
DEFINE_KEYWORD(do);
DEFINE_KEYWORD(else);
DEFINE_KEYWORD(rescue);
DEFINE_KEYWORD(catch);
DEFINE_KEYWORD(end);
DEFINE_KEYWORD(fn);

SET_ERROR_MESSAGE(kw_end, "Expected keyword `end`");

using keyword = sor<kw_do, kw_else, kw_rescue, kw_catch, kw_end, kw_fn>;

/**
 * Reserved words work a little differently from keywords, but we have those too
 */
#define DEFINE_RESERVED(rw)                                                                        \
    struct rw_##rw : KW(#rw) {};                                                                   \
    static_assert(true)
DEFINE_RESERVED(fn);
DEFINE_RESERVED(true);
DEFINE_RESERVED(false);
DEFINE_RESERVED(and);
DEFINE_RESERVED(or);
DEFINE_RESERVED(not);
DEFINE_RESERVED(in);
DEFINE_RESERVED(when);

using reserved = sor<keyword, rw_fn, rw_true, rw_false, rw_and, rw_or, rw_not, rw_in, rw_when>;

/**
 * An "expression joiner" is a token that joins multiple expressions into a block
 *
 * In our case, these are the semicolon ";" and newline character
 */
struct expr_joiner : sor<eol, one<';'>> {};

/**
 * Identifiers are words beginning with a lowercase letter or underscore, and
 * container letters, numbers, or the "!" and "?" characters
 */
// TODO: We can do better than ASCII
struct cls_ident_start : sor<one<'_'>, utf8::ranges<'a', 'z'>> {};
struct cls_ident_char : sor<ascii::identifier_other, one<'!', '?'>> {};
struct ident_base : seq<cls_ident_start, star<cls_ident_char>> {};
struct identifier : seq<not_at<keyword>, ident_base, not_at<one<':'>>> {};

// Characters that may appear in symbols:
struct cls_sym_char : sor<utf8::ranges<'a', 'z', 'A', 'Z'>, one<'_', '-', '!', '?', '@'>> {};

// A tall symbol is a sequence of tall symobl elements joined by a dot "."
struct cls_tall_sym_elem : seq<utf8::range<'A', 'Z'>, star<cls_sym_char>> {};
struct lit_tall_sym : pegtl::list<cls_tall_sym_elem, one<'.'>> {};

// A small symbol is just a sequence of symbol chars prefixed by a colon
struct lit_small_sym : seq<one<':'>, plus<cls_sym_char>> {};

/**
 * We don't usually make distinction between tall symbols and small ones in the
 * grammar. They're often interchangable
 */
struct lit_symbol : sor<lit_tall_sym, lit_small_sym> {};

// Helper for parsing with digit separators
template <typename Digit>
struct cls_digit : sor<Digit, seq<one<'_'>, Digit>> {};
// Our two custom digit classes
struct odigit : utf8::range<'0', '7'> {};  // Octal digits
struct bdigit : one<'0', '1'> {};          // Binary digits

/**
 * Integer literals. Can have prefixes for different bases
 */
struct lit_dec_int : seq<digit, star<cls_digit<digit>>> {};
struct lit_hex_int : if_must<STR("0x"), plus<cls_digit<xdigit>>> {};
struct lit_oct_int : if_must<STR("0c"), plus<cls_digit<odigit>>> {};
struct lit_bin_int : if_must<STR("0b"), plus<cls_digit<bdigit>>> {};
struct lit_int : sor<lit_dec_int, lit_hex_int, lit_oct_int, lit_bin_int> {};
SET_ERROR_MESSAGE(plus<cls_digit<xdigit>>, "Expected hexidecimal digit sequence");
SET_ERROR_MESSAGE(plus<cls_digit<odigit>>, "Expected octal digit sequence");
SET_ERROR_MESSAGE(plus<cls_digit<bdigit>>, "Expected binary digit sequence");
SET_ERROR_MESSAGE(lit_int, "Expected integer literal");

/**
 * We parse floats simply by looking for a decimal number, a dot, then another decimal
 */
struct lit_float : seq<lit_dec_int, one<'.'>, plus<lit_dec_int>> {};

struct meta_prep_arglist : success {};
ACTION(meta_prep_arglist) {
    // Push a list that will be used to store arguments for the call
    st.push(ast::list{});
}

template <typename... Keywords>
struct keyword_list_args : seq<meta_prep_arglist, Keywords...> {};
template <typename... Keywords>
struct is_restoring<keyword_list_args<Keywords...>> : std::true_type {};
template <typename... Keywords>
struct is_logging<keyword_list_args<Keywords...>> : std::true_type {};
template <typename... Keywords>
struct rule_name<keyword_list_args<Keywords...>> {
    static constexpr const char* const name = "keyword_list_args";
};

template <typename... Keywords>
struct action<keyword_list_args<Keywords...>> {
    template <typename Input>
    static void apply(Input&&, parser_state& st) {
        auto pairlist = st.pop();
        assert(pairlist.as_list() && "Top should be a list");
        st.push_to_list(std::move(pairlist));
    }
};

struct keyword_arg_id : seq<ident_base> {};
ACTION(keyword_arg_id) { st.push(symbol(in.string())); }
struct keyword_arg : seq<keyword_arg_id, one<':'>, ws, single_ex> {};
MARK_RESTORING(keyword_arg);
MARK_LOGGED(keyword_arg);
ACTION(keyword_arg) {
    auto       value = st.pop();
    auto       key   = st.pop();
    ast::tuple tup;
    tup.nodes.push_back(std::move(key));
    tup.nodes.push_back(std::move(value));
    st.push_to_list(std::move(tup));
}

/**
 * A `lit_number' is just a float or an int
 */
struct lit_number : sor<lit_float, lit_int> {};

struct lit_tuple_elem : sor<single_ex> {};
ACTION(lit_tuple_elem) {
    auto el = st.pop();
    st.push_to_list(std::move(el));
}

struct lit_tuple_tail : sor<
                            // Try for a regular tuple element:
                            seq<lit_tuple_elem,
                                ws,
                                sor<
                                    // End of tuple:
                                    one<'}'>,
                                    // Comma:
                                    seq<one<','>, ws, lit_tuple_tail>>>,
                            // Or...
                            seq<
                                // We might have an embeded keyword list:
                                opt<keyword_list_args<list_tail<keyword_arg, one<','>, space>>>,
                                ws,
                                // Then the tuple close:
                                one<'}'>>> {};
SET_ERROR_MESSAGE(lit_tuple_tail, "Expected tuple element, keyword list, or closing \"}\"");

struct lit_tuple : if_must<seq<one<'{'>, ws, meta_prep_arglist>, lit_tuple_tail> {};
ACTION(lit_tuple) {
    auto nodes = st.pop();
    assert(nodes.as_list());
    auto& node_list = *nodes.as_list();
    if (node_list.nodes.size() != 3) {
        st.push(tuple(std::move(node_list.nodes)));
    } else {
        st.push(call(symbol("{}"), {}, std::move(node_list)));
    }
}
MARK_LOGGED(lit_tuple);

struct lit_list_elem : sor<single_ex> {};
ACTION(lit_list_elem) {
    auto elem = st.pop();
    st.push_to_list(std::move(elem));
}

struct lit_list_tail : sor<seq<sor<lit_list_elem, keyword_arg>,
                               ws,
                               sor<
                                   // End of list:
                                   one<']'>,
                                   // Comma:
                                   if_must<seq<one<','>, ws>, lit_list_tail>>>,
                           one<']'>> {};
struct lit_list : if_must<seq<one<'['>, ws, meta_prep_arglist>, lit_list_tail> {};
SET_ERROR_MESSAGE(lit_list_tail, "Expected list element, keyword pair, or closing ']'");

template <char Delim>
struct string_inner : star<sor<
                          // Escaped delimieter:
                          seq<one<'\\'>, one<Delim>>,
                          // Non-delimiter non-newline character:
                          not_one<'\n', '\r', Delim>>> {};
template <char Delim>
struct action<string_inner<Delim>> {
    template <typename Input>
    static void apply(Input&& in, parser_state& st) {
        auto&&      inner_str = in.string();
        auto        citer     = inner_str.begin();
        auto        cend      = inner_str.end();
        std::string acc;
        while (citer != cend) {
            if (*citer == '\\') {
                ++citer;
                if (citer == cend) {
                    throw det_parser_error{in.position(),
                                           "Expected escape character following '\\'"};
                }
                switch (*citer) {
                case Delim:
                    acc.push_back(Delim);
                    break;
                case '\\':
                    acc.push_back('\\');
                    break;
                case 'n':
                    acc.push_back('\n');
                    break;
                case 'r':
                    acc.push_back('\r');
                    break;
                case 'f':
                    acc.push_back('\f');
                    break;
                case 'b':
                    acc.push_back('\b');
                    break;
                default:
                    throw det_parser_error{in.position(),
                                           "Unknown escape sequence '\\" + std::string(&*citer, 1)
                                               + "'"};
                }
            } else {
                acc.push_back(*citer);
            }
            ++citer;
        }
        st.push(node(std::move(acc)));
    }
};
template <char Delim>
struct lit_string_end : one<Delim> {};
template <char Delim>
struct fail_message<lit_string_end<Delim>> {
    static std::string string() {
        char c = Delim;
        return "Expected " + std::string(&c, 1) + " at the end of string literal";
    }
};
template <char Delim>
struct lit_string_ : seq<one<Delim>, string_inner<Delim>, must<lit_string_end<Delim>>> {};

template <char Delim>
struct raw_string_inner : star<not_at<rep<3, one<Delim>>>, not_one<'\0'>> {};
template <char Delim>
struct action<raw_string_inner<Delim>> {
    template <typename Input>
    static void apply(Input&& in, parser_state& st) {
        auto                   inner_str  = in.string();
        auto                   min_indent = std::numeric_limits<int>::max();
        std::string::size_type old_nl_pos = 0;
        while (true) {
            const auto nl_pos = inner_str.find('\n', old_nl_pos);
            auto       nl_end = (std::min)(nl_pos, inner_str.size());
            auto       line = std::string_view(inner_str.data() + old_nl_pos, nl_end - old_nl_pos);
            auto       line_c_iter = line.begin();
            while (line_c_iter != line.end()) {
                if (!std::isspace(*line_c_iter)) {
                    break;
                }
                ++line_c_iter;
            }
            if (line_c_iter != line.end()) {
                // We found a non-whitespace character
                min_indent = (std::min)(min_indent,
                                        static_cast<int>(std::distance(line.begin(), line_c_iter)));
            }
            if (nl_pos == inner_str.npos) {
                break;
            }
            old_nl_pos = nl_end + 1;
        }
        // Now erase the min_indent chars from the front of each line
        old_nl_pos = 0;
        std::string acc;
        while (true) {
            const auto nl_pos = inner_str.find('\n', old_nl_pos);
            const auto nl_end = (std::min)(nl_pos, inner_str.size());
            auto       line = std::string_view(inner_str.data() + old_nl_pos, nl_end - old_nl_pos);
            auto       new_begin = (std::min)(min_indent, static_cast<int>(line.size()));
            acc += line.substr(new_begin);
            if (nl_pos == inner_str.npos) {
                break;
            }
            acc += "\n";
            old_nl_pos = nl_end + 1;
        }
        while (acc.size() && acc[0] == '\n') {
            acc.erase(acc.begin());
        }
        st.push(ast::node(std::move(acc)));
    }
};
template <char Delim>
struct raw_string_end : rep<3, one<Delim>> {};
template <char Delim>
struct fail_message<raw_string_end<Delim>> {
    static std::string string() { return "Expected raw-string literal terminator"; }
};
template <char Delim>
struct lit_raw_string
    : seq<raw_string_end<Delim>, raw_string_inner<Delim>, must<raw_string_end<Delim>>> {};

struct lit_string
    : sor<lit_raw_string<'\''>, lit_string_<'\''>, lit_raw_string<'"'>, lit_string_<'\"'>> {};
struct lit_special_atom : sor<KW("nil"), KW("true"), KW("false")> {};

struct ex_var : seq<identifier, not_at<hspace, sor<one<'('>, single_ex>>> {};
MARK_LOGGED(ex_var);
MARK_RESTORING(ex_var);
ACTION(ex_var) {
    auto var = st.pop();
    st.push(call(std::move(var), {}, let::symbol("Var")));
}

struct lit_minifun_arg : seq<one<'&'>, must<lit_int>> {};
ACTION(lit_minifun_arg) {
    auto      arg_n = st.pop();
    ast::list args({arg_n});
    st.push(call("&"_sym, {}, args));
}

// Forward-decl for block expressions
struct block_expr;
struct l2r_seq_expr;
struct l2r_seq_expr_exact;

/**
 * Helper for parsing keyword blocks. Applies the action before getting to
 * the tail.
 */
template <typename Keyword>
struct keyword_block_head : seq<Keyword, ws, must<l2r_seq_expr>> {};

/**
 * A keyword block is block prefixed by a keyword (do, else, finally, etc.)
 * followed by a block expression.
 */
template <typename Keyword>
struct keyword_block : seq<keyword_block_head<Keyword>, ws, must<struct keyword_block_tail>> {};

/**
 * other_keyword_block is a keyword block for any keyword except "do", since that
 * one is special
 */
using other_keyword_block = sor<keyword_block<kw_else>>;

/**
 * keyword_block_tail will slurp up trailing keyword blocks, or stop the keyword
 * block list by finding the `end` keyword
 */
struct keyword_block_tail : sor<other_keyword_block, kw_end> {};
SET_ERROR_MESSAGE(keyword_block_tail, "Expected continuation block or 'end' following block");

/**
 * We distinguish "do" blocks since they begin a sequence of keyword block
 */
struct do_block : keyword_block<kw_do> {};

struct ex_closure_callable : seq<ex_var, one<'.'>, at<one<'('>>> {};
MARK_RESTORING(ex_closure_callable);
struct ex_local_callable : seq<identifier> {};
struct ex_remote_callable : seq<lit_tall_sym, hspace, one<'.'>, ws, identifier> {};

// We have a distinct action for call arguments, since we want to push them into
// an argument list for the parsed function call
struct call_arg_expr : sor<single_ex> {};

// A do-block that will push a new keyword list
struct do_block_no_kws : seq<keyword_list_args<do_block>> {};

// clang-format off
struct no_paren_args_kw_tail :
    seq<
        // Require at least one keyword-arg to succeed
        keyword_arg,
        sor<
            // After that keyword-arg, check for some more
            seq<hspace, one<','>, ws, must<no_paren_args_kw_tail>>,
            // If we didn't find another keyword-arg, parse a do-block
            seq<meta_in_l1_unmatched, hspace, do_block>,
            // If we didn't parse anything else, we're done.
            success
        >
    >
{};
SET_ERROR_MESSAGE(no_paren_args_kw_tail, "Expected a keyword argument");

struct no_paren_args_tail :
    sor<
        // Check for positional arguments:
        seq<
            // Parse at least one argument
            call_arg_expr,
            // Decide what to do next, now that we have parsed an argument
            sor<
                // If we encounter a comma, recurse to another argument:
                // (Trailing commas are not allowed in no-paren calls)
                seq<hspace, one<','>, ws, must<no_paren_args_tail>>,
                // If we didn't, try to parse a do-block
                opt<meta_in_l1_unmatched, hspace, do_block_no_kws>
            >
        >,
        // If no more positionals, try keyword arguments:
        keyword_list_args<no_paren_args_kw_tail>,
        // If not keyword arguments, try for a do-block
        seq<meta_in_l1_unmatched, hspace, do_block_no_kws>
        // If none of the above matched, we're not a no-paren call
    >
{};
SET_ERROR_MESSAGE(no_paren_args_tail, "Expected positional argument or keyword argument");

/**
 * no_paren_args parses an list of unwrapped function call arguments. Unlike
 * paren_args, we require *at least* one argument, or we fail to parse
 */
struct no_paren_args :
    seq<
        rhs, // We *must* have at least one horizontal space
        no_paren_args_tail
    >
{};
MARK_LOGGED(no_paren_args);

/**
 * paren_args_kw_tail also recurses on itself, and accumulates keyword arguments
 * into the argument list of the function call
 */
struct paren_args_kw_tail :
    seq<
        // Check for a keyword argument. This rule only succeeds if we have at
        // leat one!
        keyword_arg,
        ws,
        sor<
            // Recursive case:
            seq<one<','>, ws, paren_args_kw_tail>,
            // If not a keyword argument, we must be at the end of the call arguments
            // We parse an optional trailing comma, then the closing paren, then
            // an optional do-block
            seq<opt<one<','>, ws>, one<')'>, opt<meta_in_l1_unmatched, hspace, do_block>>
        >
    >
{};

/**
 * paren_args_tail recurses over each argument in a paren block
 */
struct paren_args_tail :
    sor<
        seq<
            // Parse a regular positional argument
            call_arg_expr,
            ws,
            sor<
                // We either parse a trailing argument,
                seq<
                    one<','>,
                    ws,
                    must<paren_args_tail>
                >,
                // Or we parse the closing parenthesis
                seq<
                    one<')'>,
                    // Check for a do-block
                    opt<meta_in_l1_unmatched, hspace, do_block_no_kws>
                >
            >
        >,
        // If we encounter a keyword argument, switch to parsing a keyword
        // list exclusively. See above rule.
        keyword_list_args<paren_args_kw_tail>,
        seq<
            // Or we might just be the ending paren.
            one<')'>,
            // Again, check for a do-block
            opt<meta_in_l1_unmatched, hspace, do_block_no_kws>
        >
    >
{};
SET_ERROR_MESSAGE(paren_args_tail, "Expected positional argument, keyword argument, or closing ')'");

/**
 * This rule will parse a parenthesized list of function arguments, maybe with
 * a trailing do-block
 */
struct paren_args :
    seq<
        one<'('>,
        ws,
        must<paren_args_tail>
    >
{};
MARK_LOGGED(paren_args);

/**
 * A "base call" is a basic function call, where we are making a call directly
 * to a local function, or to a remote function
 */
struct ex_base_call :
    sor<
        // We can be a call to a closure
        seq<
            ex_closure_callable,
            meta_prep_arglist,
            paren_args
        >,
        // We can be a local function call (No module qualifier)
        seq<
            ex_local_callable,
            meta_prep_arglist,
            sor<
                // But we require either parenthesis,
                paren_args,
                // Or one function argument
                no_paren_args
                // If no arguments are present, we need to parse this
                // as a variable reference, not a function call
            >
        >,
        seq<
            ex_remote_callable,
            meta_prep_arglist,
            sor<
                paren_args,
                // A remote call can have no arguments
                opt<no_paren_args>
            >
        >
    >
{};
// clang-format on
template <>
struct inout_action<ex_base_call> {
    static void enter(parser_state& st) { st.unmatched_state()++; }
    static void exit(parser_state& st) { st.unmatched_state()--; }
};
MARK_LOGGED(ex_base_call);
MARK_RESTORING(ex_base_call);
ACTION(ex_base_call) {
    auto args = st.pop();
    auto fn   = st.pop();
    st.push(call(std::move(fn), {}, std::move(args)));
}

struct anon_fn_tail : seq<l2r_seq_expr_exact, ws, must<kw_end>> {};
SET_ERROR_MESSAGE(anon_fn_tail, "Expected anonymous function clauses");
struct anon_fn : seq<kw_fn, ws, must<anon_fn_tail>> {};
MARK_LOGGED(anon_fn);
ACTION(anon_fn) {
    auto clauses = st.pop();
    st.push(call(symbol("fn"), {}, std::move(clauses)));
}
template <>
struct inout_action<anon_fn> {
    static void enter(parser_state& st) { st.push_unmatched_state(); }
    static void exit(parser_state& st) { st.pop_unmatched_state(); }
};

/**
 * A parenthesis-wrapped expression. Anything can be inside!
 */
struct ex_parenthesis : seq<one<'('>, ws, must<l2r_seq_expr>, ws, must<one<')'>>> {};
SET_ERROR_MESSAGE(one<')'>, "Expected closing parenthesis");

/**
 * The basis expressions. No higher precedence than this!
 */
struct ex_atomic : sor<ex_parenthesis,
                       lit_special_atom,
                       lit_minifun_arg,
                       anon_fn,
                       ex_var,
                       lit_list,
                       lit_tuple,
                       lit_string,
                       lit_number,
                       lit_symbol> {};

template <typename Rule>
struct op_pad : seq<ws, Rule, ws> {};

// Associativity tags:
struct left {};
struct right {};

// Forward-decl. See definition for more info
template <typename Operator, typename Operand, typename Associativity, typename RHS = Operand>
struct ex_binary_operator;

namespace detail {

template <typename BinOp>
struct binop_tail;

template <typename Rule>
struct binop_operator_inner : seq<Rule> {};

template <typename Rule>
struct binop_tail_rhs : seq<Rule> {};

// How we define the two associativities:
template <typename Operator, typename Operand, typename RHS>
struct binop_tail<ex_binary_operator<Operator, Operand, left, RHS>>
    : if_must<op_pad<binop_operator_inner<Operator>>, binop_tail_rhs<RHS>> {};
// right:
template <typename Operator, typename Operand>
struct binop_tail<ex_binary_operator<Operator, Operand, right, Operand>>
    : if_must<op_pad<binop_operator_inner<Operator>>,
              ex_binary_operator<Operator, Operand, right, Operand>> {};

}  // namespace detail

template <typename Rule>
struct fail_message<detail::binop_tail_rhs<Rule>> {
    static std::string string() { return "Expected right-hand expression for binary operator"; }
};

template <typename Operator, typename Operand, typename Associativity, typename RHS>
struct fail_message<ex_binary_operator<Operator, Operand, Associativity, RHS>> {
    // XXX: We can make a better error messag than this, I think
    static std::string string() { return "Expected right-hand expression for binary operator"; }
};

// Action to push the operator name onto the stack
template <typename Rule>
struct action<detail::binop_operator_inner<Rule>> {
    template <typename In>
    static void apply(In&& in, parser_state& st) {
        // Push the spelling of this operator as a symbol onto the stack
        st.push(symbol(in.string()));
    }
};

// The action for when we save a binop to the expression stack
template <typename BinOp>
struct action<detail::binop_tail<BinOp>> {
    template <typename In>
    static void apply(In&&, parser_state& st) {
        // We've pushed two operands
        auto      rhs = st.pop();
        auto      op  = st.pop();
        auto      lhs = st.pop();
        ast::list args;
        args.nodes.push_back(std::move(lhs));
        args.nodes.push_back(std::move(rhs));
        st.push(call(std::move(op), {}, std::move(args)));
    }
};

/**
 * Helper template for defining binary operators
 *
 * The following parameters are excepted:
 *
 * @tparam Operator The actually rule that will match the operator in source.
 *      No action should associate with this rule. The spelling of the matched
 *      input will be used as the first item of the triple defining the operator
 *      call.
 * @tparam Operand The rule that matches the operand for this type. Used to
 *      express precedence. (The next more precedent expression should be used
 *      as the operand.).
 * @tparam Associativity. Either `right` or `left`.
 */
template <typename Operator, typename Operand, typename Associativity, typename RHS>
struct ex_binary_operator
    : seq<Operand,
          star<detail::binop_tail<ex_binary_operator<Operator, Operand, Associativity, RHS>>>> {};

struct ex_dot_access : seq<identifier> {};
SET_ERROR_MESSAGE(ex_dot_access, "Expected identifier following dot '.'");

struct ex_subscript_inner : sor<single_ex> {};
SET_ERROR_MESSAGE(ex_subscript_inner, "Expected expression for subscript operation");

struct ex_subscript_close : one<']'> {};
SET_ERROR_MESSAGE(ex_subscript_close, "Expected closing ']' following subscript expression");

struct ex_subscript_tail
    : seq<one<'['>, ws, must<ex_subscript_inner>, ws, must<ex_subscript_close>> {};

struct ex_dot_tail : seq<one<'.'>, ws, must<ex_dot_access>> {};
struct ex_call_tail : seq<meta_prep_arglist, sor<paren_args, no_paren_args>> {};
template <>
struct inout_action<ex_call_tail> {
    static void enter(parser_state& st) { st.unmatched_state()++; }
    static void exit(parser_state& st) { st.unmatched_state()--; }
};
// clang-format off
struct ex_base :
    sor<
        seq<
            sor<
                ex_base_call,
                ex_atomic
            >,
            opt<
                sor<
                    seq<hspace, ex_subscript_tail>,
                    seq<hspace, ex_dot_tail>
                >,
                star<
                    sor<
                        seq<hspace, ex_subscript_tail>,
                        seq<hspace, ex_dot_tail>,
                        seq<ex_call_tail>
                    >
                >
            >
        >,
        ex_base_call
    >
{};
// clang-format on

struct ex_unary_op
    : sor<seq<one<'+', '-'>, not_at<space>>, one<'^', '!'>, seq<STR("not"), at<space>>> {};
struct ex_unary_ : seq<ex_unary_op, ws, single_ex> {};
struct ex_unary : sor<ex_unary_, ex_base> {};

/**
 * Our operators.
 *
 * The helper template ex_binary_operator helps us define operators with their
 * own associativity.
 */
struct ex_product : ex_binary_operator<one<'*', '/'>, ex_unary, left> {};
struct ex_sum
    : ex_binary_operator<sor<seq<one<'-'>, not_at<one<'-', '>'>>>, seq<one<'+'>, not_at<one<'+'>>>>,
                         ex_product,
                         left> {};
struct ex_concat
    : ex_binary_operator<sor<STR("++"), STR("--"), STR(".."), STR("<>")>, ex_sum, right> {};
struct ex_pipe : ex_binary_operator<STR("|>"), ex_concat, left> {};
struct ex_compare_op : sor<STR("=="), STR("!="), STR("=~")> {};
struct ex_compare : ex_binary_operator<ex_compare_op, ex_pipe, left> {};
struct ex_minifun_tail : seq<ex_compare> {};
struct ex_minifun_ : seq<one<'&'>, not_at<lit_int>, ws, must<ex_minifun_tail>> {};
struct ex_minifun : sor<ex_minifun_, ex_compare> {};
struct ex_assign : ex_binary_operator<STR("="), ex_minifun, right> {};
struct ex_binor : ex_binary_operator<STR("|"), ex_assign, right> {};
struct ex_typespec : ex_binary_operator<STR("::"), ex_binor, right> {};
struct ex_left_arrow : ex_binary_operator<STR("<-"), ex_typespec, right> {};

SET_ERROR_MESSAGE(ex_minifun_tail, "Expected expression for &-style function");

ACTION(ex_minifun_) {
    auto fn = st.pop();
    ast::list args({fn});
    st.push(ast::call("&"_sym, {}, args));
}

/**
 * Any single expression (Not a block expression)
 */
struct single_ex : seq<ex_left_arrow> {};
MARK_LOGGED(single_ex);
MARK_RESTORING(single_ex);

struct l2r_seq_element_head;

/**
 * XXX: Because the -> operator has lower precedence than block joiners ";" or newline,
 * we need to do a lookahead in the block parsing to check that we aren't about to parse
 * an arrow-expression, which indicates that we are starting another -> clause.
 *
 * THIS IS TERRIBLY INEFFICIENT. It requires a huge amount of lookahead. We do a
 * negative lookahead for an `l2r_seq_element_head`, which can parse an arbirary
 * number of single expressions. We throw that whole parse away and restart in
 * the l2r_seq_element rule, just to parse it again.
 *
 * We can do it better.
 */

/**
 * A block expression is a list of expressions separated by newlines or semicolons
 */
struct block_expr_tail : seq<hspace, expr_joiner, ws, not_at<l2r_seq_element_head>, single_ex> {};
MARK_LOGGED(block_expr_tail);
ACTION(block_expr_tail) {
    auto expr = st.pop();
    st.push_to_list(std::move(expr));
}

struct block_expr_tail_first : seq<expr_joiner, ws, not_at<l2r_seq_element_head>, single_ex> {};
MARK_LOGGED(block_expr_tail_first);
ACTION(block_expr_tail_first) {
    // We're the second expression in a block. We'll conver the prior expression
    // into a block expression to prepare for any subsequent block elements
    auto second = st.pop();
    auto first  = st.pop();
    st.push(ast::list{});
    st.push_to_list(std::move(first));
    st.push_to_list(std::move(second));
}

/**
 * This meta-rule just turns a list of expressions into a block expression
 */
struct block_expr_commit : seq<success> {};
ACTION(block_expr_commit) {
    // Convert our expression list into a call to the "__block__" operator
    auto exprs = st.pop();
    st.push(call(symbol("__block__"), {}, std::move(exprs)));
}

/**
 * We assume that any whitespace has already been stripped
 */
struct block_expr
    : seq<single_ex, hspace, opt<block_expr_tail_first, star<block_expr_tail>, block_expr_commit>> {
};
MARK_LOGGED(block_expr);

/**
 * An left-to-right arrow sequence is something quite special
 */
struct l2r_lhs_expr_one : seq<single_ex> {};
ACTION(l2r_lhs_expr_one) {
    // Push the lhs element to the lhs argument list
    auto expr = st.pop();
    st.push_to_list(std::move(expr));
}
SET_ERROR_MESSAGE(l2r_lhs_expr_one, "Expected single expression following comma");
struct l2r_seq_element_head
    : seq<opt<list_must<l2r_lhs_expr_one, one<','>, space>>, ws, STR("->")> {};
struct l2r_seq_element : seq<meta_prep_arglist, l2r_seq_element_head, ws, must<block_expr>> {};
MARK_RESTORING(l2r_seq_element);
MARK_LOGGED(l2r_seq_element);
MARK_LOGGED(l2r_seq_element_head);
ACTION(l2r_seq_element) {
    auto block = st.pop();
    auto lhs   = st.pop();
    auto args  = ast::list();
    args.nodes.push_back(std::move(lhs));
    args.nodes.push_back(std::move(block));
    auto tup = call(symbol("->"), {}, std::move(args));
    st.push_to_list(std::move(tup));
}
// MARK_RESTORING(l2r_seq_expr_);

struct l2r_seq_expr_exact : seq<meta_prep_arglist, plus<l2r_seq_element, ws>> {};
MARK_RESTORING(l2r_seq_expr_exact);
MARK_LOGGED(l2r_seq_expr_exact);
struct l2r_seq_expr : sor<l2r_seq_expr_exact, block_expr> {};
SET_ERROR_MESSAGE(l2r_seq_expr, "Expected expression or sequence thereof");

/**
 * These are the rules that will save/restore our node stack state in case
 * of backtracking
 */
MARK_RESTORING(lit_symbol);
MARK_RESTORING(ex_call_tail);
MARK_RESTORING(ex_base);

/**
 * Our marked actions are actions which generate a log message when we parse
 * them, purely for debugging purposes
 */
MARK_LOGGED(expr_joiner);
MARK_LOGGED(lit_symbol);
MARK_LOGGED(do_block);
MARK_LOGGED(ex_local_callable);
MARK_LOGGED(ex_remote_callable);
MARK_LOGGED(call_arg_expr);
MARK_LOGGED(ex_base);
MARK_LOGGED(meta_in_l1_unmatched);
MARK_LOGGED(rhs);

/**
 * These are our parser actions. They manipulate the node stack to produce the
 * final main expression.
 */
// "identifier" pushes the spelled identifier onto the stack as a symbol
ACTION(identifier) { st.push(symbol(in.string())); }
ACTION(lit_tall_sym) { st.push(symbol(in.string())); }
ACTION(lit_small_sym) { st.push(symbol(in.string().substr(1))); }
ACTION(lit_special_atom) { st.push(symbol(in.string())); }
// TODO: Remove digit separators from strings:
ACTION(lit_int) {
    try {
        st.push(node(integer(std::stoll(in.string()))));
    } catch (std::out_of_range) {
        throw det_parser_error(in.position(), "Integer value is too large");
    }
}
ACTION(lit_float) { st.push(node(floating(std::stod(in.string())))); }
// Action for keyword blocks. This will append the keyword argument for the
// block to the current keyword list at the top of the stack.
template <typename Keyword>
struct action<keyword_block_head<Keyword>> {
    template <typename In>
    static void apply(In&&, parser_state& st) {
        auto       block = st.pop();
        ast::tuple tup;
        tup.nodes.push_back(symbol(Keyword::spelling));
        tup.nodes.push_back(std::move(block));
        st.push_to_list(std::move(tup));
    }
};
// Entering/exitting a do block presents a new unmatched call counter
template <>
struct inout_action<do_block> {
    static void enter(parser_state& st) { st.push_unmatched_state(); }
    static void exit(parser_state& st) { st.pop_unmatched_state(); }
};
ACTION(ex_closure_callable) {
    auto      var = st.pop();
    ast::list args;
    args.nodes.push_back(std::move(var));
    st.push(call(symbol("."), {}, std::move(args)));
}
ACTION(ex_remote_callable) {
    // Put in an operator .
    auto      fn     = st.pop();
    auto      module = st.pop();
    ast::list args;
    args.nodes.push_back(std::move(module));
    args.nodes.push_back(std::move(fn));
    st.push(call(symbol("."), {}, std::move(args)));
}
ACTION(call_arg_expr) {
    // Take the current top expression and push it onto the argument list
    // underneath it in the stack
    auto arg = st.pop();
    st.push_to_list(std::move(arg));
}

ACTION(ex_subscript_tail) {
    auto      inner = st.pop();
    auto      left  = st.pop();
    ast::list args;
    args.nodes.push_back(std::move(left));
    args.nodes.push_back(std::move(inner));
    st.push(call(symbol("[]"), {}, std::move(args)));
}
ACTION(ex_dot_access) {
    auto      right = st.pop();
    auto      left  = st.pop();
    ast::list args;
    args.nodes.push_back(std::move(left));
    args.nodes.push_back(std::move(right));
    st.push(call(symbol("."), {}, std::move(args)));
}
ACTION(ex_call_tail) {
    auto args = st.pop();
    auto left = st.pop();
    st.push(call(std::move(left), {}, std::move(args)));
}

ACTION(ex_unary_op) { st.push(symbol(in.string())); }
ACTION(ex_unary_) {
    auto      operand = st.pop();
    auto      op      = st.pop();
    ast::list args;
    args.nodes.push_back(std::move(operand));
    st.push(call(std::move(op), {}, std::move(args)));
}

/**
 * The root document definition
 */
struct let_doc : seq<ws, must<block_expr>, ws, must<eof>> {};
MARK_LOGGED(let_doc);

SET_ERROR_MESSAGE(block_expr, "Expected one or more expressions.");
SET_ERROR_MESSAGE(eof, "Expected end-of-file");

template <typename Rule>
struct let_pegtl_controller : pegtl::normal<Rule> {
    template <typename Input>
    static void start(const Input&, parser_state& st) {
#if DEBUG_PARSER
        log_start(is_logging<Rule>(), st);
#endif
        inout_action<Rule>::enter(st);
        if constexpr (is_restoring<Rule>()) {
            st.push_restore();
        }
    }

    static void do_restore(std::false_type, parser_state&) {}
    static void do_restore(std::true_type, parser_state& st) { st.restore(); }

    template <typename Input>
    static void success(const Input&, parser_state& st) {
#if DEBUG_PARSER
        log_success(is_logging<Rule>(), st);
#endif
        if constexpr (is_restoring<Rule>()) {
            st.commit_restore();
        }
        inout_action<Rule>::exit(st);
    }

    template <typename Input>
    static void failure(const Input&, parser_state& st) {
#if DEBUG_PARSER
        log_fail(is_logging<Rule>(), st);
#endif
        if constexpr (is_restoring<Rule>()) {
            st.restore();
        }
        inout_action<Rule>::exit(st);
    }

    static void log_start(std::false_type, parser_state&) {}
    static void log_start(std::true_type, parser_state& st) { st.log_enter(rule_name<Rule>::name); }

    static void log_success(std::false_type, parser_state&) {}
    static void log_success(std::true_type, parser_state& st) {
        st.log_success(rule_name<Rule>::name);
    }

    static void log_fail(std::false_type, parser_state&) {}
    static void log_fail(std::true_type, parser_state& st) { st.log_fail(rule_name<Rule>::name); }

    template <typename Input>
    static void raise(const Input& in, parser_state&) {
        throw det_parser_error(in.position(), fail_message<Rule>::string());
    }
};

}  // namespace

node let::ast::parse(std::string_view::iterator first, std::string_view::iterator last) {
    // return node(integer(pegtl::analyze<let_doc>()));
    auto                str = std::string(first, last);
    pegtl::memory_input in{str, str};
    parser_state        st;
    try {
        pegtl::parse<let_doc, ::action, let_pegtl_controller>(in, st);
        return st.finish();
    } catch (const det_parser_error& err) {
        // Generate a nice error message for the people
        auto& pos = err.position();
        // Get a pointer to where the error happened:
        auto dataptr = str.begin() + pos.byte;
        // Find the line in the source string
        auto line_start = dataptr;
        auto line_end   = dataptr;
        if (line_start != str.begin() && (line_start == str.end() || *line_start == '\n'))
            --line_start;
        // Walk until we find a newline
        while (line_start != str.begin() && *line_start != '\n')
            --line_start;
        if (line_start != str.end() && *line_start == '\n')
            ++line_start;
        while (line_end != str.end() && *line_end != '\r' && *line_end != '\n')
            ++line_end;
        const auto full_line = std::string(line_start, line_end);
        assert(full_line.size() >= pos.byte_in_line);
        throw ast::parse_error(err.message(), pos.line, pos.byte_in_line, full_line);
    }
}
