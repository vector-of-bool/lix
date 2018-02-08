#include <catch/catch.hpp>

#include <let/compiler/compile.hpp>
#include <let/compiler/macro.hpp>
#include <let/exec/context.hpp>
#include <let/exec/exec.hpp>
#include <let/exec/kernel.hpp>
#include <let/parser/node.hpp>
#include <let/parser/parse.hpp>

using namespace let;

TEST_CASE("Compile a simple expression") {
    ast::list args;
    args.nodes.emplace_back(integer(2));
    args.nodes.emplace_back(integer(2));
    ast::node root = ast::call(symbol("+"), {}, std::move(args));
    CHECK_NOTHROW(let::compile(root));
    auto block = let::compile(root);
    // Should compile to four instructions:
    CHECK(block.size() == 4);
    // Try to run it
    let::exec::executor ex{block};
    let::exec::context  ctx;
    ex.execute_n(ctx, 3);
}

TEST_CASE("Compile from a string") {
    auto                ast   = let::ast::parse("4 + 5");
    auto                block = let::compile(ast);
    let::exec::executor ex{block};
    let::exec::context  ctx;
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
    REQUIRE_NOTHROW(let::ast::parse(code));
    auto in_ast   = let::ast::parse(code);
    auto ctx      = let::exec::build_kernel_context();
    auto expanded = let::expand_macros(ctx, in_ast);
}