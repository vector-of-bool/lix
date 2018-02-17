#include <cstdlib>

#include <fstream>

#include <let/util/args.hpp>
#include <let/util/wrap_fn.hpp>

// TODO: Windows
#include <unistd.h>

using namespace let::literals;

namespace {

let::symbol error_code_to_sym(int e) {
    switch (e) {
    case EBUSY:
        return "ebusy"_sym;
    case ELOOP:
        return "eloop"_sym;
    case ENAMETOOLONG:
        return "enametoolong"_sym;
    case ENOTDIR:
        return "enotdir"_sym;
    case EPERM:
        return "eperm"_sym;
    case EROFS:
        return "erofs"_sym;
    case ETXTBSY:
        return "etxtbsy"_sym;
    case EINVAL:
        return "einval"_sym;
    case ENOENT:
        return "enoent"_sym;
    case ENOSPC:
        return "enospc"_sym;
    case EACCES:
        return "eacces"_sym;
    case EISDIR:
        return "eisdir"_sym;
    default:
        return "unknown_error"_sym;
    }
}

let::exec::module& file_basemod() {
    static auto mod = [] {
        let::exec::module mod;
        mod.add_function("write",
                         let::wrap_function([](const std::string& filepath,
                                               const std::string& content) -> let::value {
                             std::ofstream outfile;
                             outfile.exceptions(std::ifstream::failbit);
                             try {
                                 outfile.open(filepath, std::ios_base::binary);
                                 using iter = std::ostreambuf_iterator<char>;
                                 std::copy(content.begin(), content.end(), iter(outfile));
                                 return "ok"_sym;
                             } catch (const std::system_error& e) {
                                 return let::tuple::make("error"_sym, error_code_to_sym(e.code().value()));
                             }
                         }));
        mod.add_function("rm", let::wrap_function([](const std::string& filepath) -> let::value {
                             if (::unlink(filepath.data()) == -1) {
                                 return let::tuple::make("error"_sym, error_code_to_sym(errno));
                             }
                             return "ok"_sym;
                         }));
        return mod;
    }();
    return mod;
}

void do_extra(let::exec::context& ctx) { ctx.register_module("__file", file_basemod()); }

}  // namespace