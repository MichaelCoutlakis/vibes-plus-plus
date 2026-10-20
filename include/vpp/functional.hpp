/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <functional>

namespace vpp
{

/// A comparator that compares two objects by a projected value.
///
/// `compare_proj<Proj, Compare>` projects each operand through `Proj` (via
/// std::invoke, so `Proj` may be a pointer-to-data-member, a
/// pointer-to-member-function, or any callable) and feeds the two results to
/// `Compare`. It is stateless and empty, which makes it well suited as the
/// comparator type of an associative container:
///
///   struct player { std::string name; int score; };
///   std::set<player, vpp::compare_proj<&player::score>> by_score;
///
/// or as a one-off predicate:
///
///   std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score>{});
///
/// For algorithms, ranges projections do the same job
/// (`std::ranges::sort(v, std::less{}, &player::score)`); `compare_proj` exists
/// to give a reusable comparator *type* for containers, which projections do not.
///
/// @tparam Proj    Projection applied to each operand before comparison.
/// @tparam Compare Comparator applied to the projected values; defaults to
///                 `std::less<>` (transparent, so the projected type is deduced).
template <auto Proj, class Compare = std::less<>>
struct compare_proj
{
    template <typename T>
    constexpr bool operator()(const T &lhs, const T &rhs) const
    {
        return Compare{}(std::invoke(Proj, lhs), std::invoke(Proj, rhs));
    }
};

} // namespace vpp
