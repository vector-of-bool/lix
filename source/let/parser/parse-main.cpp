#include <let/parser/parse.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>

namespace {
int parse_istream(std::istream& in) {
    std::string code;
    using iter = std::istreambuf_iterator<char>;
    std::copy(iter(in), iter{}, std::back_inserter(code));
    try {
        auto node = let::ast::parse(code);
        std::cout << node << '\n';
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
        return parse_istream(in);
    } else {
        return parse_istream(std::cin);
    }
}