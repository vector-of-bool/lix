#include "map.hpp"

#include <lix/value.hpp>

// #define HAMT_DEBUG_VERBOSE 1

#include <hamt/hash_trie.hpp>

using namespace lix;

using std::move;

namespace {

class map_entry {
    lix::value _key;
    lix::value _value;

public:
    map_entry(lix::value key, lix::value val)
        : _key(move(key))
        , _value(move(val)) {}

    auto& key() const noexcept { return _key; }
    auto& value() const noexcept { return _value; }
};

// inline bool operator==(const map_entry& lhs, const map_entry& rhs) {
//     return lhs.key() == rhs.key();
// }

struct map_entry_lookup {
    static size_t hash(const lix::value& v) { return std::hash<lix::value>()(v); }
    static size_t hash(const map_entry& e) { return hash(e.key()); }

    static bool compare(const map_entry& lhs, const map_entry& rhs) {
        return lhs.key() == rhs.key();
    }

    static bool compare(const map_entry& lhs, const lix::value& rhs) { return lhs.key() == rhs; }
};

}  // namespace

namespace std {

// template <>
// struct hash<map_entry> {
//     std::size_t operator()(const map_entry& entry) const {
//         return std::hash<lix::value>()(entry.key());
//     }
// };

}  // namespace std

namespace lix::detail {

struct map_impl {
    using key_type    = lix::value;
    using value_type  = lix::value;
    using trie_data   = hamt::hash_trie_data<map_entry, map_entry_lookup>;
    using path_type   = hamt::path<map_entry, map_entry_lookup, key_type>;
    using branch_node = hamt::branch_node<map_entry, map_entry_lookup>;
    using leaf_node   = hamt::leaf_node<map_entry, map_entry_lookup>;

    const branch_node* _root = branch_node::create_empty().release();
    std::size_t        _size = 0;

    map_impl() = default;
    map_impl(const map_impl& o)
        : _root(o._root)
        , _size(o._size) {
        addref(_root);
    }
    map_impl(const branch_node* br, std::size_t size)
        : _root(br)
        , _size(size) {}
    ~map_impl() { release(_root); }

    map_impl _insert_at(const path_type& path, const key_type& key, const value_type& value) const {
        map_entry new_entry{key, value};
        if (path.leaf()) {
            auto new_root = add_value_at_leaf(path, move(new_entry));
            return map_impl{new_root, _size + 1};
        } else {
            auto new_root
                = add_value_at_currently_unset_position(path,
                                                        leaf_node::create(move(new_entry),
                                                                          path.whole_hash()));
            return map_impl{new_root, _size + 1};
        }
    }

    map_impl insert(const key_type& key, const value_type& value) const {
        path_type path{key, _root};
        if (auto leaf = path.leaf()) {
            auto ptr = leaf->find(key);
            if (ptr) {
                throw std::runtime_error{"Insert of already-existing entry into map"};
            }
        }
        return _insert_at(path, key, value);
    }

    map_impl insert_or_update(const key_type& key, const value_type& value) const {
        path_type path{key, _root};
        if (auto leaf = path.leaf()) {
            auto existing_idx = leaf->index_of(key);
            if (existing_idx != leaf->npos) {
                // Replace the value
                map_entry new_entry{key, value};
                auto      new_leaf = leaf->with_replaced_value(existing_idx, move(new_entry));
                auto      new_branch
                    = path.last_branch()->with_replaced(hamt::sparse_index(path.hash_chunk()),
                                                        new_leaf.get());
                new_leaf.release();
                auto new_root = path.rewrite(new_branch.get());
                new_branch.release();
                return map_impl(new_root, _size);
            }
        }
        return _insert_at(path, key, value);
    }

    std::optional<std::pair<lix::value, map_impl>> pop(const key_type& key) const {
        path_type path{key, _root};
        if (auto leaf = path.leaf()) {
            auto existing_idx = leaf->index_of(key);
            if (existing_idx != leaf->npos) {
                // Found it
                auto entry    = leaf->get_at(existing_idx);
                auto new_leaf = leaf->with_erased_value(existing_idx);
                auto new_branch
                    = path.last_branch()->with_replaced(hamt::sparse_index(path.hash_chunk()),
                                                        new_leaf.get());
                new_leaf.release();
                auto new_root = path.rewrite(new_branch.get());
                new_branch.release();
                return std::pair(entry.value(), map_impl(new_root, _size - 1));
            }
        }
        return std::nullopt;
    }

    opt_ref<const lix::value> find(const key_type& key) const {
        path_type path{key, _root};
        auto      leaf = path.leaf();
        if (!leaf) {
            return std::nullopt;
        }
        auto ptr = leaf->find(key);
        if (!ptr) {
            return std::nullopt;
        }
        return ptr->value();
    }
};

}  // namespace lix::detail

map::map()
    : map(lix::detail::map_impl()) {}

map::map(detail::map_impl&& impl)
    : _impl(std::make_shared<detail::map_impl>(move(impl))) {}

map map::insert(const lix::value& key, const lix::value& val) const {
    return _impl->insert(key, val);
}

map map::insert_or_update(const lix::value& key, const lix::value& val) const {
    return _impl->insert_or_update(key, val);
}

std::optional<std::pair<lix::value, lix::map>> map::pop(const lix::value& key) const {
    auto pair = _impl->pop(key);
    if (pair) {
        return std::pair(move(pair->first), map(move(pair->second)));
    } else {
        return std::nullopt;
    }
}

opt_ref<const value> map::find(const lix::value& key) const { return _impl->find(key); }

std::ostream& lix::operator<<(std::ostream& o, const lix::map&) {
    o << "%{[map]}";
    return o;
}