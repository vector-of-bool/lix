#include <experimental/filesystem>

#include <lix/exec/context.hpp>
#include <lix/util/args.hpp>

namespace {

namespace fs = std::experimental::filesystem;

lix::exec::module build_path_basemod() {
    lix::exec::module mod;
    mod.add_function("cwd",
                     [&](auto&, const auto&) -> lix::value { return fs::current_path().string(); });
    mod.add_function("absname", [&](auto&, const auto& tup) -> lix::value {
        const auto& [in_str, base_str] = lix::unpack_arg_tuple<std::string, std::string>(tup);
        auto in_path                   = fs::path(in_str);
        auto base                      = fs::path(base_str);
        if (in_path.is_absolute()) {
            return in_path.string();
        } else {
            return (base / in_path).string();
        }
    });
    mod.add_function("basename", [&](auto&, const auto& tup) {
        const auto& [in_str] = lix::unpack_arg_tuple<std::string>(tup);
        return fs::path(in_str).filename().string();
    });
    mod.add_function("dirname", [&](auto&, const auto& tup) {
        const auto& [in_str] = lix::unpack_arg_tuple<std::string>(tup);
        return fs::path(in_str).parent_path().string();
    });
    mod.add_function("join", [&](auto&, const auto& tup) {
        const auto& [lhs, rhs] = lix::unpack_arg_tuple<std::string, std::string>(tup);
        return (fs::path(lhs) / fs::path(rhs)).string();
    });
    mod.add_function("list_dir_r", [&](auto&, const auto& args) {
        const auto& [path_str] = lix::unpack_arg_tuple<std::string>(args);
        lix::list ret;
        for (auto& entry : fs::recursive_directory_iterator(path_str)) {
            ret = ret.push_front(entry.path().string());
        }
        return ret;
    });
    mod.add_function("extname", [&](auto&, const auto& args) {
        const auto& [path_str] = lix::unpack_arg_tuple<std::string>(args);
        return fs::path(path_str).extension().string();
    });
    // TODO: Path.wildcard
    return mod;
}

lix::exec::module& path_basemod() {
    static auto mod = build_path_basemod();
    return mod;
}

void do_extra(lix::exec::context& ctx) { ctx.register_module("__path", path_basemod()); }

}  // namespace