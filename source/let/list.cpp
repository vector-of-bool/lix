#include "list.hpp"

#include <let/value.hpp>

let::list::list(std::vector<let::value> els)
    : _elements(std::move(els)) {}

std::size_t let::list::size() const noexcept { return _elements.size(); }
void let::list::push(let::value b) { _elements.push_back(std::move(b)); }

void let::list::pop() { _elements.pop_back(); }

void let::list::push_front(let::value b) { _elements.insert(_elements.begin(), b); }

void let::list::pop_front() { _elements.erase(_elements.begin()); }

std::ostream& let::operator<<(std::ostream& o, const list& l) {
    o << "[";
    auto iter = l.begin();
    while (iter != l.end()) {
        o << *iter;
        ++iter;
        if (iter != l.end()) {
            o << ", ";
        }
    }
    o << "]";
    return o;
}