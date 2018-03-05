#include <lix/raise.hpp>
#include <lix/util/args.hpp>
#include <lix/util/wrap_fn.hpp>

#include <string>

using namespace lix::literals;

namespace {

lix::exec::module build_string_basemod() {
    lix::exec::module mod;
    mod.add_function(
        "find_pattern",
        lix::wrap_function([](const lix::string& str, const lix::value& pattern) -> lix::value {
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
                        lix::raise(lix::tuple::make("einval"_sym,
                                                    "Elements of pattern list must be strings"));
                    }
                }
                return "nil"_sym;
            } else {
                lix::raise(
                    lix::tuple::make("einval"_sym, "Invalid pattern for String.contains?()"));
            }
        }));
    mod.add_function("split",
                     lix::wrap_function(
                         [](const lix::string& str, const lix::string& pattern) -> lix::list {
                             std::vector<lix::string> parts;
                             lix::string::size_type   new_end  = 0;
                             lix::string::size_type   last_end = 0;
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
                             auto mov_last  = std::make_move_iterator(parts.end());
                             return lix::list(mov_first, mov_last);
                         }));
    mod.add_function("replace",
                     lix::wrap_function([](lix::string        subject,
                                           const lix::string& pattern,
                                           const lix::string& repl) -> lix::string {
                         lix::string::size_type pos         = 0;
                         const auto             pattern_len = pattern.size();
                         const auto             repl_len    = repl.size();
                         while ((pos = subject.find(pattern, pos)) != subject.npos) {
                             subject = subject.replace(pos, pattern_len, repl);
                             pos += repl_len;
                         }
                         return subject;
                     }));
    return mod;
}

lix::exec::module& string_basemod() {
    static auto mod = build_string_basemod();
    return mod;
}

void do_extra(lix::exec::context& ctx) { ctx.register_module("__string", string_basemod()); }

}  // namespace