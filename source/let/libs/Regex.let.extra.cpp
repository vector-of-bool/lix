#include <let/raise.hpp>
#include <let/util/args.hpp>
#include <let/util/wrap_fn.hpp>

#include <regex>
#include <string>

using namespace let::literals;

LET_BASIC_TYPEINFO(std::regex);

namespace {

let::exec::module build_regex_basemod() {
    let::exec::module mod;
    mod.add_function("compile", let::wrap_function([](const std::string& pattern) -> let::tuple {
                         try {
                             std::regex ret_re{pattern};
                             return let::tuple::make("ok"_sym, let::boxed(std::move(ret_re)));
                         } catch (const std::regex_error& e) {
                             return let::tuple::make("error"_sym, e.what());
                         }
                     }));
    mod.add_function("is_regex?", let::wrap_function([](const let::value& v) {
                         auto box = v.as_boxed();
                         if (!box) {
                             return "false"_sym;
                         } else {
                             auto as_re = let::box_cast<std::regex>(&*box);
                             if (as_re == nullptr) {
                                 return "false"_sym;
                             } else {
                                 return "true"_sym;
                             }
                         }
                     }));
    mod.add_function("run",
                     let::wrap_function(
                         [](const std::regex& re, const std::string& string) -> let::value {
                             std::smatch match;
                             auto        did_match = std::regex_search(string, match, re);
                             if (!did_match) {
                                 return "nil"_sym;
                             }
                             return let::list(match.begin(), match.end());
                         }));
    return mod;
}

let::exec::module& regex_basemod() {
    static auto mod = build_regex_basemod();
    return mod;
}

void do_extra(let::exec::context& ctx) { ctx.register_module("__regex", regex_basemod()); }

}  // namespace