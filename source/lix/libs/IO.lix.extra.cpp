#include <cstdlib>

#include <lix/exec/context.hpp>
#include <lix/util/args.hpp>

namespace {

lix::exec::module build_io_basemod() {
    lix::exec::module mod;
    mod.add_function("puts", [](auto&, auto args) {
        const auto& [str] = lix::unpack_arg_tuple<lix::string>(args);
        std::puts(str.data());
        return lix::symbol("ok");
    });
    return mod;
}

lix::exec::module& io_basemod() {
    static auto mod = build_io_basemod();
    return mod;
}

void do_extra(lix::exec::context& ctx) { ctx.register_module("__io", io_basemod()); }

} // namespace