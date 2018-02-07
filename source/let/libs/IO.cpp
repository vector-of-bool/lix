#include "IO.hpp"

#include <cstdlib>

#include <let/eval.hpp>
#include <let/util/args.hpp>

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

}  // namespace

void let::libs::IO::eval(let::exec::context& ctx) {
    ctx.register_module("__io", io_basemod());
    let::eval(R"(
      defmodule IO do
        def puts(str), do: :__io.puts(str)
      end
    )",
              ctx);
}
