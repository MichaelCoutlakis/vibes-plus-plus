/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/mat_view.hpp>
#include <gtest/gtest.h>
#include <numeric>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// Test fixtures: a minimal owning matrix and a borrowed block of one, hooked
// into mat_view through mat_view_traits (as an adapter header would).
// ---------------------------------------------------------------------------

namespace {

struct toy_mat
{
    toy_mat(std::size_t r, std::size_t c) : rows(r), cols(c), mem(r * c)
    {
        std::iota(mem.begin(), mem.end(), 0.f);
    }
    std::size_t rows, cols;
    std::vector<float> mem;
};

struct toy_block
{
    float *ptr;
    std::size_t rows, cols, stride;
};

struct toy_cube
{
    toy_cube(std::size_t r, std::size_t c, std::size_t s) : rows(r), cols(c), slices(s), mem(r * c * s)
    {
        std::iota(mem.begin(), mem.end(), 0.f);
    }
    std::size_t rows, cols, slices;
    std::vector<float> mem;
};

// Element value at (r, c) of a toy_mat filled by iota.
float at(const toy_mat &m, std::size_t r, std::size_t c) { return static_cast<float>(r + c * m.rows); }

} // namespace

template <>
struct vpp::mat_view_traits<toy_mat>
{
    static constexpr bool borrowed = false;
    static auto *data(auto &m) noexcept { return m.mem.data(); }
    static std::size_t rows(const auto &m) noexcept { return m.rows; }
    static std::size_t cols(const auto &m) noexcept { return m.cols; }
    static std::size_t col_stride(const auto &m) noexcept { return m.rows; }
};

template <>
struct vpp::mat_view_traits<toy_block>
{
    static constexpr bool borrowed = true;
    static float *data(const toy_block &b) noexcept { return b.ptr; }
    static std::size_t rows(const toy_block &b) noexcept { return b.rows; }
    static std::size_t cols(const toy_block &b) noexcept { return b.cols; }
    static std::size_t col_stride(const toy_block &b) noexcept { return b.stride; }
};

template <>
struct vpp::cube_view_traits<toy_cube>
{
    static constexpr bool borrowed = false;
    static auto *data(auto &m) noexcept { return m.mem.data(); }
    static std::size_t rows(const auto &m) noexcept { return m.rows; }
    static std::size_t cols(const auto &m) noexcept { return m.cols; }
    static std::size_t slices(const auto &m) noexcept { return m.slices; }
    static std::size_t col_stride(const auto &m) noexcept { return m.rows; }
    static std::size_t slice_stride(const auto &m) noexcept { return m.rows * m.cols; }
};

namespace {

float sum(vpp::mat_view<const float> v)
{
    float total = 0;
    for(std::size_t c = 0; c < v.cols(); ++c)
        for(float x : v.col(c))
            total += x;
    return total;
}

void fill(vpp::mat_view<float> v, float value)
{
    for(std::size_t c = 0; c < v.cols(); ++c)
        for(float &x : v.col(c))
            x = value;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction and conversions
// ---------------------------------------------------------------------------

TEST(mat_view, DefaultIsEmpty)
{
    vpp::mat_view<float> v;
    EXPECT_EQ(v.data(), nullptr);
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.size(), 0u);
}

TEST(mat_view, ContiguousConstructorSetsColStrideToRows)
{
    std::vector<float> mem(12);
    vpp::mat_view v(mem.data(), 3, 4);
    static_assert(std::is_same_v<decltype(v), vpp::mat_view<float>>);
    EXPECT_EQ(v.rows(), 3u);
    EXPECT_EQ(v.cols(), 4u);
    EXPECT_EQ(v.col_stride(), 3u);
    EXPECT_EQ(v.size(), 12u);
    EXPECT_TRUE(v.is_contiguous());
}

TEST(mat_view, ConvertsToConstButNotBack)
{
    static_assert(std::is_convertible_v<vpp::mat_view<float>, vpp::mat_view<const float>>);
    static_assert(!std::is_convertible_v<vpp::mat_view<const float>, vpp::mat_view<float>>);
    static_assert(!std::is_convertible_v<vpp::mat_view<float>, vpp::mat_view<double>>);
}

TEST(mat_view, IsTriviallyCopyable)
{
    static_assert(std::is_trivially_copyable_v<vpp::mat_view<float>>);
    static_assert(std::is_trivially_copyable_v<vpp::cube_view<const double>>);
}

TEST(mat_view, ImplicitlyConvertsFromAdaptedMatrix)
{
    toy_mat m(3, 4);
    EXPECT_EQ(sum(m), 66.f); // 0 + 1 + ... + 11
    fill(m, 2.f);
    EXPECT_EQ(sum(m), 24.f);
}

TEST(mat_view, ConstnessFollowsSource)
{
    static_assert(std::is_convertible_v<toy_mat &, vpp::mat_view<float>>);
    static_assert(std::is_convertible_v<toy_mat &, vpp::mat_view<const float>>);
    static_assert(std::is_convertible_v<const toy_mat &, vpp::mat_view<const float>>);
    static_assert(!std::is_convertible_v<const toy_mat &, vpp::mat_view<float>>);
    static_assert(!std::is_convertible_v<toy_mat &, vpp::mat_view<double>>);
}

TEST(mat_view, RejectsTemporaryOfOwningType)
{
    // A view of a temporary owning matrix would dangle.
    static_assert(!std::is_convertible_v<toy_mat &&, vpp::mat_view<const float>>);
    static_assert(!std::is_constructible_v<vpp::mat_view<const float>, toy_mat>);
}

TEST(mat_view, AcceptsTemporaryOfBorrowedType)
{
    toy_mat m(4, 3);
    EXPECT_EQ(sum(toy_block{m.mem.data() + 1, 2, 3, 4}), 1.f + 2 + 5 + 6 + 9 + 10);
}

TEST(mat_view, UnadaptedTypesDoNotConvert)
{
    static_assert(!std::is_convertible_v<std::vector<float> &, vpp::mat_view<float>>);
    static_assert(!std::is_convertible_v<toy_cube &, vpp::mat_view<float>>);
}

TEST(mat_view, DeductionGuideAndAsViewFollowConstness)
{
    toy_mat m(2, 2);
    const toy_mat &cm = m;
    static_assert(std::is_same_v<decltype(vpp::mat_view{m}), vpp::mat_view<float>>);
    static_assert(std::is_same_v<decltype(vpp::mat_view{cm}), vpp::mat_view<const float>>);
    static_assert(std::is_same_v<decltype(vpp::as_view(m)), vpp::mat_view<float>>);
    static_assert(std::is_same_v<decltype(vpp::as_view(cm)), vpp::mat_view<const float>>);
    EXPECT_EQ(vpp::as_view(m).data(), m.mem.data());
}

// ---------------------------------------------------------------------------
// Element access
// ---------------------------------------------------------------------------

TEST(mat_view, ElementAccessIsColumnMajor)
{
    toy_mat m(3, 4);
    vpp::mat_view<float> v = m;
    for(std::size_t c = 0; c < 4; ++c)
        for(std::size_t r = 0; r < 3; ++r)
            EXPECT_EQ(v(r, c), at(m, r, c));
#if defined(__cpp_multidimensional_subscript)
    EXPECT_EQ((v[2, 1]), at(m, 2, 1));
#endif
}

TEST(mat_view, WritesThroughConstView)
{
    toy_mat m(2, 2);
    const vpp::mat_view<float> v = m;
    v(1, 1) = 42.f;
    EXPECT_EQ(m.mem[3], 42.f);
}

TEST(mat_view, ColIsContiguousSpan)
{
    toy_mat m(3, 4);
    vpp::mat_view<const float> v = m;
    auto col = v.col(2);
    ASSERT_EQ(col.size(), 3u);
    EXPECT_EQ(col.data(), v.col_ptr(2));
    EXPECT_EQ(col[0], at(m, 0, 2));
    EXPECT_EQ(col[2], at(m, 2, 2));
}

// ---------------------------------------------------------------------------
// Sub-views
// ---------------------------------------------------------------------------

TEST(mat_view, BlockKeepsParentColStride)
{
    toy_mat m(5, 4);
    vpp::mat_view<float> b = vpp::mat_view<float>(m).block(1, 2, 3, 2);
    EXPECT_EQ(b.rows(), 3u);
    EXPECT_EQ(b.cols(), 2u);
    EXPECT_EQ(b.col_stride(), 5u);
    EXPECT_FALSE(b.is_contiguous());
    for(std::size_t c = 0; c < 2; ++c)
        for(std::size_t r = 0; r < 3; ++r)
            EXPECT_EQ(b(r, c), at(m, r + 1, c + 2));
}

TEST(mat_view, RowsFromSkipsLeadingFramesOfEveryChannel)
{
    toy_mat m(6, 2); // 6 frames, 2 channels
    auto tail = vpp::as_view(m).rows_from(4);
    EXPECT_EQ(tail.rows(), 2u);
    EXPECT_EQ(tail.cols(), 2u);
    EXPECT_EQ(tail(0, 0), at(m, 4, 0));
    EXPECT_EQ(tail(1, 1), at(m, 5, 1));
    EXPECT_EQ(sum(tail), at(m, 4, 0) + at(m, 5, 0) + at(m, 4, 1) + at(m, 5, 1));
}

TEST(mat_view, ColRangeOfContiguousViewIsContiguous)
{
    toy_mat m(3, 5);
    auto v = vpp::as_view(m);
    EXPECT_TRUE(v.col_range(1, 3).is_contiguous());
    EXPECT_TRUE(v.cols_from(2).is_contiguous());
    EXPECT_EQ(v.cols_from(2).cols(), 3u);
    EXPECT_EQ(v.cols_from(2)(0, 0), at(m, 0, 2));
    EXPECT_FALSE(v.row_range(0, 2).is_contiguous());
}

TEST(mat_view, RowIsOneByColsView)
{
    toy_mat m(3, 4);
    auto r = vpp::as_view(m).row(1);
    EXPECT_EQ(r.rows(), 1u);
    EXPECT_EQ(r.cols(), 4u);
    for(std::size_t c = 0; c < 4; ++c)
        EXPECT_EQ(r(0, c), at(m, 1, c));
}

TEST(mat_view, SingleColumnBlockIsContiguous)
{
    toy_mat m(4, 4);
    EXPECT_TRUE(vpp::as_view(m).block(1, 2, 2, 1).is_contiguous());
}

TEST(mat_view, IsConstexpr)
{
    constexpr auto check = [] {
        float mem[6] = {0, 1, 2, 3, 4, 5};
        vpp::mat_view v(mem, 2, 3);
        return v.block(1, 1, 1, 2)(0, 1);
    };
    static_assert(check() == 5.f);
}

// ---------------------------------------------------------------------------
// cube_view
// ---------------------------------------------------------------------------

TEST(cube_view, ContiguousConstructorSetsStrides)
{
    std::vector<double> mem(24);
    vpp::cube_view v(mem.data(), 2, 3, 4);
    EXPECT_EQ(v.col_stride(), 2u);
    EXPECT_EQ(v.slice_stride(), 6u);
    EXPECT_EQ(v.size(), 24u);
    EXPECT_TRUE(v.is_contiguous());
}

TEST(cube_view, ElementAccessAndSlices)
{
    toy_cube m(2, 3, 4);
    vpp::cube_view<const float> v = m;
    EXPECT_EQ(v(1, 2, 3), 1.f + 2 * 2 + 3 * 6);
    auto s = v.slice(2);
    static_assert(std::is_same_v<decltype(s), vpp::mat_view<const float>>);
    EXPECT_EQ(s.rows(), 2u);
    EXPECT_EQ(s.cols(), 3u);
    EXPECT_EQ(s(1, 0), v(1, 0, 2));
    EXPECT_EQ(sum(v.slice(0)), 0.f + 1 + 2 + 3 + 4 + 5);
}

TEST(cube_view, BlockKeepsParentStrides)
{
    toy_cube m(4, 3, 5);
    auto b = vpp::as_view(m).block(1, 1, 2, 2, 2, 3);
    static_assert(std::is_same_v<decltype(b), vpp::cube_view<float>>);
    EXPECT_EQ(b.col_stride(), 4u);
    EXPECT_EQ(b.slice_stride(), 12u);
    EXPECT_FALSE(b.is_contiguous());
    EXPECT_EQ(b(0, 0, 0), (vpp::as_view(m)(1, 1, 2)));
    EXPECT_EQ(b(1, 1, 2), (vpp::as_view(m)(2, 2, 4)));
}

TEST(cube_view, SliceRangeOfContiguousCubeIsContiguous)
{
    toy_cube m(2, 2, 4);
    auto v = vpp::as_view(m);
    EXPECT_TRUE(v.slice_range(1, 2).is_contiguous());
    EXPECT_FALSE(v.row_range(0, 1).is_contiguous());
    EXPECT_FALSE(v.col_range(0, 1).is_contiguous());
    EXPECT_EQ(v.col_range(1, 1)(1, 0, 3), v(1, 1, 3));
}

TEST(cube_view, ConvertsToConstButNotBack)
{
    static_assert(std::is_convertible_v<vpp::cube_view<float>, vpp::cube_view<const float>>);
    static_assert(!std::is_convertible_v<vpp::cube_view<const float>, vpp::cube_view<float>>);
    static_assert(!std::is_convertible_v<const toy_cube &, vpp::cube_view<float>>);
    static_assert(!std::is_convertible_v<toy_cube &&, vpp::cube_view<const float>>);
}
