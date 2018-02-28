#include <lix/parser/parse.hpp>
#include <lix/compiler/compile.hpp>
#include <lix/exec/kernel.hpp>
#include <lix/compiler/macro.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
int compile_istream(std::istream& in) {
    std::string code;
    using iter = std::istreambuf_iterator<char>;
    std::copy(iter(in), iter{}, std::back_inserter(code));
    try {
        auto ctx   = lix::exec::build_kernel_context();
        auto node  = lix::ast::parse(code);
        node       = lix::expand_macros(ctx, node);
        auto block = lix::compile(node);
        std::cout << block;
    } catch (const lix::ast::parse_error& e) {
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