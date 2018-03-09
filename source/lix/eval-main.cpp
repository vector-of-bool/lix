#include <lix/eval.hpp>
#include <lix/libs/libs.hpp>

#include <fstream>
#include <iostream>

namespace {
int run_istream(std::istream& in) {
    std::string code;
    using iter = std::istreambuf_iterator<char>;
    std::copy(iter(in), iter{}, std::back_inserter(code));
    try {
        auto ctx = lix::libs::create_context<lix::libs::Enum, lix::libs::IO>();
        auto rc  = lix::eval(code, ctx);
        std::cout << rc << '\n';
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
}  // namespace

namespace lix {

int eval_main(int argc, char** argv, char** = nullptr) {
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

}  // namespace lix

// int main(int argc, char** argv) { return lix::eval_main(argc, argv); }
