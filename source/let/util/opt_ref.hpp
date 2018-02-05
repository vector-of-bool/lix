#ifndef LET_UTIL_OPT_REF_HPP_INCLUDED
#define LET_UTIL_OPT_REF_HPP_INCLUDED

#include <cassert>
#include <memory>
#include <optional>

namespace let {

using std::nullopt;

template <typename T>
class opt_ref {
    T* _ptr = nullptr;

public:
    opt_ref() = default;
    opt_ref(T& reference)
        : _ptr(std::addressof(reference)) {}
    opt_ref(std::nullopt_t)
        : _ptr(nullptr) {}

    explicit operator bool() const noexcept { return _ptr != nullptr; }

    T& operator*() noexcept {
        assert(_ptr != nullptr && "Dereferencing null opt_ref");
        return *_ptr;
    }

    T* operator->() noexcept {
        assert(_ptr != nullptr && "Dereferencing null opt_ref");
        return _ptr;
    }
};

}  // namespace let

#endif  // LET_UTIL_OPT_REF_HPP_INCLUDED