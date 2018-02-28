#include "node.hpp"

#include <lix/value.hpp>

#include <cassert>
#include <sstream>

using namespace lix;
using namespace lix::ast;

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
    void operator()(real f) const { o << f; }
    void operator()(const symbol& s) const { o << s; }
    void operator()(const string& s) const { o << '\'' << s << '\''; }
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

value meta_to_value(const meta& m) {
    auto fn_dets = m.fn_details();
    auto first
        = fn_dets ? value(lix::tuple::make(fn_dets->first, fn_dets->second)) : value("nil"_sym);
    return lix::tuple::make(first, m.line(), m.column());
}

ast::meta meta_from_value(const lix::value& v) {
    ast::meta ret;
    auto      tup = v.as_tuple();
    if (!tup || tup->size() != 3) {
        return ret;
    }
    auto first = (*tup)[0];
    auto pair  = first.as_tuple();
    if (pair && pair->size() == 2) {
        auto mod = (*pair)[0];
        auto fn  = (*pair)[1];
        if (mod.as_symbol() && fn.as_symbol()) {
            ret.set_fn_details(mod.as_symbol()->string(), fn.as_symbol()->string());
        }
    }
    auto line = (*tup)[1].as_integer();
    if (line) {
        ret.set_line(*line);
    }
    auto col = (*tup)[2].as_integer();
    if (col) {
        ret.set_column(*col);
    }
    return ret;
}

using lix::value;
struct to_value_converter {
    value operator()(const ast::list& l) {
        std::vector<value> vals;
        for (auto& n : l.nodes) {
            vals.emplace_back(n.to_value());
        }
        auto move_begin = std::make_move_iterator(vals.begin());
        auto move_end   = std::make_move_iterator(vals.end());
        return lix::list(move_begin, move_end);
    }
    value operator()(const ast::tuple& t) {
        std::vector<value> vals;
        for (auto& n : t.nodes) {
            vals.emplace_back(n.to_value());
        }
        return lix::tuple(std::move(vals));
    }
    value operator()(ast::integer i) { return i; }
    value operator()(ast::real f) { return f; }
    value operator()(const ast::symbol& s) { return s; }
    value operator()(const ast::string& s) { return s; }
    value operator()(const ast::call& c) {
        std::vector<value> triple;
        triple.emplace_back(c.target().to_value());
        triple.emplace_back(meta_to_value(c.meta()));
        triple.emplace_back(c.arguments().to_value());
        return lix::tuple(std::move(triple));
    }
};

struct from_value_converter {
    node operator()(const lix::list& l) {
        std::vector<node> nodes;
        for (auto& v : l) {
            nodes.emplace_back(node::from_value(v));
        }
        return node(ast::list(std::move(nodes)));
    }
    node operator()(const lix::map&) {
        assert(false && "TODO");
        std::terminate();
    }
    node operator()(const lix::tuple& tup) {
        if (tup.size() == 3) {
            // It's a call
            auto target = node::from_value(tup[0]);
            auto meta   = meta_from_value(tup[1]);
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
    node operator()(lix::integer i) { return node(i); }
    node operator()(lix::real r) { return node(r); }
    node operator()(const lix::symbol& s) { return node(s); }
    node operator()(const lix::string& s) { return node(s); }
    node operator()(const lix::exec::function&) {
        assert(false && "Cannot use function for AST node");
        std::terminate();
    }
    node operator()(const lix::exec::closure&) {
        assert(false && "Cannot use function for AST node");
        std::terminate();
    }
    node operator()(const lix::boxed&) {
        assert(false && "Cannot use boxed value for AST node");
        std::terminate();
    }
    node operator()(lix::exec::detail::binding_slot) {
        assert(false && "Cannot use binding slot for AST node");
        std::terminate();
    }
    node operator()(lix::exec::detail::cons) {
        assert(false && "Cannot use cons for AST node");
        std::terminate();
    }
};

}  // namespace

lix::value node::to_value() const { return visit(to_value_converter{}); }
node       node::from_value(const lix::value& val) { return val.visit(from_value_converter{}); }