#ifndef LIX_LIST_FWD_HPP_INCLUDED
#define LIX_LIST_FWD_HPP_INCLUDED

#include <iterator>
#include <memory>
#include <ostream>

#include "value_fwd.hpp"

namespace lix {

namespace detail {

struct list_node;

}  // namespace detail

class list {
public:
    class iterator {
        std::shared_ptr<detail::list_node> _node;

    public:
        iterator()                = default;
        iterator(const iterator&) = default;
        iterator(iterator&&)      = default;
        explicit iterator(std::shared_ptr<detail::list_node> n)
            : _node(std::move(n)) {}

        inline iterator& operator++();

        inline iterator operator++(int);

        inline bool operator==(const iterator& other) const;
        inline bool operator!=(const iterator& other) const;

        using difference_type   = std::ptrdiff_t;
        using value_type        = lix::value;
        using pointer           = const value_type*;
        using reference         = const value_type&;
        using iterator_category = std::forward_iterator_tag;

        inline reference operator*() const noexcept;
        inline pointer   operator->() const noexcept;
    };

private:
    std::shared_ptr<detail::list_node> _head_node;
    std::size_t                        _size = 0;

    list(std::shared_ptr<detail::list_node> head, std::size_t size)
        : _head_node(std::move(head))
        , _size(size) {}

public:
    constexpr list() noexcept  = default;
    list(const list&) noexcept = default;
    list(list&&) noexcept      = default;
    list& operator=(const list&) noexcept = default;
    list& operator=(list&&) noexcept = default;

    template <typename Iterator,
              typename EndIter,
              typename = typename std::iterator_traits<Iterator>::value_type,
              typename = typename std::iterator_traits<Iterator>::difference_type>
    inline list(Iterator first, EndIter last);

    [[nodiscard]] static list concat(const list& lhs, const list& rhs);

    [[nodiscard]] inline list                             pop_front() const noexcept;
    [[nodiscard]] inline std::pair<lix::value, lix::list> take_front() const noexcept;
    [[nodiscard]] inline list                             push_front(lix::value&&) const noexcept;
    [[nodiscard]] inline list                             push_front(const lix::value&) const;

    inline iterator begin() const noexcept;
    inline iterator end() const noexcept;

    constexpr std::size_t size() const noexcept { return _size; }
};

std::ostream& operator<<(std::ostream& o, const list& l);

}  // namespace lix

#endif  // LIX_LIST_FWD_HPP_INCLUDED