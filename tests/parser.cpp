#include <let/parser.hpp>
#include <let/parser/token.hpp>

#include <catch/catch.hpp>

TEST_CASE("Tokenize some strings", "[parser][tokenizer]") {
    auto str = "foo bar";
    auto toks = let::ast::tokenize(str);
    CHECK(toks.size() == 2);
    str = "foo bar     ";
    toks = let::ast::tokenize(str);
    REQUIRE(toks.size() == 2);
    {
        auto first = toks[0];
        CHECK(first.string() == "foo");
        CHECK(first.range().start == (let::ast::source_location{ 1, 1 }));
        CHECK(first.range().end == (let::ast::source_location{ 1, 4 }));
        auto second = toks[1];
        CHECK(second.string() == "bar");
        CHECK(second.range().start == (let::ast::source_location{ 1, 5 }));
        CHECK(second.range().end == (let::ast::source_location{ 1, 8 }));
    }

    str = "Cat 2 dog";
    toks = let::ast::tokenize(str);
    REQUIRE(toks.size() == 3);
    {
        auto cat = toks[0];
        auto two = toks[1];
        auto dog = toks[2];
        CHECK(cat.string() == "Cat");
        CHECK(cat.range().start == (let::ast::source_location{ 1, 1 }));
        CHECK(cat.range().end == (let::ast::source_location{ 1, 4 }));
        CHECK(two.string() == "2");
        CHECK(two.range().start == (let::ast::source_location{ 1, 5 }));
        CHECK(two.range().end == (let::ast::source_location{ 1, 6 }));
        CHECK(dog.string() == "dog");
        CHECK(dog.range().start == (let::ast::source_location{ 1, 7 }));
        CHECK(dog.range().end == (let::ast::source_location{ 1, 10 }));
    }
}


TEST_CASE("Parse a simple literal", "[parser]") { let::parse_string("12"); }