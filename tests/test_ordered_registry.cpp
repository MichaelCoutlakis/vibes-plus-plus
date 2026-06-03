/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/ordered_registry.hpp>
#include <gtest/gtest.h>
#include <string>

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

struct widget {
    int         id;
    std::string name;
};

static int widget_id(const widget& w) { return w.id; }

using widget_registry = vpp::ordered_registry<widget, int, int (*)(const widget&)>;

static widget_registry make_reg()
{
    return widget_registry{&widget_id};
}

// ---------------------------------------------------------------------------
// insert / size / empty
// ---------------------------------------------------------------------------

TEST(ordered_registry, InsertIncreasesSize)
{
    auto reg = make_reg();
    EXPECT_TRUE(reg.empty());
    auto [it, inserted] = reg.insert({1, "alpha"});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->id, 1);
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_FALSE(reg.empty());
}

TEST(ordered_registry, InsertDuplicateReturnsFalseAndExistingIterator)
{
    auto reg = make_reg();
    reg.insert({42, "first"});
    auto [it, inserted] = reg.insert({42, "second"});
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->name, "first");   // existing element, not overwritten
    EXPECT_EQ(reg.size(), 1u);
}

// ---------------------------------------------------------------------------
// insert_or_assign
// ---------------------------------------------------------------------------

TEST(ordered_registry, InsertOrAssignInsertsWhenAbsent)
{
    auto reg = make_reg();
    auto [it, inserted] = reg.insert_or_assign({7, "seven"});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->name, "seven");
    EXPECT_EQ(reg.size(), 1u);
}

TEST(ordered_registry, InsertOrAssignOverwritesExisting)
{
    auto reg = make_reg();
    reg.insert({7, "seven"});
    auto [it, inserted] = reg.insert_or_assign({7, "SEVEN"});
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->name, "SEVEN");
    EXPECT_EQ(reg.size(), 1u);       // no new element
}

TEST(ordered_registry, InsertOrAssignPreservesOrder)
{
    auto reg = make_reg();
    reg.insert({1, "one"});
    reg.insert({2, "two"});
    reg.insert({3, "three"});
    reg.insert_or_assign({2, "TWO"});  // overwrite middle element
    std::vector<int> order;
    for(const auto& w : reg)
        order.push_back(w.id);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));  // order unchanged
    EXPECT_EQ(reg.at(2).name, "TWO");
}

// ---------------------------------------------------------------------------
// find_ptr / find / contains
// ---------------------------------------------------------------------------

TEST(ordered_registry, FindPtrReturnsNullOnMiss)
{
    auto reg = make_reg();
    reg.insert({1, "a"});
    EXPECT_EQ(reg.find_ptr(99), nullptr);
}

TEST(ordered_registry, FindPtrReturnsCorrectElement)
{
    auto reg = make_reg();
    reg.insert({7, "seven"});
    const widget* p = reg.find_ptr(7);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "seven");
}

TEST(ordered_registry, FindIteratorEqualsEndOnMiss)
{
    auto reg = make_reg();
    EXPECT_EQ(reg.find(0), reg.end());
}

TEST(ordered_registry, Contains)
{
    auto reg = make_reg();
    reg.insert({3, "three"});
    EXPECT_TRUE(reg.contains(3));
    EXPECT_FALSE(reg.contains(4));
}

// ---------------------------------------------------------------------------
// operator[] and at(key)
// ---------------------------------------------------------------------------

TEST(ordered_registry, AtKeyReturnsCorrectElement)
{
    auto reg = make_reg();
    reg.insert({5, "five"});
    EXPECT_EQ(reg.at(5).name, "five");
}

TEST(ordered_registry, AtKeyThrowsOnMiss)
{
    auto reg = make_reg();
    EXPECT_THROW(reg.at(99), std::out_of_range);
}

TEST(ordered_registry, AtKeyConstOverload)
{
    auto reg = make_reg();
    reg.insert({5, "five"});
    const auto& creg = reg;
    EXPECT_EQ(creg.at(5).name, "five");
    EXPECT_THROW(creg.at(99), std::out_of_range);
}

// ---------------------------------------------------------------------------
// erase by key
// ---------------------------------------------------------------------------

TEST(ordered_registry, EraseByKeyRemovesElement)
{
    auto reg = make_reg();
    reg.insert({1, "a"});
    reg.insert({2, "b"});
    reg.erase(1);
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_EQ(reg.find_ptr(1), nullptr);
    EXPECT_NE(reg.find_ptr(2), nullptr);
}

TEST(ordered_registry, EraseByMissingKeyIsNoOp)
{
    auto reg = make_reg();
    reg.insert({1, "a"});
    reg.erase(99);
    EXPECT_EQ(reg.size(), 1u);
}

// ---------------------------------------------------------------------------
// erase by pointer
// ---------------------------------------------------------------------------

TEST(ordered_registry, EraseByPtrRemovesElement)
{
    auto reg = make_reg();
    reg.insert({1, "a"});
    reg.insert({2, "b"});
    widget* p = reg.find_ptr(1);
    ASSERT_NE(p, nullptr);
    reg.erase(p);
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_FALSE(reg.contains(1));
}

TEST(ordered_registry, EraseByPtrOutOfRangeThrows)
{
    auto reg = make_reg();
    reg.insert({1, "a"});
    widget w{99, "external"};
    EXPECT_THROW(reg.erase(&w), std::out_of_range);
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

TEST(ordered_registry, ClearEmptiesContainer)
{
    auto reg = make_reg();
    reg.insert({1, "a"});
    reg.insert({2, "b"});
    reg.clear();
    EXPECT_TRUE(reg.empty());
    EXPECT_EQ(reg.size(), 0u);
}

// ---------------------------------------------------------------------------
// insertion order preservation
// ---------------------------------------------------------------------------

TEST(ordered_registry, InsertionOrderPreserved)
{
    auto reg = make_reg();
    for(int i : {5, 3, 8, 1})
        reg.insert({i, ""});

    std::vector<int> order;
    for(const auto& w : reg)
        order.push_back(w.id);

    EXPECT_EQ(order, (std::vector<int>{5, 3, 8, 1}));
}

// ---------------------------------------------------------------------------
// functor key_func (not just free-function pointer)
// ---------------------------------------------------------------------------

TEST(ordered_registry, FunctorKeyFunc)
{
    struct get_id {
        int operator()(const widget& w) const { return w.id; }
    };
    vpp::ordered_registry<widget, int, get_id> reg;
    EXPECT_TRUE(reg.insert({7, "seven"}).second);
    EXPECT_TRUE(reg.contains(7));
    EXPECT_EQ(reg.at(7).name, "seven");
}

// ---------------------------------------------------------------------------
// CTAD deduction guide
// ---------------------------------------------------------------------------

TEST(ordered_registry, CTADFromFreeFunctionPointer)
{
    vpp::ordered_registry reg{&widget_id};
    static_assert(std::is_same_v<decltype(reg), vpp::ordered_registry<widget, int, int(*)(const widget&)>>);
    EXPECT_TRUE(reg.insert({1, "one"}).second);
}
