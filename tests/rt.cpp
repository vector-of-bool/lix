#include <lix/boxed.hpp>
#include <lix/compiler/compile.hpp>
#include <lix/eval.hpp>
#include <lix/exec/context.hpp>
#include <lix/exec/exec.hpp>
#include <lix/exec/kernel.hpp>
#include <lix/list.hpp>
#include <lix/parser/parse.hpp>
#include <lix/refl_get_member.hpp>
#include <lix/symbol.hpp>
#include <lix/tuple.hpp>

#include <catch/catch.hpp>

using namespace lix::literals;

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

LIX_TYPEINFO(my_int, value);
LIX_TYPEINFO(my_java_int, value, set_value, other_fn);
static_assert(lix::refl::is_reflected<my_int>::value);
static_assert(lix::refl::is_reflected<my_java_int>::value);

static_assert(!lix::is_boxable<std::string>::value);

TEST_CASE("Iterate reflected members") {
    using info = lix::refl::ct_type_info<my_int>;
    CHECK(info::name() == "my_int"s);
    info::foreach_member([](auto mem) {
        CHECK(mem.name == "value"s);
        static_assert(std::is_same<typename decltype(mem)::type, int>::value);
    });
}

TEST_CASE("Qualified type info") {
    using info = lix::refl::ct_type_info<const my_int>;
    CHECK(info::name() == "my_int const");
}

TEST_CASE("RTTI my_int") {
    using ct_info = lix::refl::ct_type_info<my_int>;
    auto rt_info  = lix::refl::rt_type_info{ct_info{}};
    CHECK(rt_info.name() == "my_int"s);
    CHECK(rt_info == rt_info);
    auto copy = rt_info;
    CHECK(copy == rt_info);

    auto cv_rt = lix::refl::rt_type_info::for_type<const my_int&>();
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

    // lix::value v = my_int{};
}

TEST_CASE("RTTI my_java_int") {
    auto rt_info = lix::refl::rt_type_info::for_type<my_java_int>();
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
    auto   boxed_ref = lix::boxed(std::ref(i));
}

TEST_CASE("Map basics") {
    lix::map m;
    auto     orig = m;
    m             = m.insert("foo"_sym, 12);
    // Check that we inserted:
    auto ref = m.find("foo"_sym);
    REQUIRE(ref);
    CHECK(*ref == 12);
    // Check that the original remains untouched:
    ref = orig.find("foo"_sym);
    CHECK_FALSE(ref);
    // Find bogus element:
    ref = m.find("cat"_sym);
    CHECK_FALSE(ref);
    // Try to insert a second item at the same key:
    CHECK_THROWS_AS(m.insert("foo"_sym, 42), std::runtime_error);
    ref = m.find("foo"_sym);
    REQUIRE(ref);
    CHECK(*ref == 12);
    m   = m.insert_or_update("foo"_sym, 42);
    ref = m.find("foo"_sym);
    REQUIRE(ref);
    CHECK(*ref == 42);
    auto pair_opt = m.pop("foo"_sym);
    REQUIRE(pair_opt);
    auto [old_val, new_map] = *pair_opt;
    m                       = new_map;
    ref                     = m.find("foo"_sym);
    CHECK_FALSE(ref);
}

TEST_CASE("Simple eval") {
    auto val = lix::eval("2 + 5");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 7);
}

TEST_CASE("Bigger eval") {
    auto val = lix::eval("2 + 3 + 4");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 9);
}

TEST_CASE("Bigger eval!") {
    auto val = lix::eval("3 + 3 - 4");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 2);
}

TEST_CASE("Eval 2") {
    auto val = lix::eval("2 + (6 - 2)");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 6);
}

TEST_CASE("Eval 3") {
    auto val = lix::eval("4 - 9 - 22");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == -27);
}

TEST_CASE("Eval 4") {
    auto val = lix::eval("4 - (9 - 22)");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 17);
}

TEST_CASE("Eval 5") {
    auto val = lix::eval("foo = 12");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 12);
}

TEST_CASE("Eval 6") {
    auto val = lix::eval("foo = 33; foo + 3");
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 36);
}

TEST_CASE("Eval 7") {
    auto val = lix::eval("{1, 2}");
    REQUIRE(val.as_tuple());
}

TEST_CASE("Eval 8") {
    auto val = lix::eval(R"(
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
    auto ast   = lix::ast::parse(code);
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 42);
}

TEST_CASE("Eval 10") {
    auto code  = R"(
        ast = quote do: 12
    )";
    auto ast   = lix::ast::parse(code);
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
    REQUIRE(val.as_integer());
    CHECK(*val.as_integer() == 12);
}

TEST_CASE("Eval 11") {
    auto code  = R"(
        ast = quote do
            pie = 12
        end
    )";
    auto ast   = lix::ast::parse(code);
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
}

TEST_CASE("Eval 12") {
    auto code  = R"(
        langname = case :cxx do
          :c -> "C"
          :cxx -> "C++"
        end
        langid = "Language " + langname
        "Language C++" = langid
    )";
    auto ast   = lix::ast::parse(code);
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
}

TEST_CASE("Call function") {
    auto code = R"(
        Test.test_fn(12)
    )";
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    lix::exec::executor ex{block};

    lix::exec::module mod;
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

    lix::exec::context ctx;
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
}

TEST_CASE("case clauses 2") {
    auto code = R"(
        foo = 12
        case foo do
            val -> val + 12
        end
    )";
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    try {
        lix::eval(code);
        CHECK(false);
    } catch (const lix::raised_exception& e) {
        CHECK(e.value() == 1);
    }
}

TEST_CASE("Register a module") {
    auto code = R"(
        modname = MyModule
        mod = :__lix.register_module(modname)
    )";
    CHECK_NOTHROW(lix::eval(code));
    auto value = lix::eval(code);
    auto boxed = value.as_boxed();
    REQUIRE(boxed);
    auto mod = lix::box_cast<lix::exec::module>(*boxed);
}

TEST_CASE("Anonymous fn") {
    auto code = R"(
        fun = fn
            -> 12
        end
        fun.()
    )";
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(code);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(code);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
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
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(code);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
    auto val = lix::eval(ast);
    CHECK(val == 3);
}

TEST_CASE("Define function") {
    auto code = R"(
        mod = :__lix.register_module(MyModule)
        fun = fn
            -> :good
        end
        :__lix.register_function(mod, :my_fn, fun)

        :good = MyModule.my_fn()
    )";
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE(lix::eval(ast) == lix::symbol("good"));
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
    auto ctx  = lix::exec::build_kernel_context();
    REQUIRE_NOTHROW(lix::eval(code, ctx));
    auto value = lix::eval("MyModule.my_function(13)", ctx);
    REQUIRE(value.as_integer());
    REQUIRE(*value.as_integer() == (13 + 42));
    value = lix::eval("MyModule.my_function()", ctx);
    REQUIRE(value.as_symbol());
    CHECK(value.as_symbol()->string() == "no_args");

    auto true_sym  = lix::symbol("true");
    auto false_sym = lix::symbol("false");
    CHECK(lix::eval("MyModule.is_cat_sound('meow')", ctx) == true_sym);
    CHECK(lix::eval("MyModule.is_cat_sound('hiss')", ctx) == true_sym);
    CHECK(lix::eval("MyModule.is_cat_sound('barf')", ctx) == true_sym);
    CHECK(lix::eval("MyModule.is_cat_sound('woof')", ctx) == false_sym);
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
    auto ctx  = lix::exec::build_kernel_context();
    REQUIRE_NOTHROW(lix::eval(code, ctx));
    CHECK(lix::eval("OtherMod.test_alias(4)", ctx) == 46);
    // CHECK(lix::eval("OtherMod2.test_alias(4)", ctx) == 46);
}

TEST_CASE("Cons 1") {
    auto code = R"(
        list = [:cat, :dog, :bird, :person]
        [hd|tail] = list
        hd
    )";
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE(lix::eval(ast) == lix::symbol("cat"));
}

TEST_CASE("Cons 2") {
    auto code = R"(
        list = [:dog, :bird, :person]
        new_list = [:cat | list]
    )";
    auto ast  = lix::ast::parse(code);
    CHECK_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    CHECK_NOTHROW(lix::eval(ast));
}

TEST_CASE("Raise 1") {
    auto code = R"(
        raise 5
    )";
    try {
        lix::eval(code);
        CHECK(false);
    } catch (const lix::raised_exception& e) {
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
        lix::eval(code);
        CHECK(false);
    } catch (const lix::raised_exception& e) {
        CHECK(e.value() == lix::tuple::make(lix::symbol("nomatch"), 5));
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
        lix::eval(code);
        CHECK(false);
    } catch (const lix::raised_exception& e) {
        CHECK(e.value()
              == lix::tuple::make(lix::symbol("badarg"), "Dummy.foo", lix::tuple::make(5, 7)));
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
    CHECK_NOTHROW(lix::eval(code));
}

TEST_CASE("to_string") {
    auto code = R"(
        "foo" = to_string("foo")
        "foo" = to_string(:foo)
    )";
    CHECK_NOTHROW(lix::eval(code));
}

TEST_CASE("Unbound captured name in anon_fn") {
    auto code = R"(
        value = 12
        call_fun = fn fun, arg -> fun.(arg) end
        {foo, foo2} = call_fun.(
            fn
                # Even though `foo` is a name in the parent scope, it is not
                # yet been declared. As such, `foo` in this context is an
                # unbound, not a closure variable
                foo -> {foo + 2, foo}
            end,
            12
        )
    )";
    CHECK_NOTHROW(lix::eval(code));
}

TEST_CASE("map 1") {
    auto code = R"(
        map = %{}
    )";
    auto ast  = lix::ast::parse(code);
    REQUIRE_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
}

TEST_CASE("map 2") {
    auto code = R"(
        map = %{foo: 12}
        12 = map.foo
    )";
    auto ast  = lix::ast::parse(code);
    REQUIRE_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast));
}

TEST_CASE("Tail call") {
    auto code = R"(
        tail_fn = fn
            0, _ -> 42
            val, tail ->
                tail.(val - 1, tail)
        end

        42 = tail_fn.(999990, tail_fn)
    )";
    auto ctx  = lix::exec::build_kernel_context();
    auto ast  = lix::ast::parse(code);
    REQUIRE_NOTHROW(lix::compile(ast));
    auto block = lix::compile(ast);
    INFO(block);
    REQUIRE_NOTHROW(lix::eval(ast, ctx));
}
