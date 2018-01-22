#include <let/lex/token.hpp>
#include <let/parser.hpp>

#include <catch/catch.hpp>

TEST_CASE("Tokenize some strings", "[parser][tokenizer]") {
    auto str = "foo bar";
    auto toks = let::lex::tokenize(str);
    CHECK(toks.size() == 3);
    str = "foo bar     ";
    toks = let::lex::tokenize(str);
    REQUIRE(toks.size() == 3);
    {
        auto first = toks[0];
        CHECK(first.string() == "foo");
        CHECK(first.range().start == (let::lex::source_location{1, 1}));
        CHECK(first.range().end == (let::lex::source_location{1, 4}));
        auto second = toks[1];
        CHECK(second.string() == "bar");
        CHECK(second.range().start == (let::lex::source_location{1, 5}));
        CHECK(second.range().end == (let::lex::source_location{1, 8}));
    }

    str = "Cat 2 dog";
    toks = let::lex::tokenize(str);
    REQUIRE(toks.size() == 4);
    {
        auto cat = toks[0];
        auto two = toks[1];
        auto dog = toks[2];
        CHECK(cat.type() == let::lex::token_type::word);
        CHECK(cat.string() == "Cat");
        CHECK(cat.range().start == (let::lex::source_location{1, 1}));
        CHECK(cat.range().end == (let::lex::source_location{1, 4}));

        CHECK(two.type() == let::lex::token_type::integer);
        CHECK(two.string() == "2");
        CHECK(two.range().start == (let::lex::source_location{1, 5}));
        CHECK(two.range().end == (let::lex::source_location{1, 6}));

        CHECK(dog.type() == let::lex::token_type::word);
        CHECK(dog.string() == "dog");
        CHECK(dog.range().start == (let::lex::source_location{1, 7}));
        CHECK(dog.range().end == (let::lex::source_location{1, 10}));
    }

    str = "1.2";
    toks = let::lex::tokenize(str);
    REQUIRE(toks.size() == 2);
    {
        auto val = toks[0];
        CHECK(val.type() == let::lex::token_type::decimal);
    }

    str = "foo\n";
    toks = let::lex::tokenize(str);
    REQUIRE(toks.size() == 3);  // We tokenize the newline
    {
        auto word = toks[0];
        auto nl = toks[1];
        CHECK(word.string() == "foo");
        CHECK(word.range().start == (let::lex::source_location{1, 1}));
        CHECK(word.range().end == (let::lex::source_location{1, 4}));
        CHECK(nl.string() == "\n");
        CHECK(nl.range().start == word.range().end);
        CHECK(nl.range().end == (let::lex::source_location{2, 1}));
    }

    str = R"(
        def meow() do
            12
        end
    )";
    toks = let::lex::tokenize(str);
    REQUIRE(toks.size() == 12);
}

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
