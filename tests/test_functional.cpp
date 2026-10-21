/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <algorithm>
#include <complex>
#include <functional>
#include <gtest/gtest.h>
#include <memory>
#include <set>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vpp/functional.hpp>


// ---------------------------------------------------------------------------
// compare_proj
// ---------------------------------------------------------------------------

namespace
{

struct player
{
    std::string name;
    int score;

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
    std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score, std::greater<>>{});
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
    // Names are unique, so a set keyed on them loses nothing.
    std::set<player, vpp::compare_proj<&player::name>> by_name{{"c", 30}, {"a", 10}, {"b", 20}};

    auto it = by_name.begin();
    EXPECT_EQ((it++)->name, "a");
    EXPECT_EQ((it++)->name, "b");
    EXPECT_EQ((it++)->name, "c");
}

TEST(compare_proj, FindsByKeyWithoutConstructingAnElement)
{
    std::set<player, vpp::compare_proj<&player::name>> by_name{{"a", 10}, {"b", 20}};

    const auto it = by_name.find(std::string_view{"b"});
    ASSERT_NE(it, by_name.end());
    EXPECT_EQ(it->score, 20);
    EXPECT_TRUE(by_name.contains("a"));
    EXPECT_FALSE(by_name.contains("z"));
}

TEST(compare_proj, MultisetGroupsEqualKeys)
{
    // Scores are not unique: a set would drop {"c", 20}, a multiset keeps both.
    std::multiset<player, vpp::compare_proj<&player::score>> by_score{
        {"a", 30},
        {"b", 20},
        {"c", 20}
    };

    EXPECT_EQ(by_score.count(20), 2u);
    const auto [lo, hi] = by_score.equal_range(20);
    EXPECT_EQ(std::distance(lo, hi), 2);
    EXPECT_EQ(by_score.lower_bound(25)->name, "a");
}

TEST(compare_proj, ComparesKeyAgainstElementInEitherOrder)
{
    const vpp::compare_proj<&player::score> cmp;
    const player p{"a", 20};
    EXPECT_TRUE(cmp(10, p));
    EXPECT_FALSE(cmp(p, 10));
    EXPECT_TRUE(cmp(p, 30));
    EXPECT_FALSE(cmp(30, p));
}

TEST(compare_proj, ComparesDerivedAgainstBase)
{
    struct pro : player
    {
    };
    const vpp::compare_proj<&player::score> cmp;
    EXPECT_TRUE(cmp(pro{{"a", 10}}, player{"b", 20}));
    EXPECT_FALSE(cmp(player{"b", 20}, pro{{"a", 10}}));
}

TEST(compare_proj, IsEmptyAndTransparent)
{
    using cmp = vpp::compare_proj<&player::score>;
    static_assert(std::is_empty_v<cmp>);
    static_assert(requires { typename cmp::is_transparent; });
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

TEST(abs, GivesComplexMagnitude) { EXPECT_DOUBLE_EQ(vpp::abs(std::complex<double>{3, -4}), 5.0); }

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

TEST(distance_to, IsConstexpr) { static_assert(vpp::distance_to(5)(2) == 3); }
