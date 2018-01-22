#include <let/boxed.hpp>
#include <let/symbol.hpp>
#include <let/tuple.hpp>
#include <let/list.hpp>

#include <catch/catch.hpp>

struct my_int {
    int value;
};

using namespace std::string_literals;

LET_TYPEINFO(my_int, value);
static_assert(let::refl::is_reflected<my_int>::value, "foo");

TEST_CASE("Iterate reflected members") {
    using info = let::refl::ct_type_info<my_int>;
    CHECK(info::name() == "my_int"s);
    info::foreach_member([](auto mem) {
        CHECK(mem.name == "value"s);
        static_assert(std::is_same<typename decltype(mem)::type, int>::value);
    });
}

TEST_CASE("Qualified type info") {
    using info = let::refl::ct_type_info<const my_int>;
    CHECK(info::name() == "my_int const");
}

TEST_CASE("RTTI") {
    using ct_info = let::refl::ct_type_info<my_int>;
    auto rt_info = let::refl::rt_type_info{ct_info{}};
    CHECK(rt_info.name() == "my_int"s);
    CHECK(rt_info == rt_info);
    auto copy = rt_info;
    CHECK(copy == rt_info);

    auto cv_rt = let::refl::rt_type_info::for_type<const my_int&>();
    CHECK(cv_rt.name() == "my_int const&");
    CHECK(cv_rt != rt_info);
    CHECK(cv_rt.is_const());
    CHECK(!cv_rt.is_volatile());
    CHECK(cv_rt.is_lref());
    CHECK(!cv_rt.is_rref());
    CHECK(cv_rt.is_reference());
}

TEST_CASE("Create boxed from symbol") {
    let::boxed b = let::symbol{"ok"};
    CHECK(b.type_info() == let::refl::rt_type_info::for_type<let::symbol>());
    CHECK_THROWS_AS(let::box_convert<int>(b), let::bad_box_cast);
    CHECK(let::box_cast<let::symbol>(b) == "ok");
}

TEST_CASE("Box conversions") {
    let::boxed b = let::integer{12};
    CHECK(let::box_convert<int>(b) == 12);
}

TEST_CASE("Create tuple") {
    auto tup = let::tuple::make("foo"s, let::integer(12));
    CHECK(tup.size() == 2);
    CHECK(let::box_cast<std::string>(tup[0]) == "foo");
}
