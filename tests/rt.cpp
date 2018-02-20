#include <let/boxed.hpp>
#include <let/compiler/compile.hpp>
#include <let/eval.hpp>
#include <let/exec/context.hpp>
#include <let/exec/exec.hpp>
#include <let/exec/kernel.hpp>
#include <let/list.hpp>
#include <let/parser/parse.hpp>
#include <let/refl_get_member.hpp>
#include <let/symbol.hpp>
#include <let/tuple.hpp>

#include <catch/catch.hpp>

using namespace let::literals;

struct my_int {
    int value;
};

struct my_java_int {
    int _value;

public:
    int  value() const noexcept { return _value; }
    void set_value(int i) noexcept { _value = i; }
    void other_fn(int) {}
};

using namespace std::string_literals;

LET_TYPEINFO(my_int, value);
LET_TYPEINFO(my_java_int, value, set_value, other_fn);
static_assert(let::refl::is_reflected<my_int>::value);
static_assert(let::refl::is_reflected<my_java_int>::value);

static_assert(!let::is_boxable<std::string>::value);

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

TEST_CASE("RTTI my_int") {
    using ct_info = let::refl::ct_type_info<my_int>;
    auto rt_info  = let::refl::rt_type_info{ct_info{}};
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

    auto mem_info = rt_info.get_member_info("not_a_member");
    CHECK_FALSE(mem_info);

    mem_info = rt_info.get_member_info("value");
    REQUIRE(mem_info);
    CHECK(mem_info->is_gettable());

    // let::value v = my_int{};
}

TEST_CASE("RTTI my_java_int") {
    auto rt_info = let::refl::rt_type_info::for_type<my_java_int>();
    CHECK(rt_info.name() == "my_java_int");
    auto getter_info = rt_info.get_member_info("value");
    REQUIRE(getter_info);
    CHECK(getter_info->is_gettable());
    auto setter_info = rt_info.get_member_info("set_value");
    REQUIRE(setter_info);
    CHECK_FALSE(setter_info->is_gettable());

    my_java_int inst;
    inst.set_value(3);
    CHECK(inst.value() == 3);
    CHECK(getter_info->get(inst) == 3);
}

TEST_CASE("Pass reference") {
    my_int i{42};
    auto   boxed_ref = let::boxed(std::ref(i));
}

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
    let::exec::executor ex{block};

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

    let::exec::context ctx;
    ctx.register_module("Test", mod);
    auto value = ex.execute_n(ctx, 100);
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

TEST_CASE("case clauses 3") {
    auto code = R"(
        tup = {:foo, :bar}
        case tup do
            # Can only match if both elements are the same:
            {key, key} -> raise "Wrong"
            # Matches any 2-tuple
            {key1, key2} -> :ok
        end
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(let::eval(ast));
}

TEST_CASE("case clauses 4") {
    auto code = R"(
        tup = {:foo, :bar}
        case tup do
            nil -> raise :never
            {:foo, other} -> nil
        end
        # Check that the compiler unwinds the case clause properly
        raise 1
    )";
    auto ast  = let::ast::parse(code);
    CHECK_NOTHROW(let::compile(ast));
    auto block = let::compile(ast);
    INFO(block);
    try {
        let::eval(code);
        CHECK(false);
    } catch (const let::raised_exception& e) {
        CHECK(e.value() == 1);
    }
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

TEST_CASE("Alias 1") {
    auto code = R"(
        defmodule MyModule.Foo do
            def bar(val) do
                val + 42
            end
        end

        defmodule OtherMod do
            def test_alias(v) do
                alias MyModule.Foo
                Foo.bar(v)
            end
        end

        ## TODO:
        ## defmodule OtherMod2 do
        ##     alias MyModule.Foo
        ##     def test_alias(v) do
        ##         Foo.bar(v)
        ##     end
        ## end
    )";
    auto ctx  = let::exec::build_kernel_context();
    REQUIRE_NOTHROW(let::eval(code, ctx));
    CHECK(let::eval("OtherMod.test_alias(4)", ctx) == 46);
    // CHECK(let::eval("OtherMod2.test_alias(4)", ctx) == 46);
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

TEST_CASE("Raise 1") {
    auto code = R"(
        raise 5
    )";
    try {
        let::eval(code);
        CHECK(false);
    } catch (const let::raised_exception& e) {
        CHECK(e.value() == 5);
    }
}

TEST_CASE("Raise 2") {
    auto code = R"(
        case 5 do
            1 -> nil
        end
    )";
    try {
        let::eval(code);
        CHECK(false);
    } catch (const let::raised_exception& e) {
        CHECK(e.value() == let::tuple::make(let::symbol("nomatch"), 5));
    }
}

TEST_CASE("Raise badarg") {
    auto code = R"(
        defmodule Dummy do
            def foo do
                nil
            end
        end

        Dummy.foo(5, 7)
    )";
    try {
        let::eval(code);
        CHECK(false);
    } catch (const let::raised_exception& e) {
        CHECK(e.value()
              == let::tuple::make(let::symbol("badarg"), "Dummy.foo", let::tuple::make(5, 7)));
    }
}

TEST_CASE("Pipe") {
    auto code = R"(
        defmodule Dummy do
            def foo do
                :cats
            end

            def bar(:cats) do
                :meow
            end
        end

        :meow = Dummy.foo |> Dummy.bar
    )";
    CHECK_NOTHROW(let::eval(code));
}

TEST_CASE("to_string") {
    auto code = R"(
        "foo" = to_string("foo")
        "foo" = to_string(:foo)
    )";
    CHECK_NOTHROW(let::eval(code));
}