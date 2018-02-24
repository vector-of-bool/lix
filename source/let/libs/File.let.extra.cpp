#include <cstdlib>

#include <chrono>
#include <fstream>

#include <let/raise.hpp>
#include <let/util/args.hpp>
#include <let/util/wrap_fn.hpp>

// TODO: Windows
#include <sys/stat.h>
#include <unistd.h>

using namespace let::literals;

namespace let::File {

struct Stat {
    let::integer access;
    let::integer atime;
    let::integer ctime;
    let::integer gid;
    let::integer inode;
    let::integer links;
    let::integer major_device;
    let::integer minor_device;
    let::integer mode;
    let::integer mtime;
    let::integer size;
    let::integer type;
    let::integer uid;
};

}  // namespace let::File

LET_TYPEINFO(let::File::Stat,
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

let::integer convert_time_t_to_sec(::timespec t) { return t.tv_sec; }

let::exec::module& file_basemod() {
    static auto mod = [] {
        let::exec::module mod;
        mod.add_function("write",
                         let::wrap_function([](const std::string& filepath,
                                               const std::string& content) -> let::value {
                             std::ofstream outfile;
                             outfile.exceptions(std::fstream::failbit);
                             try {
                                 outfile.open(filepath, std::ios_base::binary);
                                 using iter = std::ostreambuf_iterator<char>;
                                 std::copy(content.begin(), content.end(), iter(outfile));
                                 return "ok"_sym;
                             } catch (const std::system_error& e) {
                                 return let::tuple::make("error"_sym,
                                                         error_code_to_sym(e.code().value()));
                             }
                         }));
        mod.add_function("read", let::wrap_function([](const std::string& filepath) -> let::value {
                             std::ifstream infile;
                             infile.exceptions(std::fstream::failbit);
                             try {
                                 infile.open(filepath, std::ios_base::binary);
                                 using iter = std::istreambuf_iterator<char>;
                                 std::string content;
                                 std::copy(iter(infile), iter(), std::back_inserter(content));
                                 return let::tuple::make("ok"_sym, std::move(content));
                             } catch (const std::system_error& e) {
                                 return let::tuple::make("error"_sym,
                                                         error_code_to_sym(e.code().value()));
                             }
                         }));
        mod.add_function("rm", let::wrap_function([](const std::string& filepath) -> let::value {
                             if (::unlink(filepath.data()) == -1) {
                                 return let::tuple::make("error"_sym, error_code_to_sym(errno));
                             }
                             return "ok"_sym;
                         }));
        mod.add_function("stat", let::wrap_function([](const std::string& filepath) -> let::value {
                             struct ::stat st;
                             auto          err = ::stat(filepath.data(), &st);
                             if (err != 0) {
                                 return let::tuple::make("error"_sym, error_code_to_sym(errno));
                             }
                             let::File::Stat let_stat;
                             let_stat.major_device = st.st_dev;
                             let_stat.minor_device = st.st_rdev;
                             let_stat.inode        = st.st_ino;
                             let_stat.mode         = st.st_mode;
                             let_stat.uid          = st.st_uid;
                             let_stat.gid          = st.st_gid;
                             let_stat.size         = st.st_size;
                             let_stat.atime        = convert_time_t_to_sec(st.st_atim);
                             let_stat.mtime        = convert_time_t_to_sec(st.st_mtim);
                             let_stat.ctime        = convert_time_t_to_sec(st.st_ctim);
                             return let::tuple::make("ok"_sym, let::boxed(let_stat));
                         }));
        return mod;
    }();
    return mod;
}

void do_extra(let::exec::context& ctx) { ctx.register_module("__file", file_basemod()); }

}  // namespace