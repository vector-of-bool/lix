#include "Enum.hpp"

#include <let/eval.hpp>

void let::libs::Enum::eval(let::exec::context& ctx) {
    let::eval(R"(
      defmodule Enum do
        def map(list, cb), do: _do_map(cb, list, [])
        def _do_map(cb, [hd|tail], acc),  do: _do_map(cb, tail, [cb.(hd)|acc])
        def _do_map(_cb, [], acc),        do: reverse(acc)

        def reverse(list), do: Kernel.__reverse_list(list)

        def each([hd|tail], cb) do
          cb.(hd)
          each(tail, cb)
        end

        def each([], _cb), do: :ok
      end
    )",
              ctx);
}
