#include "node.hpp"

#include <sstream>

using namespace let;
using namespace let::ast;

namespace {

struct ostream_node_visitor {
    std::ostream& o;

    void operator()(const list& l) const {
        o << '[';
        auto n = l.nodes.size();
        auto iter = l.nodes.begin();
        while (n--) {
            o << *iter;
            if (n > 0) {
                o << ", ";
            }
            ++iter;
        }
        o << ']';
    }

    void operator()(const tuple& t) const {
        o << '{';
        auto n = t.nodes.size();
        auto iter = t.nodes.begin();
        while (n--) {
            o << *iter;
            if (n > 0) {
                o << ", ";
            }
            ++iter;
        }
        o << '}';
    }
    void operator()(integer i) const {
        o << i;
    }
    void operator()(floating f) const {
        o << f;
    }
    void operator()(const symbol& s) const {
        o << s;
    }
    void operator()(const call& c) const {
        o << '{' << c.target() << ", [], " << c.arguments() << '}';
    }
};

}  // namespace

std::ostream& ast::operator<<(std::ostream& o, const node& n) {
    n.visit(ostream_node_visitor{o});
    return o;
}

std::string ast::to_string(const node& n) {
    std::stringstream strm;
    strm << n;
    return strm.str();
}