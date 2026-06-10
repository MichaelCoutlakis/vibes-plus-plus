/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/functional.hpp>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

static int add3(int a, int b, int c) { return a + b + c; }

struct accumulator {
    int base = 0;
    int add(int x) const { return base + x; }
    int scale_add(int factor, int x) { return base + factor * x; }
};

// A move-only argument, to verify perfect forwarding at the call site.
struct move_only {
    int value;
    explicit move_only(int v) : value(v) {}
    move_only(move_only &&) = default;
    move_only &operator=(move_only &&) = default;
    move_only(const move_only &) = delete;
    move_only &operator=(const move_only &) = delete;
};

static int take_move_only(move_only m) { return m.value; }

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

TEST(bind_front, ForwardsAllArgumentsToFreeFunction)
{
    auto g = vpp::bind_front(&add3);
    EXPECT_EQ(g(1, 2, 3), 6);
}

TEST(bind_front, BindsLeadingArgumentsToFreeFunction)
{
    auto g = vpp::bind_front(&add3, 10);
    EXPECT_EQ(g(2, 3), 15);

    auto h = vpp::bind_front(&add3, 10, 20);
    EXPECT_EQ(h(3), 33);
}

// ---------------------------------------------------------------------------
// Member functions (the motivating use case)
// ---------------------------------------------------------------------------

TEST(bind_front, BindsMemberFunctionToObjectPointer)
{
    accumulator acc{100};
    auto        cb = vpp::bind_front(&accumulator::add, &acc);
    EXPECT_EQ(cb(5), 105);
}

TEST(bind_front, BindsMemberFunctionWithExtraBoundArgument)
{
    accumulator acc{100};
    auto        cb = vpp::bind_front(&accumulator::scale_add, &acc, 3);
    EXPECT_EQ(cb(4), 112); // 100 + 3 * 4
}

// ---------------------------------------------------------------------------
// Functors / lambdas
// ---------------------------------------------------------------------------

TEST(bind_front, BindsLambda)
{
    auto lambda = [](int a, int b) { return a * b; };
    auto g      = vpp::bind_front(lambda, 6);
    EXPECT_EQ(g(7), 42);
}

// ---------------------------------------------------------------------------
// Forwarding semantics
// ---------------------------------------------------------------------------

TEST(bind_front, PerfectlyForwardsMoveOnlyCallArgument)
{
    auto g = vpp::bind_front(&take_move_only);
    EXPECT_EQ(g(move_only{7}), 7);
}

TEST(bind_front, BoundArgumentsAreDecayCopied)
{
    // A bound rvalue/temporary must be stored, not dangle. Calling repeatedly
    // must keep working off the stored copy.
    auto g = vpp::bind_front(&add3, 1, 2);
    EXPECT_EQ(g(3), 6);
    EXPECT_EQ(g(10), 13);
}

TEST(bind_front, ReferenceWrapperBindsByReference)
{
    int  counter = 0;
    auto bump    = [](int &c) { ++c; };
    auto g       = vpp::bind_front(bump, std::ref(counter));
    g();
    g();
    EXPECT_EQ(counter, 2);
}

TEST(bind_front, RvalueWrapperMovesBoundArgument)
{
    auto g      = vpp::bind_front(&take_move_only, move_only{42});
    int  result = std::move(g)(); // invoke as rvalue -> bound arg is moved out
    EXPECT_EQ(result, 42);
}

// ---------------------------------------------------------------------------
// const correctness
// ---------------------------------------------------------------------------

TEST(bind_front, ConstWrapperIsInvocable)
{
    accumulator      acc{1};
    const auto       cb = vpp::bind_front(&accumulator::add, &acc);
    EXPECT_EQ(cb(9), 10);
}
