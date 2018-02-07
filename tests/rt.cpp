#include <let/boxed.hpp>
#include <let/compiler/compile.hpp>
#include <let/eval.hpp>
#include <let/exec/context.hpp>
#include <let/exec/kernel.hpp>
#include <let/list.hpp>
#include <let/parser/parse.hpp>
#include <let/symbol.hpp>
#include <let/tuple.hpp>

#include <catch/catch.hpp>

struct my_int {
    int value;
};

using namespace std::string_literals;

LET_TYPEINFO(my_int, value);
static_assert(let::refl::is_reflected<my_int>::value, "foo");

// TEST_CASE("Iterate reflected members") {
//     using info = let::refl::ct_type_info<my_int>;
//     CHECK(info::name() == "my_int"s);
//     info::foreach_member([](auto mem) {
//         CHECK(mem.name == "value"s);
//         static_assert(std::is_same<typename decltype(mem)::type, int>::value);
//     });
// }

// TEST_CASE("Qualified type info") {
//     using info = let::refl::ct_type_info<const my_int>;
//     CHECK(info::name() == "my_int const");
// }

// TEST_CASE("RTTI") {
//     using ct_info = let::refl::ct_type_info<my_int>;
//     auto rt_info  = let::refl::rt_type_info{ct_info{}};
//     CHECK(rt_info.name() == "my_int"s);
//     CHECK(rt_info == rt_info);
//     auto copy = rt_info;
//     CHECK(copy == rt_info);

//     auto cv_rt = let::refl::rt_type_info::for_type<const my_int&>();
//     CHECK(cv_rt.name() == "my_int const&");
//     CHECK(cv_rt != rt_info);
//     CHECK(cv_rt.is_const());
//     CHECK(!cv_rt.is_volatile());
//     CHECK(cv_rt.is_lref());
//     CHECK(!cv_rt.is_rref());
//     CHECK(cv_rt.is_reference());
// }

// TEST_CASE("Create boxed from symbol") {
//     let::boxed b = let::symbol{"ok"};
//     CHECK(b.type_info() == let::refl::rt_type_info::for_type<let::symbol>());
//     CHECK_THROWS_AS(let::box_convert<int>(b), let::bad_box_cast);
//     CHECK(let::box_cast<let::symbol>(b) == "ok");
// }

// TEST_CASE("Box conversions") {
//     let::boxed b = let::integer{12};
//     CHECK(let::box_convert<int>(b) == 12);
// }

// TEST_CASE("Create tuple") {
//     auto tup = let::tuple::make("foo"s, let::integer(12));
//     CHECK(tup.size() == 2);
//     CHECK(let::box_cast<std::string>(tup[0]) == "foo");
// }

TEST_CASE("Simple eval") {
    auto val = let::eval("2 + 5");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 7);
}

TEST_CASE("Bigger eval") {
    auto val = let::eval("2 + 3 + 4");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 9);
}

TEST_CASE("Bigger eval!") {
    auto val = let::eval("3 + 3 - 4");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 2);
}

TEST_CASE("Eval 2") {
    auto val = let::eval("2 + (6 - 2)");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 6);
}

TEST_CASE("Eval 3") {
    auto val = let::eval("4 - 9 - 22");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == -27);
}

TEST_CASE("Eval 4") {
    auto val = let::eval("4 - (9 - 22)");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 17);
}

TEST_CASE("Eval 5") {
    auto val = let::eval("foo = 12");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 12);
}

TEST_CASE("Eval 6") {
    auto val = let::eval("foo = 33; foo + 3");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 36);
}

TEST_CASE("Eval 7") {
    auto val = let::eval("{1, 2}");
    REQUIRE(val.as_tuple());
}

TEST_CASE("Eval 8") {
    auto val = let::eval(R"(
        tup = {1, 2, 3}
        {first, 2, 3} = tup
        first + 45
    )");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 46);
}

TEST_CASE("Eval 9") {
    auto code  = R"(
        value = cond do
            false -> 31
            true -> 42
            false -> 99 + 23
        end
    )";
    auto ast   = let::ast::parse(code);
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 42);
}

TEST_CASE("Eval 10") {
    auto code  = R"(
        ast = quote do: 12
    )";
    auto ast   = let::ast::parse(code);
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 12);
}

TEST_CASE("Eval 11") {
    auto code  = R"(
        ast = quote do
            pie = 12
        end
    )";
    auto ast   = let::ast::parse(code);
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
}

TEST_CASE("Call function") {
    auto code = R"(
        Test.test_fn(12)
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    let::exec::context ctx{block};

    let::exec::module mod;
    mod.add_function("test_fn", [](auto&, auto arg) {
        auto arglist = arg.as_tuple();
        if (!arglist || arglist->size() != 1) {
            throw std::runtime_error{"Bad arg"};
        }
        auto int_ = (*arglist)[0].as_integer();
        if (!int_) {
            throw std::runtime_error{"Bad arg"};
        }
        return *int_ + 71;
    });

    ctx.register_module("Test", mod);
    auto value = ctx.execute_n(100);
    REQUIRE(value);
    REQUIRE(value->as_integer());
    CHECK(*value->as_integer() == 83);
}

TEST_CASE("case clauses") {
    auto code = R"(
        foo = 12
        case foo do
            11 -> 34
            12 -> 33
        end
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
}

TEST_CASE("case clauses 2") {
    auto code = R"(
        foo = 12
        case foo do
            val -> val + 12
        end
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
}

TEST_CASE("Register a module") {
    auto code = R"(
        modname = MyModule
        mod = :__let.register_module(modname)
    )";
    CHECK_NOTHROW(let::eval(code));
    auto value = let::eval(code);
    auto boxed = value.as_boxed();
    REQUIRE(boxed);
    auto mod = let::box_cast<let::exec::module>(*boxed);
}

TEST_CASE("Anonymous fn") {
    auto code = R"(
        fun = fn
            -> 12
        end
        fun.()
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 12);
}

TEST_CASE("Anonymous fn 2") {
    auto code = R"(
        fun = fn
            1 -> 124
            2 -> 45
            1, 55 -> 2
        end
        fun.(1, 55)
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    CHECK(val == 2);
}

TEST_CASE("Anonymous fn 3") {
    auto code = R"(
        value = 12
        fun = fn
            1 -> 124
            2 -> 45
            1, 55 -> value + 3
        end
        fun.(1, 55)
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(code);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    CHECK(val == 15);
}

TEST_CASE("Anonymous fn 4") {
    auto code = R"(
        value = 12
        fun = fn
            1 -> 124
            2 -> 45
            1, 55 ->
                intermediate = value + 12
                intermediate - 3
        end
        fun.(1, 55)
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(code);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    CHECK(val == 21);
}

TEST_CASE("Anonymous fn 5") {
    auto code = R"(
        value = 12
        fun = fn
            1 -> 124
            2 -> 45
            1, 55 ->
                sum = 1 + 2
                fn -> sum end
        end
        new_fun = fun.(1, 55)
        new_fun.()
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(code);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
    auto val = let::eval(ast);
    CHECK(val == 3);
}

TEST_CASE("Define function") {
    auto code = R"(
        mod = :__let.register_module(MyModule)
        fun = fn
            -> :good
        end
        :__let.register_function(mod, :my_fn, fun)

        :good = MyModule.my_fn()
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE(let::eval(ast) == let::symbol("good"));
}

TEST_CASE("defmodule") {
    auto code = R"(
        defmodule MyModule do
            def my_function(val) do
                val + 42
            end

            def my_function, do: :no_args
            def is_cat_sound(sound) do
                case sound do
                    'meow' -> true
                    'hiss' -> true
                    'barf' -> true
                    _ -> false
                end
            end
        end
    )";
    auto ctx  = let::exec::build_kernel_context();
    REQUIRE_NOTHROW(let::eval(code, ctx));
    auto value = let::eval("MyModule.my_function(13)", ctx);
    REQUIRE(value.as_integer());
    REQUIRE(*value.as_integer() == (13 + 42));
    value = let::eval("MyModule.my_function()", ctx);
    REQUIRE(value.as_symbol());
    CHECK(value.as_symbol()->string() == "no_args");

    auto true_sym  = let::symbol("true");
    auto false_sym = let::symbol("false");
    CHECK(let::eval("MyModule.is_cat_sound('meow')", ctx) == true_sym);
    CHECK(let::eval("MyModule.is_cat_sound('hiss')", ctx) == true_sym);
    CHECK(let::eval("MyModule.is_cat_sound('barf')", ctx) == true_sym);
    CHECK(let::eval("MyModule.is_cat_sound('woof')", ctx) == false_sym);
}

TEST_CASE("Cons 1") {
    auto code = R"(
        list = [:cat, :dog, :bird, :person]
        [hd|tail] = list
        hd
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE(let::eval(ast) == let::symbol("cat"));
}

TEST_CASE("Cons 2") {
    auto code = R"(
        list = [:dog, :bird, :person]
        new_list = [:cat | list]
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    CHECK_NOTHROW(let::eval(ast));
}
