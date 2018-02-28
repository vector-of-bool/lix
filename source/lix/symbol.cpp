#include "symbol.hpp"

#include <cinttypes>
#include <mutex>
#include <set>
#include <string>

namespace lix::detail {

class symtab_entry_ptr {};

}  // namespace lix::detail

namespace {

class symbol_table {
    mutable std::set<std::string, std::less<>> _symbols;
    std::mutex                                 _lock;

public:
    const std::string* get(const std::string_view& spelling) {
        std::lock_guard    lk{_lock};
        auto               iter = _symbols.find(spelling);
        if (iter != _symbols.end()) {
            return &*iter;
        } else {
            iter = _symbols.emplace(spelling).first;
            return &*iter;
        }
    }
};

}  // namespace

const std::string* lix::detail::get_intern_symbol_string(const std::string_view& str) {
    static symbol_table tab;
    return tab.get(str);
}