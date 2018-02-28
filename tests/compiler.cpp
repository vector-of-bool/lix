#include <catch/catch.hpp>

#include <lix/compiler/compile.hpp>
#include <lix/compiler/macro.hpp>
#include <lix/exec/context.hpp>
#include <lix/exec/exec.hpp>
#include <lix/exec/kernel.hpp>
#include <lix/parser/node.hpp>
#include <lix/parser/parse.hpp>

using namespace lix;

TEST_CASE("Compile a simple expression") {
    ast::list args;
    args.nodes.emplace_back(integer(2));
    args.nodes.emplace_back(integer(2));
    ast::node root = ast::call(symbol("+"), {}, std::move(args));
    CHECK_NOTHROW(lix::compile(root));
    auto block = lix::compile(root);
    // Should compile to four instructions:
    CHECK(block.size() == 4);
    // Try to run it
    lix::exec::executor ex{block};
    lix::exec::context  ctx;
    ex.execute_n(ctx, 3);
}

TEST_CASE("Compile from a string") {
    auto                ast   = lix::ast::parse("4 + 5");
    auto                block = lix::compile(ast);
    lix::exec::executor ex{block};
    lix::exec::context  ctx;
    auto                ret = ex.execute_n(ctx, 3000);
    REQUIRE(ret);
    REQUIRE(ret->as_integer());
    CHECK(*ret->as_integer() == 9);
}

TEST_CASE("Expand some macros") {
    auto code = R"(
        defmodule MyModule do
            def my_fun do
                something(meow)
            end
            def another_fun do
                what(now)
            end
        end
    )";
    REQUIRE_NOTHROW(lix::ast::parse(code));
    auto in_ast   = lix::ast::parse(code);
    auto ctx      = lix::exec::build_kernel_context();
    auto expanded = lix::expand_macros(ctx, in_ast);
}