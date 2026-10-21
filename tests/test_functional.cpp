/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/functional.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <complex>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// compare_proj
// ---------------------------------------------------------------------------

namespace {

struct player {
    std::string name;
    int         score;

    int rank() const { return -score; } // a member-function projection
};

} // namespace

TEST(compare_proj, SortsByDataMemberAscending)
{
    std::vector<player> v{{"a", 30}, {"b", 10}, {"c", 20}};
    std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score>{});
    EXPECT_EQ(v[0].name, "b");
    EXPECT_EQ(v[1].name, "c");
    EXPECT_EQ(v[2].name, "a");
}

TEST(compare_proj, SortsByDataMemberDescendingWithCustomCompare)
{
    std::vector<player> v{{"a", 30}, {"b", 10}, {"c", 20}};
    std::sort(
        v.begin(), v.end(), vpp::compare_proj<&player::score, std::greater<>>{});
    EXPECT_EQ(v[0].name, "a");
    EXPECT_EQ(v[2].name, "b");
}

TEST(compare_proj, ProjectsThroughMemberFunction)
{
    // rank() == -score, so ascending rank is descending score.
    std::vector<player> v{{"a", 30}, {"b", 10}, {"c", 20}};
    std::sort(v.begin(), v.end(), vpp::compare_proj<&player::rank>{});
    EXPECT_EQ(v[0].name, "a");
    EXPECT_EQ(v[2].name, "b");
}

TEST(compare_proj, ServesAsAssociativeContainerComparator)
{
    std::set<player, vpp::compare_proj<&player::score>> by_score;
    by_score.insert({"a", 30});
    by_score.insert({"b", 10});
    by_score.insert({"c", 20});

    auto it = by_score.begin();
    EXPECT_EQ((it++)->name, "b");
    EXPECT_EQ((it++)->name, "c");
    EXPECT_EQ((it++)->name, "a");
}

TEST(compare_proj, IsEmptyAndStateless)
{
    static_assert(std::is_empty_v<vpp::compare_proj<&player::score>>);
    EXPECT_TRUE(std::is_empty_v<vpp::compare_proj<&player::score>>);
}

// ---------------------------------------------------------------------------
// abs
// ---------------------------------------------------------------------------

TEST(abs, HandlesArithmeticTypes)
{
    EXPECT_EQ(vpp::abs(-3), 3);
    EXPECT_EQ(vpp::abs(3), 3);
    EXPECT_EQ(vpp::abs(7u), 7u);
    EXPECT_DOUBLE_EQ(vpp::abs(-2.5), 2.5);
    static_assert(std::is_same_v<decltype(vpp::abs(short{-1})), short>);
}

TEST(abs, IsConstexpr)
{
    static_assert(vpp::abs(-4) == 4);
    static_assert(vpp::abs(-1.5) == 1.5);
}

TEST(abs, GivesComplexMagnitude)
{
    EXPECT_DOUBLE_EQ(vpp::abs(std::complex<double>{3, -4}), 5.0);
}

TEST(abs, WorksAsProjection)
{
    std::vector<int> v{-5, 2, -1, 4};
    std::ranges::sort(v, {}, vpp::abs);
    EXPECT_EQ(v, (std::vector<int>{-1, 2, 4, -5}));
}

// ---------------------------------------------------------------------------
// distance_to
// ---------------------------------------------------------------------------

TEST(distance_to, MeasuresBothSidesOfTarget)
{
    const vpp::distance_to d(10);
    EXPECT_EQ(d(7), 3);
    EXPECT_EQ(d(13), 3);
    EXPECT_EQ(d(10), 0);
}

TEST(distance_to, DoesNotWrapForUnsigned)
{
    const vpp::distance_to d(4u);
    EXPECT_EQ(d(3u), 1u);
    EXPECT_EQ(d(10u), 6u);
}

TEST(distance_to, AcceptsMixedArithmeticTypes)
{
    const vpp::distance_to d(2.5);
    EXPECT_DOUBLE_EQ(d(1), 1.5);
    EXPECT_DOUBLE_EQ(d(4), 1.5);
}

TEST(distance_to, MeasuresComplexByMagnitude)
{
    const vpp::distance_to d(std::complex<double>{1, 1});
    EXPECT_DOUBLE_EQ(d(std::complex<double>{4, 5}), 5.0);
}

TEST(distance_to, IsConstexpr)
{
    static_assert(vpp::distance_to(5)(2) == 3);
}
