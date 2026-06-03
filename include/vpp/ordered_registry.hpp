/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <cassert>
#include <cstddef>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vpp
{

// Insertion-ordered container that stores elements by a key derived from each
// element via a caller-supplied callable (KeyFunc). Lookup is O(n); suited for
// small registries where stable order and contiguous storage matter more than
// lookup speed.
//
// KeyFunc must be callable as: RegKey(const T&)
// For stateless functors and free functions it may be default-constructed;
// for lambdas, pass the instance to the constructor.
template <typename T, typename RegKey, typename KeyFunc>
class ordered_registry
{
public:
    using container_type = std::vector<T>;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;
    using key_type = RegKey;

    ordered_registry() = default;
    explicit ordered_registry(KeyFunc kf) :
        m_key_func(std::move(kf))
    {
    }

    // ---- iterators / capacity ----

    auto begin() noexcept { return m_items.begin(); }
    auto end() noexcept { return m_items.end(); }
    auto begin() const noexcept { return m_items.begin(); }
    auto end() const noexcept { return m_items.end(); }
    auto cbegin() const noexcept { return m_items.cbegin(); }
    auto cend() const noexcept { return m_items.cend(); }

    auto size() const noexcept { return m_items.size(); }
    bool empty() const noexcept { return m_items.empty(); }

    // ---- modifiers ----

    void clear() { m_items.clear(); }

    // Inserts t if its key is not already present.
    // Returns {iterator-to-element, true} on insertion,
    //         {iterator-to-existing,  false} on duplicate.
    std::pair<iterator, bool> insert(T t)
    {
        auto k = find_idx(m_key_func(t));
        if(k != npos)
            return {m_items.begin() + k, false};
        m_items.push_back(std::move(t));
        return {std::prev(m_items.end()), true};
    }

    // Inserts t if its key is absent; overwrites the existing element if present.
    // Returns {iterator-to-element, true} on insertion,
    //         {iterator-to-element, false} on assignment.
    std::pair<iterator, bool> insert_or_assign(T t)
    {
        auto k = find_idx(m_key_func(t));
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
            throw std::out_of_range("ordered_registry::erase: pointer not in container");
        m_items.erase(m_items.begin() + (ptr - base));
    }

    // ---- element access ----

    // Unchecked access by key. Behaviour is undefined (assert fires in debug
    // builds) when the key is absent.
    T &operator[](key_type key)
    {
        T *p = find_ptr(key);
        assert(p && "ordered_registry::operator[]: key not found");
        return *p;
    }
    const T &operator[](key_type key) const
    {
        const T *p = find_ptr(key);
        assert(p && "ordered_registry::operator[]: key not found");
        return *p;
    }

    // Checked access by key; throws std::out_of_range on miss.
    T &at(key_type key)
    {
        if(T *p = find_ptr(key))
            return *p;
        throw std::out_of_range("ordered_registry::at: key not found");
    }
    const T &at(key_type key) const
    {
        if(const T *p = find_ptr(key))
            return *p;
        throw std::out_of_range("ordered_registry::at: key not found");
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

protected:
    auto &items() noexcept { return m_items; }
    const auto &items() const noexcept { return m_items; }

    std::size_t find_idx(key_type key) const
    {
        for(std::size_t u = 0; u != m_items.size(); ++u)
            if(m_key_func(m_items[u]) == key)
                return u;
        return npos;
    }

private:
    container_type m_items;
    KeyFunc m_key_func{};
    static constexpr std::size_t npos{std::numeric_limits<std::size_t>::max()};
};

// Deduction guide: ordered_registry reg(&get_key);
template <typename T, typename RegKey>
ordered_registry(RegKey (*)(const T &)) -> ordered_registry<T, RegKey, RegKey (*)(const T &)>;

} // namespace vpp
