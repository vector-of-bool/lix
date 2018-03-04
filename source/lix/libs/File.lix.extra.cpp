#include <cstdlib>

#include <chrono>
#include <fstream>

#include <lix/raise.hpp>
#include <lix/util/args.hpp>
#include <lix/util/wrap_fn.hpp>

// TODO: Windows
#include <sys/stat.h>
#include <unistd.h>

using namespace lix::literals;

namespace lix::File {

struct Stat {
    lix::integer access;
    lix::integer atime;
    lix::integer ctime;
    lix::integer gid;
    lix::integer inode;
    lix::integer links;
    lix::integer major_device;
    lix::integer minor_device;
    lix::integer mode;
    lix::integer mtime;
    lix::integer size;
    lix::symbol  type = "<invalid>"_sym;
    lix::integer uid;
};

}  // namespace lix::File

LIX_TYPEINFO(lix::File::Stat,
             access,
             atime,
             ctime,
             gid,
             inode,
             links,
             major_device,
             minor_device,
             mode,
             mtime,
             size,
             type,
             uid);

namespace {

lix::symbol type_to_sym(::mode_t st_mode) {
    static const auto dir_sym   = "directory"_sym;
    static const auto file_sym  = "file"_sym;
    static const auto other_sym = "other"_sym;
    if (S_ISDIR(st_mode)) {
        return dir_sym;
    } else if (S_ISREG(st_mode)) {
        return file_sym;
    } else {
        return other_sym;
    }
}

lix::symbol error_code_to_sym(int e) {
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

lix::integer convert_time_t_to_sec(::timespec t) { return t.tv_sec; }

lix::exec::module& file_basemod() {
    static auto mod = [] {
        lix::exec::module mod;
        mod.add_function("write",
                         lix::wrap_function([](const std::string& filepath,
                                               const std::string& content) -> lix::value {
                             std::ofstream outfile;
                             outfile.exceptions(std::fstream::failbit);
                             try {
                                 outfile.open(filepath, std::ios_base::binary);
                                 using iter = std::ostreambuf_iterator<char>;
                                 std::copy(content.begin(), content.end(), iter(outfile));
                                 return "ok"_sym;
                             } catch (const std::system_error& e) {
                                 return lix::tuple::make("error"_sym,
                                                         error_code_to_sym(e.code().value()));
                             }
                         }));
        mod.add_function("read", lix::wrap_function([](const std::string& filepath) -> lix::value {
                             std::ifstream infile;
                             infile.exceptions(std::fstream::failbit);
                             try {
                                 infile.open(filepath, std::ios_base::binary);
                                 using iter = std::istreambuf_iterator<char>;
                                 std::string content;
                                 std::copy(iter(infile), iter(), std::back_inserter(content));
                                 return lix::tuple::make("ok"_sym, std::move(content));
                             } catch (const std::system_error& e) {
                                 return lix::tuple::make("error"_sym,
                                                         error_code_to_sym(e.code().value()));
                             }
                         }));
        mod.add_function("rm", lix::wrap_function([](const std::string& filepath) -> lix::value {
                             if (::unlink(filepath.data()) == -1) {
                                 return lix::tuple::make("error"_sym, error_code_to_sym(errno));
                             }
                             return "ok"_sym;
                         }));
        mod.add_function("stat", lix::wrap_function([](const std::string& filepath) -> lix::value {
                             struct ::stat st;
                             auto          err = ::stat(filepath.data(), &st);
                             if (err != 0) {
                                 return lix::tuple::make("error"_sym, error_code_to_sym(errno));
                             }
                             lix::File::Stat lix_stat;
                             lix_stat.major_device = st.st_dev;
                             lix_stat.minor_device = st.st_rdev;
                             lix_stat.inode        = st.st_ino;
                             lix_stat.mode         = st.st_mode;
                             lix_stat.type         = type_to_sym(st.st_mode);
                             lix_stat.uid          = st.st_uid;
                             lix_stat.gid          = st.st_gid;
                             lix_stat.size         = st.st_size;
                             lix_stat.atime        = convert_time_t_to_sec(st.st_atim);
                             lix_stat.mtime        = convert_time_t_to_sec(st.st_mtim);
                             lix_stat.ctime        = convert_time_t_to_sec(st.st_ctim);
                             return lix::tuple::make("ok"_sym, lix::boxed(lix_stat));
                         }));
        return mod;
    }();
    return mod;
}

void do_extra(lix::exec::context& ctx) { ctx.register_module("__file", file_basemod()); }

}  // namespace