/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/keyed_vector.hpp>
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

using widget_kv = vpp::keyed_vector<widget, int, int (*)(const widget&)>;

static widget_kv make_kv()
{
    return widget_kv{&widget_id};
}

// ---------------------------------------------------------------------------
// insert / size / empty
// ---------------------------------------------------------------------------

TEST(keyed_vector, InsertIncreasesSize)
{
    auto kv = make_kv();
    EXPECT_TRUE(kv.empty());
    auto [it, inserted] = kv.insert({1, "alpha"});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->id, 1);
    EXPECT_EQ(kv.size(), 1u);
    EXPECT_FALSE(kv.empty());
}

TEST(keyed_vector, InsertDuplicateReturnsFalseAndExistingIterator)
{
    auto kv = make_kv();
    kv.insert({42, "first"});
    auto [it, inserted] = kv.insert({42, "second"});
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->name, "first");   // existing element, not overwritten
    EXPECT_EQ(kv.size(), 1u);
}

// ---------------------------------------------------------------------------
// insert_or_assign
// ---------------------------------------------------------------------------

TEST(keyed_vector, InsertOrAssignInsertsWhenAbsent)
{
    auto kv = make_kv();
    auto [it, inserted] = kv.insert_or_assign({7, "seven"});
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->name, "seven");
    EXPECT_EQ(kv.size(), 1u);
}

TEST(keyed_vector, InsertOrAssignOverwritesExisting)
{
    auto kv = make_kv();
    kv.insert({7, "seven"});
    auto [it, inserted] = kv.insert_or_assign({7, "SEVEN"});
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->name, "SEVEN");
    EXPECT_EQ(kv.size(), 1u);
}

TEST(keyed_vector, InsertOrAssignPreservesOrder)
{
    auto kv = make_kv();
    kv.insert({1, "one"});
    kv.insert({2, "two"});
    kv.insert({3, "three"});
    kv.insert_or_assign({2, "TWO"});
    std::vector<int> order;
    for(const auto& w : kv)
        order.push_back(w.id);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    EXPECT_EQ(kv.at(2).name, "TWO");
}

// ---------------------------------------------------------------------------
// find_ptr / find / contains
// ---------------------------------------------------------------------------

TEST(keyed_vector, FindPtrReturnsNullOnMiss)
{
    auto kv = make_kv();
    kv.insert({1, "a"});
    EXPECT_EQ(kv.find_ptr(99), nullptr);
}

TEST(keyed_vector, FindPtrReturnsCorrectElement)
{
    auto kv = make_kv();
    kv.insert({7, "seven"});
    const widget* p = kv.find_ptr(7);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->name, "seven");
}

TEST(keyed_vector, FindIteratorEqualsEndOnMiss)
{
    auto kv = make_kv();
    EXPECT_EQ(kv.find(0), kv.end());
}

TEST(keyed_vector, Contains)
{
    auto kv = make_kv();
    kv.insert({3, "three"});
    EXPECT_TRUE(kv.contains(3));
    EXPECT_FALSE(kv.contains(4));
}

// ---------------------------------------------------------------------------
// operator[] and at(key)
// ---------------------------------------------------------------------------

TEST(keyed_vector, AtKeyReturnsCorrectElement)
{
    auto kv = make_kv();
    kv.insert({5, "five"});
    EXPECT_EQ(kv.at(5).name, "five");
}

TEST(keyed_vector, AtKeyThrowsOnMiss)
{
    auto kv = make_kv();
    EXPECT_THROW(kv.at(99), std::out_of_range);
}

TEST(keyed_vector, AtKeyConstOverload)
{
    auto kv = make_kv();
    kv.insert({5, "five"});
    const auto& ckv = kv;
    EXPECT_EQ(ckv.at(5).name, "five");
    EXPECT_THROW(ckv.at(99), std::out_of_range);
}

// ---------------------------------------------------------------------------
// erase by key
// ---------------------------------------------------------------------------

TEST(keyed_vector, EraseByKeyRemovesElement)
{
    auto kv = make_kv();
    kv.insert({1, "a"});
    kv.insert({2, "b"});
    kv.erase(1);
    EXPECT_EQ(kv.size(), 1u);
    EXPECT_EQ(kv.find_ptr(1), nullptr);
    EXPECT_NE(kv.find_ptr(2), nullptr);
}

TEST(keyed_vector, EraseByMissingKeyIsNoOp)
{
    auto kv = make_kv();
    kv.insert({1, "a"});
    kv.erase(99);
    EXPECT_EQ(kv.size(), 1u);
}

// ---------------------------------------------------------------------------
// erase by pointer
// ---------------------------------------------------------------------------

TEST(keyed_vector, EraseByPtrRemovesElement)
{
    auto kv = make_kv();
    kv.insert({1, "a"});
    kv.insert({2, "b"});
    widget* p = kv.find_ptr(1);
    ASSERT_NE(p, nullptr);
    kv.erase(p);
    EXPECT_EQ(kv.size(), 1u);
    EXPECT_FALSE(kv.contains(1));
}

TEST(keyed_vector, EraseByPtrOutOfRangeThrows)
{
    auto kv = make_kv();
    kv.insert({1, "a"});
    widget w{99, "external"};
    EXPECT_THROW(kv.erase(&w), std::out_of_range);
}

// ---------------------------------------------------------------------------
// clear
// ---------------------------------------------------------------------------

TEST(keyed_vector, ClearEmptiesContainer)
{
    auto kv = make_kv();
    kv.insert({1, "a"});
    kv.insert({2, "b"});
    kv.clear();
    EXPECT_TRUE(kv.empty());
    EXPECT_EQ(kv.size(), 0u);
}

// ---------------------------------------------------------------------------
// insertion order preservation
// ---------------------------------------------------------------------------

TEST(keyed_vector, InsertionOrderPreserved)
{
    auto kv = make_kv();
    for(int i : {5, 3, 8, 1})
        kv.insert({i, ""});

    std::vector<int> order;
    for(const auto& w : kv)
        order.push_back(w.id);

    EXPECT_EQ(order, (std::vector<int>{5, 3, 8, 1}));
}

// ---------------------------------------------------------------------------
// functor key_func (not just free-function pointer)
// ---------------------------------------------------------------------------

TEST(keyed_vector, FunctorKeyFunc)
{
    struct get_id {
        int operator()(const widget& w) const { return w.id; }
    };
    vpp::keyed_vector<widget, int, get_id> kv;
    EXPECT_TRUE(kv.insert({7, "seven"}).second);
    EXPECT_TRUE(kv.contains(7));
    EXPECT_EQ(kv.at(7).name, "seven");
}

// ---------------------------------------------------------------------------
// CTAD deduction guides
// ---------------------------------------------------------------------------

TEST(keyed_vector, CTADFromFreeFunctionPointer)
{
    vpp::keyed_vector kv{&widget_id};
    static_assert(std::is_same_v<decltype(kv), vpp::keyed_vector<widget, int, int(*)(const widget&)>>);
    EXPECT_TRUE(kv.insert({1, "one"}).second);
}

TEST(keyed_vector, CTADFromPointerToDataMember)
{
    vpp::keyed_vector kv{&widget::id};
    static_assert(std::is_same_v<decltype(kv), vpp::keyed_vector<widget, int, int widget::*>>);
    EXPECT_TRUE(kv.insert({5, "five"}).second);
    EXPECT_TRUE(kv.contains(5));
    EXPECT_EQ(kv.at(5).name, "five");
}

// ---------------------------------------------------------------------------
// pointer-to-member-function as key
// ---------------------------------------------------------------------------

TEST(keyed_vector, PointerToMemberFunction)
{
    struct named {
        int         id;
        std::string label;
        int get_id() const { return id; }
    };
    vpp::keyed_vector kv{&named::get_id};
    static_assert(std::is_same_v<decltype(kv), vpp::keyed_vector<named, int, int(named::*)() const>>);
    EXPECT_TRUE(kv.insert({3, "three"}).second);
    EXPECT_TRUE(kv.contains(3));
}
