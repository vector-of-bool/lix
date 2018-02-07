#include <cstdlib>

#include <let/exec/context.hpp>

namespace {

let::exec::module build_io_basemod() {
    let::exec::module mod;
    mod.add_function("puts", [](auto&, auto args) {
        const auto& [str] = let::unpack_arg_tuple<let::string>(args);
        std::puts(str.data());
        return let::symbol("ok");
    });
    return mod;
}

let::exec::module& io_basemod() {
    static auto mod = build_io_basemod();
    return mod;
}

void do_extra(let::exec::context& ctx) { ctx.register_module("__io", io_basemod()); }

} // namespace