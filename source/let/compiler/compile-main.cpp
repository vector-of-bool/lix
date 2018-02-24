#include <let/parser/parse.hpp>
#include <let/compiler/compile.hpp>
#include <let/exec/kernel.hpp>
#include <let/compiler/macro.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
int compile_istream(std::istream& in) {
    std::string code;
    using iter = std::istreambuf_iterator<char>;
    std::copy(iter(in), iter{}, std::back_inserter(code));
    try {
        auto ctx   = let::exec::build_kernel_context();
        auto node  = let::ast::parse(code);
        node       = let::expand_macros(ctx, node);
        auto block = let::compile(node);
        std::cout << block;
    } catch (const let::ast::parse_error& e) {
        std::cerr << "FAIL:\n" << e.what() << '\n';
        return 1;
    }
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc == 2) {
        std::ifstream in{argv[1]};
        if (!in) {
            std::cerr << "Failed to open filed: " << argv[1] << '\n';
            return 2;
        }
        return compile_istream(in);
    } else {
        return compile_istream(std::cin);
    }
}