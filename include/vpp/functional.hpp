/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <cmath>
#include <concepts>
#include <cstdlib>
#include <functional>
#include <type_traits>
#include <utility>

namespace vpp
{

/// Function-object type of vpp::abs.
struct abs_fn
{
    template <typename T>
    constexpr auto operator()(const T &x) const
    {
        if constexpr (std::is_unsigned_v<T>)
            return x;
        else if constexpr (std::is_arithmetic_v<T>)
            return x < T{} ? static_cast<T>(-x) : x;
        else
        {
            using std::abs;
            return abs(x); // e.g. std::complex: the magnitude
        }
    }
};

/// Absolute value as a function object, usable as a projection or callback.
///
/// `std::abs` cannot be passed as an argument: it is an overload set, and the
/// standard library does not permit taking the address of its functions.
///
///   auto i = vpp::index_max(spectrum, vpp::abs); // largest-magnitude bin
///
/// Unsigned values are returned unchanged; other arithmetic types are negated
/// when below zero (constexpr, unlike `std::abs` before C++23); anything else
/// (e.g. `std::complex`) goes to `abs` found by `std::` or argument-dependent
/// lookup.
///
/// @note As with `std::abs`, the absolute value of the most negative signed
///       integer is not representable.
inline constexpr abs_fn abs{};

/// A projection giving the distance `|x - target|` from a fixed target.
///
///   auto i = vpp::index_min(v, vpp::distance_to(x)); // see vpp::index_nearest
///
/// For totally ordered operands the distance is computed as the larger minus
/// the smaller, so it does not wrap for unsigned types; otherwise (e.g.
/// `std::complex`) it is `vpp::abs(x - target)`.
///
/// @warning With unsigned elements, use an unsigned target: comparing a signed
///          target against unsigned elements applies the usual arithmetic
///          conversions.
template <typename T>
class distance_to
{
public:
    constexpr explicit distance_to(T target) :
        m_target(std::move(target))
    {
    }

    template <typename U>
    constexpr auto operator()(const U &x) const
    {
        if constexpr (std::totally_ordered_with<U, T>)
            return x < m_target ? m_target - x : x - m_target;
        else
            return vpp::abs(x - m_target);
    }

private:
    T m_target;
};

/// A comparator that compares two objects by a projected value.
///
/// `compare_proj<Proj, Compare>` projects each operand through `Proj` (via
/// std::invoke, so `Proj` may be a pointer-to-data-member, a
/// pointer-to-member-function, or any callable) and feeds the two results to
/// `Compare`. It is stateless and empty, which makes it well suited as the
/// comparator type of an associative container:
///
///   struct player { std::string name; int score; };
///   std::set<player, vpp::compare_proj<&player::name>> by_name;
///
/// or as a one-off predicate:
///
///   std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score>{});
///
/// It is transparent: an operand `Proj` cannot be applied to is taken to be an
/// already-projected key and compared as is. Containers then look up by key
/// without constructing an element:
///
///   by_name.find(std::string_view{"ada"}); // O(log n), no player, no std::string
///
/// For algorithms, ranges projections do the same job
/// (`std::ranges::sort(v, std::less{}, &player::score)`); `compare_proj` exists
/// to give a reusable comparator *type* for containers, which projections do not.
///
/// @tparam Proj    Projection applied to each operand before comparison. Must
///                 be SFINAE-friendly, as pointers to members are: a generic
///                 lambda needs a constraint or an explicit parameter type.
/// @tparam Compare Comparator applied to the projected values; defaults to
///                 `std::less<>` (transparent, so the projected type is deduced).
///                 For keyed lookup it must accept the key type alongside the
///                 projected type.
template <auto Proj, class Compare = std::less<>>
struct compare_proj
{
    using is_transparent = void;

    template <typename L, typename R>
    constexpr bool operator()(const L &lhs, const R &rhs) const
    {
        return Compare{}(project(lhs), project(rhs));
    }

private:
    template <typename U>
    static constexpr decltype(auto) project(const U &u)
    {
        if constexpr(std::is_invocable_v<decltype(Proj), const U &>)
            return std::invoke(Proj, u);
        else
            return (u); // already a key; parenthesised to return by reference
    }
};

} // namespace vpp
