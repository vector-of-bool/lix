#include <lix/raise.hpp>
#include <lix/util/args.hpp>
#include <lix/util/wrap_fn.hpp>

#include <regex>
#include <string>

using namespace lix::literals;

LIX_BASIC_TYPEINFO(std::regex);

namespace {

lix::exec::module build_regex_basemod() {
    lix::exec::module mod;
    mod.add_function("compile", lix::wrap_function([](const std::string& pattern) -> lix::tuple {
                         try {
                             std::regex ret_re{pattern};
                             return lix::tuple::make("ok"_sym, lix::boxed(std::move(ret_re)));
                         } catch (const std::regex_error& e) {
                             return lix::tuple::make("error"_sym, e.what());
                         }
                     }));
    mod.add_function("is_regex?", lix::wrap_function([](const lix::value& v) {
                         auto box = v.as_boxed();
                         if (!box) {
                             return "false"_sym;
                         } else {
                             auto as_re = lix::box_cast<std::regex>(&*box);
                             if (as_re == nullptr) {
                                 return "false"_sym;
                             } else {
                                 return "true"_sym;
                             }
                         }
                     }));
    mod.add_function("run",
                     lix::wrap_function(
                         [](const std::regex& re, const std::string& string) -> lix::value {
                             std::smatch match;
                             auto        did_match = std::regex_search(string, match, re);
                             if (!did_match) {
                                 return "nil"_sym;
                             }
                             return lix::list(match.begin(), match.end());
                         }));
    return mod;
}

lix::exec::module& regex_basemod() {
    static auto mod = build_regex_basemod();
    return mod;
}

void do_extra(lix::exec::context& ctx) { ctx.register_module("__regex", regex_basemod()); }

}  // namespace