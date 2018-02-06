#include "Enum.hpp"

#include <let/eval.hpp>

void let::libs::Enum::eval(let::exec::context& ctx) {
    let::eval(R"(
      defmodule Enum do
        def map(list, cb), do: _do_map(cb, list, [])
        def _do_map(cb, list, acc) do
          case list do
            [hd|tail] ->
              new_acc = [cb.(hd) | acc]
              _do_map(cb, tail, new_acc)
            [hd] ->
              reverse [cb.(hd) | acc]
            [] -> acc
          end
        end
        def reverse(list), do: Kernel.__reverse_list(list)
      end
    )",
              ctx);
}
