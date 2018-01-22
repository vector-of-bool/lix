#include <let/parser.hpp>

#include <catch/catch.hpp>

TEST_CASE("Parse a simple literal", "[parser]") {
    std::vector<std::pair<std::string, std::string>> pairs = {
        {"12", "12"},
        {":symbol_here", ":symbol_here"},
        {"2.3", "2.3"},
        {"foo", "{:foo, [], :Var}"},
        {"foo()", "{:foo, [], []}"},
        {"foo(12)", "{:foo, [], [12]}"},
        {"foo(bar())", "{:foo, [], [{:bar, [], []}]}"},
        {"foo 12", "{:foo, [], [12]}"},
        {"foo 12, 13", "{:foo, [], [12, 13]}"},
        {R"(
            foo(
                12,
                13
            )
        )",
         "{:foo, [], [12, 13]}"},
        {R"(
            foo 12,
                13
        )",
         "{:foo, [], [12, 13]}"},
        {"foo\n\n\n", "{:foo, [], :Var}"},
        {"Mod.foo()", "{{:., [], [:Mod, :foo]}, [], []}"},
        {"Mod.foo", "{{:., [], [:Mod, :foo]}, [], []}"},
        {"12", "12"},
        {"12 + 13", "{:+, [], [12, 13]}"},
        {"12 + 13 + 14", "{:+, [], [{:+, [], [12, 13]}, 14]}"},
        {R"(
            1 <- 2 <- 3
        )",
         "{:<-, [], [1, {:<-, [], [2, 3]}]}"},
        {"1 ++ 2", "{:++, [], [1, 2]}"},
        {"1 ++ 2 ++ 3", "{:++, [], [1, {:++, [], [2, 3]}]}"},
        {"12 . foo", "{:., [], [12, :foo]}"},
        {"1 + (2 - 3)", "{:+, [], [1, {:-, [], [2, 3]}]}"},
        {":foo", ":foo"},
        {":foo.bar", "{:., [], [:foo, :bar]}"},
        {"if foo() do 12 end", "{:if, [], [{:foo, [], []}, [{:do, 12}]]}"},
        {"if foo do 12 end",   "{:if, [], [{:foo, [], :Var}, [{:do, 12}]]}"},
        {"foo = 12", "{:=, [], [{:foo, [], :Var}, 12]}"},
        {"foo do 12 end", "{:foo, [], [[{:do, 12}]]}"},
        {"deftype MyType do\n"
         "  value :: int\n"
         "  other :: String\n"
         "end",
         // Now we're getting a bit messy:
         "{:deftype, [], [:MyType, [{:do, {:__block__, [], [{:::, [], [{:value, [], :Var}, {:int, [], :Var}]}, {:::, [], [{:other, [], :Var}, :String]}]}}]]}"},
         {"foo bar: baz", "{:foo, [], [[{:bar, {:baz, [], :Var}}]]}"},
         {"foo 12, bar: baz", "{:foo, [], [12, [{:bar, {:baz, [], :Var}}]]}"},
         {"if foo, do: 12, else: 33", "{:if, [], [{:foo, [], :Var}, [{:do, 12}, {:else, 33}]]}"},
         {"if foo,\n  do: 12,\n  else: 33\n", "{:if, [], [{:foo, [], :Var}, [{:do, 12}, {:else, 33}]]}"},
         {"{12, 22}", "{12, 22}"},
         {"{12, 22, 45}", "{:{}, [], [12, 22, 45]}"},
         {"{foo: 42}", "{[{:foo, 42}]}"},
         {"{:cat, foo: 42}", "{:cat, [{:foo, 42}]}"},
         {"[1, 2, 3]", "[1, 2, 3]"},
    };
    for (auto[code, canon] : pairs) {
        INFO(code);
        auto node = let::ast::parse(code);
        CHECK(to_string(node) == canon);
    }
    // CHECK_THROWS(let::ast::parse("foo("));
    // let::ast::parse("foo + bar");
}
