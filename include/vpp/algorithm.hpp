/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <vpp/functional.hpp>
#include <vpp/macros.hpp>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ranges>
#include <stdexcept>

// Index-returning counterparts of std::ranges::max_element / min_element, in the
// spirit of Armadillo's index_max / index_min. Each throws std::invalid_argument
// on an empty range, and on ties returns the lowest index.

namespace vpp
{

/// Index of the largest element, compared by `proj`.
///
///   auto i = vpp::index_max(v);
///   auto j = vpp::index_max(spectrum, vpp::abs); // largest magnitude
///
/// The range may be a temporary: only the index is returned.
///
/// @throws std::invalid_argument if `r` is empty.
template <
    std::ranges::forward_range R,
    std::indirectly_regular_unary_invocable<std::ranges::iterator_t<R>> Proj = std::identity>
    requires std::indirect_strict_weak_order<
        std::ranges::less,
        std::projected<std::ranges::iterator_t<R>, Proj>>
constexpr std::size_t index_max(R &&r, Proj proj = {})
{
    auto first = std::ranges::begin(r);
    auto last = std::ranges::end(r);
    if (first == last)
        throw std::invalid_argument("index_max: empty range");
    return static_cast<std::size_t>(
        std::ranges::distance(first, std::ranges::max_element(first, last, {}, std::move(proj))));
}

/// Index of the smallest element, compared by `proj`.
///
///   auto i = vpp::index_min(v);
///   auto j = vpp::index_min(v, vpp::distance_to(x)); // or vpp::index_nearest(v, x)
///
/// @throws std::invalid_argument if `r` is empty.
template <
    std::ranges::forward_range R,
    std::indirectly_regular_unary_invocable<std::ranges::iterator_t<R>> Proj = std::identity>
    requires std::indirect_strict_weak_order<
        std::ranges::less,
        std::projected<std::ranges::iterator_t<R>, Proj>>
constexpr std::size_t index_min(R &&r, Proj proj = {})
{
    auto first = std::ranges::begin(r);
    auto last = std::ranges::end(r);
    if (first == last)
        throw std::invalid_argument("index_min: empty range");
    return static_cast<std::size_t>(
        std::ranges::distance(first, std::ranges::min_element(first, last, {}, std::move(proj))));
}

/// Index of the element whose projection is nearest to `x`.
///
///   auto i = vpp::index_nearest(v, 4.0);
///   auto j = vpp::index_nearest(bins, 445.0, &bin::freq);
///
/// O(n); see index_nearest_sorted for sorted input. Distances are computed by
/// vpp::distance_to, so unsigned elements do not wrap and `std::complex`
/// elements are measured by magnitude. For any other metric, use index_min with
/// a projection that returns the distance.
///
/// @throws std::invalid_argument if `r` is empty.
template <
    std::ranges::forward_range R,
    typename T,
    std::indirectly_regular_unary_invocable<std::ranges::iterator_t<R>> Proj = std::identity>
constexpr std::size_t index_nearest(R &&r, const T &x, Proj proj = {})
{
    auto first = std::ranges::begin(r);
    auto last = std::ranges::end(r);
    if (first == last)
        throw std::invalid_argument("index_nearest: empty range");
    const vpp::distance_to<T> dist(x);
    auto by_distance = [&](auto &&e) { return dist(std::invoke(proj, VPP_FWD(e))); };
    return static_cast<std::size_t>(
        std::ranges::distance(first, std::ranges::min_element(first, last, {}, by_distance)));
}

/// Index of the element whose projection is nearest to `x`, in a range sorted
/// ascending by that projection, by binary search.
///
/// Returns the same index as index_nearest: on a tie between neighbours the
/// lower one wins, and of equal values the first. O(log n) comparisons; O(log n)
/// time on random-access ranges.
///
/// @pre `r` is sorted ascending by `proj` under `std::ranges::less`.
/// @throws std::invalid_argument if `r` is empty.
template <
    std::ranges::bidirectional_range R,
    typename T,
    std::indirectly_regular_unary_invocable<std::ranges::iterator_t<R>> Proj = std::identity>
    requires std::totally_ordered_with<
        std::indirect_result_t<Proj &, std::ranges::iterator_t<R>>,
        const T &>
constexpr std::size_t index_nearest_sorted(R &&r, const T &x, Proj proj = {})
{
    auto first = std::ranges::begin(r);
    auto last = std::ranges::end(r);
    if (first == last)
        throw std::invalid_argument("index_nearest_sorted: empty range");

    auto it = std::ranges::lower_bound(first, last, x, {}, proj); // first projection >= x
    if (it != first)
    {
        auto prev = std::ranges::prev(it);
        const vpp::distance_to<T> dist(x);
        if (it == last || !(dist(std::invoke(proj, *it)) < dist(std::invoke(proj, *prev))))
        {
            // first of equal projections
            it = std::ranges::lower_bound(first, prev, std::invoke(proj, *prev), {}, proj);
        }
    }
    return static_cast<std::size_t>(std::ranges::distance(first, it));
}

} // namespace vpp
