#include "node.hpp"

#include <let/value.hpp>

#include <cassert>
#include <sstream>

using namespace let;
using namespace let::ast;

namespace {

struct ostream_node_visitor {
    std::ostream& o;

    void operator()(const ast::list& l) const {
        o << '[';
        auto n    = l.nodes.size();
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

    void operator()(const ast::tuple& t) const {
        o << '{';
        auto n    = t.nodes.size();
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
    void operator()(integer i) const { o << i; }
    void operator()(floating f) const { o << f; }
    void operator()(const symbol& s) const { o << s; }
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

namespace {

using let::value;
struct to_value_converter {
    value operator()(const ast::list& l) {
        std::vector<value> vals;
        for (auto& n : l.nodes) {
            vals.emplace_back(n.to_value());
        }
        return let::list(std::move(vals));
    }
    value operator()(const ast::tuple& t) {
        std::vector<value> vals;
        for (auto& n : t.nodes) {
            vals.emplace_back(n.to_value());
        }
        return let::tuple(std::move(vals));
    }
    value operator()(ast::integer i) { return i; }
    value operator()(ast::floating f) { return f; }
    value operator()(ast::symbol s) { return s; }
    value operator()(const ast::call& c) {
        std::vector<value> triple;
        triple.emplace_back(c.target().to_value());
        triple.emplace_back(let::list({}));
        triple.emplace_back(c.arguments().to_value());
        return let::tuple(std::move(triple));
    }
};

struct from_value_converter {
    node operator()(const let::list& l) {
        std::vector<node> nodes;
        for (auto& v : l) {
            nodes.emplace_back(node::from_value(v));
        }
        return node(ast::list(std::move(nodes)));
    }
    node operator()(const let::tuple& tup) {
        if (tup.size() == 3) {
            // It's a call
            auto target = node::from_value(tup[0]);
            auto meta   = ast::meta{};  // TODO
            auto args   = node::from_value(tup[2]);
            return node(ast::call(std::move(target), meta, std::move(args)));
        } else {
            std::vector<node> nodes;
            for (auto i = 0u; i < tup.size(); ++i) {
                nodes.push_back(node::from_value(tup[i]));
            }
            return node(ast::tuple(std::move(nodes)));
        }
    }
    node operator()(let::integer i) { return node(i); }
    node operator()(let::real r) { return node(r); }
    node operator()(const let::symbol& s) { return node(s); }

    node operator()(let::string) {
        /// TODO
        std::terminate();
    }
    node operator()(const let::exec::function&) {
        assert(false && "Cannot use function for AST node");
        std::terminate();
    }
    node operator()(const let::exec::closure&) {
        assert(false && "Cannot use function for AST node");
        std::terminate();
    }
    node operator()(const let::boxed&) {
        assert(false && "Cannot use boxed value for AST node");
        std::terminate();
    }
};

}  // namespace

let::value node::to_value() const { return visit(to_value_converter{}); }
node node::from_value(const let::value& val) { return val.visit(from_value_converter{}); }