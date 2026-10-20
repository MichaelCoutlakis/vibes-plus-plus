/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/functional.hpp>
#include <gtest/gtest.h>
#include <algorithm>
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
