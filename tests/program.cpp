#include <catch/catch.hpp>

#include <lix/exec/context.hpp>
#include <lix/exec/exec.hpp>

namespace is = lix::code::is_types;

TEST_CASE("Simple program") {
    auto code = lix::code::make_code(is::const_int{2},
                                     is::const_int{4},
                                     is::add{{0}, {1}},
                                     is::add{{2}, {1}},
                                     is::ret{{0}});

    lix::exec::executor ex{code};
    lix::exec::context  ctx;
    auto                value = ex.execute_n(ctx, 4);
    CHECK_FALSE(value);
    value = ex.execute_n(ctx, 1);
    CHECK(value);
    value = ex.execute_n(ctx, 12);
    CHECK(value);
}

TEST_CASE("Simple binding match") {
    auto                bl = lix::code::make_code(is::const_int{42},
                                   is::const_binding_slot{{1}},
                                   is::hard_match{{1}, {0}},
                                   is::ret{{0}});
    lix::exec::executor ex{bl};
    lix::exec::context  ctx;
    // Run our block
    auto value = ex.execute_n(ctx, 2);
    CHECK_FALSE(value);
    ex.execute_n(ctx, 1);
    value = ex.execute_n(ctx, 1);
    CHECK(value);
}

TEST_CASE("Match tuple") {
    auto                bl = lix::code::make_code(is::const_int{33},
                                   is::const_int{45},
                                   is::const_binding_slot{{2}},
                                   is::mk_tuple_2{{0}, {2}},  // {33, <slot>}
                                   is::mk_tuple_2{{0}, {1}},  // {33, 45}
                                   is::hard_match{{3}, {4}},  // {33, <slot>} = {33, 45}
                                   is::ret{{0}});
    lix::exec::executor ex{bl};
    lix::exec::context  ctx;
    auto                value = ex.execute_n(ctx, 6);
    CHECK_FALSE(value);
}
