#include <catch/catch.hpp>

#include <let/eval.hpp>
#include <let/libs/libs.hpp>

TEST_CASE("Create a context with libraries") {
    auto ctx = let::libs::create_context<let::libs::Enum,
                                         let::libs::String,
                                         let::libs::IO,
                                         let::libs::Regex,
                                         let::libs::Keyword,
                                         let::libs::File>();
    auto val = let::eval("[3, 4, 5, 6] = Enum.map([1, 2, 3, 4], fn v -> v + 2 end); :ok", ctx);
    CHECK(val == let::symbol("ok"));

    val = let::eval(R"code(
        [3, 3, 3] = Enum.filter([1, 2, 3, 2, 5, 8, 3, 4, 3], fn
            3 -> true
            _ -> false
        end)

        [1, 2, 3] = Enum.flatten([
            [1],
            [2, 3]
        ])

        true = String.contains?("Hello, world!", "world!")
        false = String.contains?("Hello, world!", "Eggs")

        found = Enum.find(["foo", "bar", "baz"], fn el -> String.contains?(el, "z") end)
        "baz" = found

        {:ok, re} = Regex.compile("foo.*")
        {:error, re_error} = Regex.compile("*foo[");

        nil = Regex.run(re, "cats")
        ["fooooootball"] = Regex.run(re, "fooooootball")
        ["foobar", "bar"] = Regex.run("foo(bar)", "foobar")

        true = Regex.regex?(re)
        false = Regex.regex?(4)

        kwlist = [
            foo: :bar,
            baz: "String"
        ]

        :bar = Keyword.get(kwlist, :foo)
        nil = Keyword.get(kwlist, :meow)
        :foo = Keyword.get(kwlist, :meow, :foo)
        "String" = Keyword.get(kwlist, :baz)

        [2, 3, 4] = Enum.map([1, 2, 3], &(&1 + 1))

        IO.puts("Hello, Mike")
    )code",
                    ctx);
}
