#include <let/eval.hpp>
#include <let/libs/libs.hpp>

#include <iostream>
#include <fstream>

namespace {
int run_istream(std::istream& in) {
    std::string code;
    using iter = std::istreambuf_iterator<char>;
    std::copy(iter(in), iter{}, std::back_inserter(code));
    try {
        auto ctx = let::libs::create_context<let::libs::Enum, let::libs::IO>();
        auto rc = let::eval(code, ctx);
        std::cout << rc << '\n';
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc == 2) {
        std::ifstream in{argv[1]};
        if (!in) {
            std::cerr << "Failed to open filed: " << argv[1] << '\n';
            return 2;
        }
        return run_istream(in);
    } else {
        return run_istream(std::cin);
    }
}
