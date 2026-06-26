/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#define VPP_DEFINE_SHORT_MACROS
#include <vpp/macros.hpp>
#include <gtest/gtest.h>
#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Test fixtures
// ---------------------------------------------------------------------------

namespace {

// Records how its argument was passed: as an lvalue or an rvalue.
enum class passed_as { lvalue, rvalue };

passed_as inspect(const int &) { return passed_as::lvalue; }
passed_as inspect(int &&) { return passed_as::rvalue; }

// A perfect-forwarding wrapper built on VPP_FWD: whatever value category the
// caller supplies must reach inspect() unchanged.
template <typename T>
passed_as forward_through(T &&arg)
{
    return inspect(VPP_FWD(arg));
}

struct move_probe {
    bool moved_from = false;
    move_probe()    = default;
    move_probe(move_probe &&other) noexcept { other.moved_from = true; }
    move_probe(const move_probe &) = default;
};

} // namespace

// ---------------------------------------------------------------------------
// VPP_FWD
// ---------------------------------------------------------------------------

TEST(VppFwd, PreservesLvalueCategory)
{
    int x = 0;
    EXPECT_EQ(forward_through(x), passed_as::lvalue);
}

TEST(VppFwd, PreservesRvalueCategory)
{
    EXPECT_EQ(forward_through(42), passed_as::rvalue);
}

// VPP_FWD on a forwarding-reference parameter produces an xvalue when the
// parameter binds an rvalue, so the underlying object is moved from.
TEST(VppFwd, MovesWhenForwardingAnRvalue)
{
    auto sink = [](auto &&p) { return decltype(p)(VPP_FWD(p)); };
    move_probe src;
    move_probe dst = sink(std::move(src));
    (void)dst;
    EXPECT_TRUE(src.moved_from);
}

// ---------------------------------------------------------------------------
// Short-macro opt-in alias
// ---------------------------------------------------------------------------

// FWD is just the prefixed macro: on a forwarding reference it preserves the
// caller's value category.
TEST(ShortMacros, FwdAliasIsDefinedAndExpandsToPrefixed)
{
    auto fwd_ref = [](auto &&p) { return inspect(FWD(p)); };
    int  x       = 0;
    EXPECT_EQ(fwd_ref(x), passed_as::lvalue);  // lvalue preserved
    EXPECT_EQ(fwd_ref(42), passed_as::rvalue); // rvalue preserved
}
