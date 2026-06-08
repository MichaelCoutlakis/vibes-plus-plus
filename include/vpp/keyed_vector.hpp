/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace vpp
{

// Insertion-ordered container that stores elements of type T in contiguous
// memory and provides keyed lookup via a compile-time key accessor (KeyFn).
// Lookup is O(n); suited for small collections where stable insertion order
// and contiguous storage matter more than lookup speed.
//
// KeyFn is a non-type template parameter and may be:
//   - a free function pointer:       &my_free_fn          Key(const T&)
//   - a pointer to data member:      &my_struct::member
//   - a pointer to member function:  &my_struct::getter   Key() const
//
// Key is deduced from KeyFn via std::invoke_result but may be overridden
// explicitly as a third template argument.
//
// Because KeyFn is a compile-time constant, lambdas and stateful functors
// are not supported (C++17 restriction on non-type template parameters).
//
// Typical usage via a type alias:
//   using my_container = vpp::keyed_vector<my_data, &my_data::id>;
template <
    typename T,
    auto KeyFn,
    typename Key = std::decay_t<std::invoke_result_t<decltype(KeyFn), const T &>>>
class keyed_vector final
{
public:
    using container_type = std::vector<T>;
    using iterator       = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using key_type       = Key;

    keyed_vector() = default;

    // Constructs from an initializer list. Duplicate keys are ignored (first
    // occurrence wins), consistent with insert() semantics.
    keyed_vector(std::initializer_list<T> il)
    {
        m_items.reserve(il.size());
        for(const auto &t : il)
            insert(t);
    }

    // ---- iterators / capacity ----

    auto begin()  noexcept { return m_items.begin(); }
    auto end()    noexcept { return m_items.end(); }
    auto begin()  const noexcept { return m_items.begin(); }
    auto end()    const noexcept { return m_items.end(); }
    auto cbegin() const noexcept { return m_items.cbegin(); }
    auto cend()   const noexcept { return m_items.cend(); }

    auto size()  const noexcept { return m_items.size(); }
    bool empty() const noexcept { return m_items.empty(); }

    // ---- modifiers ----

    void clear() { m_items.clear(); }

    // Inserts t if its key is not already present.
    // Returns {iterator-to-element, true} on insertion,
    //         {iterator-to-existing, false} on duplicate.
    std::pair<iterator, bool> insert(T t)
    {
        auto k = find_idx(std::invoke(KeyFn, t));
        if(k != npos)
            return {m_items.begin() + k, false};
        m_items.push_back(std::move(t));
        return {std::prev(m_items.end()), true};
    }

    // Inserts t if its key is absent; overwrites the existing element if present.
    // Returns {iterator-to-element, true} on insertion,
    //         {iterator-to-element, false} on assignment.
    // Position in insertion order is preserved on assignment.
    std::pair<iterator, bool> insert_or_assign(T t)
    {
        auto k = find_idx(std::invoke(KeyFn, t));
        if(k != npos)
        {
            m_items[k] = std::move(t);
            return {m_items.begin() + k, false};
        }
        m_items.push_back(std::move(t));
        return {std::prev(m_items.end()), true};
    }

    // No-op if the key is not present.
    void erase(key_type key)
    {
        if(auto *p = find_ptr(key))
            m_items.erase(m_items.begin() + (p - m_items.data()));
    }

    // Throws std::out_of_range if ptr does not point to an element owned by
    // this container.
    void erase(T *ptr)
    {
        const T *base = m_items.data();
        if(ptr < base || ptr >= base + m_items.size())
            throw std::out_of_range("keyed_vector::erase: pointer not in container");
        m_items.erase(m_items.begin() + (ptr - base));
    }

    // ---- element access ----

    // Unchecked access by key. Behaviour is undefined (assert fires in debug
    // builds) when the key is absent.
    T &operator[](key_type key)
    {
        T *p = find_ptr(key);
        assert(p && "keyed_vector::operator[]: key not found");
        return *p;
    }
    const T &operator[](key_type key) const
    {
        const T *p = find_ptr(key);
        assert(p && "keyed_vector::operator[]: key not found");
        return *p;
    }

    // Checked access by key; throws std::out_of_range on miss.
    T &at(key_type key)
    {
        if(T *p = find_ptr(key))
            return *p;
        throw std::out_of_range("keyed_vector::at: key not found");
    }
    const T &at(key_type key) const
    {
        if(const T *p = find_ptr(key))
            return *p;
        throw std::out_of_range("keyed_vector::at: key not found");
    }

    // ---- search ----

    T *find_ptr(key_type key)
    {
        auto k = find_idx(key);
        return k != npos ? m_items.data() + k : nullptr;
    }
    const T *find_ptr(key_type key) const
    {
        auto k = find_idx(key);
        return k != npos ? m_items.data() + k : nullptr;
    }

    iterator find(key_type key)
    {
        auto k = find_idx(key);
        return k != npos ? m_items.begin() + k : m_items.end();
    }
    const_iterator find(key_type key) const
    {
        auto k = find_idx(key);
        return k != npos ? m_items.begin() + k : m_items.end();
    }

    bool contains(key_type key) const { return find_ptr(key) != nullptr; }

    // ---- vector access ----

    // Returns a reference to the underlying vector, giving direct access to
    // the full std::vector API (e.g. index by position, data(), capacity()).
    //
    // Note: the non-const overload allows mutation of elements including their
    // key fields, which can violate the uniqueness invariant. This is no worse
    // than mutation via iterators or find_ptr() and is accepted as a conscious
    // trade-off in exchange for the convenience of the full vector API.
    container_type &as_vec() noexcept { return m_items; }
    const container_type &as_vec() const noexcept { return m_items; }

private:
    std::size_t find_idx(key_type key) const
    {
        for(std::size_t u = 0; u != m_items.size(); ++u)
            if(std::invoke(KeyFn, m_items[u]) == key)
                return u;
        return npos;
    }

    container_type m_items;
    static constexpr std::size_t npos{std::numeric_limits<std::size_t>::max()};
};

} // namespace vpp
