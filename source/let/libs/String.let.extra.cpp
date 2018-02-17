#include <let/raise.hpp>
#include <let/util/args.hpp>
#include <let/util/wrap_fn.hpp>

#include <string>

using namespace let::literals;

namespace {

let::exec::module build_string_basemod() {
    let::exec::module mod;
    mod.add_function(
        "find_pattern",
        let::wrap_function([](const let::string& str, const let::value& pattern) -> let::value {
            if (auto pat_str = pattern.as_string()) {
                auto pos = str.find(*pat_str);
                if (pos != str.npos) {
                    return pos;
                } else {
                    return "nil"_sym;
                }
            } else if (auto pat_list = pattern.as_list()) {
                for (auto& el : *pat_list) {
                    if (auto el_str = el.as_string()) {
                        auto pos = str.find(*el_str);
                        if (pos != str.npos) {
                            return pos;
                        }
                    } else {
                        let::raise(let::tuple::make("einval"_sym,
                                                    "Elements of pattern list must be strings"));
                    }
                }
                return "nil"_sym;
            } else {
                let::raise(
                    let::tuple::make("einval"_sym, "Invalid pattern for String.contains?()"));
            }
        }));
    mod.add_function("split",
                     let::wrap_function(
                         [](const let::string& str, const let::string& pattern) -> let::list {
                             std::vector<let::string> parts;
                             let::string::size_type   new_end = 0;
                             let::string::size_type   last_end = 0;
                             while (true) {
                                 new_end = str.find(pattern, last_end);
                                 new_end = (std::min)(new_end, str.size());
                                 parts.emplace_back(str.data() + last_end, new_end - last_end);
                                 last_end = new_end + pattern.size();
                                 if (new_end == str.size()) {
                                     // We've found them all
                                     break;
                                 }
                             }
                             auto mov_first = std::make_move_iterator(parts.begin());
                             auto mov_last = std::make_move_iterator(parts.end());
                             return let::list(mov_first, mov_last);
                         }));
    return mod;
}

let::exec::module& string_basemod() {
    static auto mod = build_string_basemod();
    return mod;
}

void do_extra(let::exec::context& ctx) { ctx.register_module("__string", string_basemod()); }

}  // namespace