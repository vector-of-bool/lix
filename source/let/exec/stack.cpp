#include "stack.hpp"

#include <let/exec/context.hpp>

using namespace let;
using namespace let::exec;

let::tuple exec::convert_ex(const ex_tuple& tup) {
    std::vector<let::value> values;
    for (auto& el : tup.elements) {
        values.push_back(el.convert_to_value());
    }
    return let::tuple(std::move(values));
}
let::list exec::convert_ex(const ex_list& list) {
    std::vector<let::value> values;
    for (auto& el : list.elements) {
        values.push_back(el.convert_to_value());
    }
    return let::list(std::move(values));
}

namespace {

struct value_convert_visitor {
    value operator()(const value& val) { return val; }
    value operator()(const ex_tuple& tup) { return convert_ex(tup); }
    value operator()(const ex_list& list) { return convert_ex(list); }
    value operator()(exec::binding_slot) {
        throw std::runtime_error{"Cannot convert binding slot to value"};
    }
    value operator()(const exec::closure& cl) {
        return cl;
    }
};

}  // namespace

value stack_element::convert_to_value() const { return visit(value_convert_visitor{}); }