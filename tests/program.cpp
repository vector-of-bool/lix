#include <catch/catch.hpp>

#include <let/exec/context.hpp>

namespace is = let::code::is_types;

TEST_CASE("Simple program") {
    auto ebb = let::code::make_code(is::const_int{2},
                                    is::const_int{4},
                                    is::add{{0}, {1}},
                                    is::add{{2}, {1}},
                                    is::ret{{0}});
    let::exec::context ctx{ebb};
    auto               value = ctx.execute_n(4);
    CHECK_FALSE(value);
    CHECK(*ctx.top().as_integer() == 10);
    value = ctx.execute_n(1);
    CHECK(value);
    value = ctx.execute_n(12);
    CHECK(value);
}

TEST_CASE("Simple binding match") {
    auto bl = let::code::make_code(is::const_int{42},
                                   is::const_binding_slot{{1}},
                                   is::hard_match{{1}, {0}},
                                   is::ret{{0}});
    let::exec::context ctx{bl};
    // Run our block
    auto value = ctx.execute_n(2);
    CHECK_FALSE(value);
    ctx.execute_n(1);
    CHECK(*ctx.nth({1}).as_integer() == 42);
    value = ctx.execute_n(1);
    CHECK(value);
}

TEST_CASE("Match tuple") {
    auto bl = let::code::make_code(is::const_int{33},
                                   is::const_int{45},
                                   is::const_binding_slot{{2}},
                                   is::mk_tuple_2{{0}, {2}},  // {33, <slot>}
                                   is::mk_tuple_2{{0}, {1}},  // {33, 45}
                                   is::hard_match{{3}, {4}},  // {33, <slot>} = {33, 45}
                                   is::ret{{0}});
    let::exec::context ctx{bl};
    auto               value = ctx.execute_n(6);
    CHECK_FALSE(value);
    CHECK(*ctx.nth({2}).as_integer() == 45);
}
