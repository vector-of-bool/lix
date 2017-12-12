#include "parse.hpp"

using str_iter = std::string_view::iterator;

namespace {

using namespace let::ast;

node parse_statement_seq(str_iter begin, const str_iter end) { return {}; }

}  // namespace

let::ast::node let::parse_string(std::string_view str) { return parse_statement_seq(str.begin(), str.end()); }