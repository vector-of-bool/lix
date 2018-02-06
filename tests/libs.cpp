#include <catch/catch.hpp>

#include <let/libs/libs.hpp>
#include <let/eval.hpp>

TEST_CASE("Create a context with libraries") {
    auto ctx = let::libs::create_context<let::libs::Enum>();
    auto val = let::eval("[3, 4, 5, 6] = Enum.map([1, 2, 3, 4], fn v -> v + 2 end); :ok", ctx);
    CHECK(val == let::symbol("ok"));
}
