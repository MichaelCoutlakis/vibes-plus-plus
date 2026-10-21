/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/mat_view_arma.hpp>
#include <gtest/gtest.h>
#include <type_traits>

namespace {

float total(vpp::mat_view<const float> v)
{
    float total = 0;
    for(std::size_t c = 0; c < v.cols(); ++c)
        for(float x : v.col(c))
            total += x;
    return total;
}

void scale(vpp::mat_view<float> v, float k)
{
    for(std::size_t c = 0; c < v.cols(); ++c)
        for(float &x : v.col(c))
            x *= k;
}

arma::fmat make_fmat(arma::uword rows, arma::uword cols)
{
    arma::fmat m(rows, cols);
    for(arma::uword i = 0; i < m.n_elem; ++i)
        m(i) = static_cast<float>(i);
    return m;
}

} // namespace

// ---------------------------------------------------------------------------
// Mat, Col, Row
// ---------------------------------------------------------------------------

TEST(mat_view_arma, ViewsWholeMatrix)
{
    arma::fmat m = make_fmat(3, 4);
    vpp::mat_view<const float> v = m;
    EXPECT_EQ(v.data(), m.memptr());
    EXPECT_EQ(v.rows(), 3u);
    EXPECT_EQ(v.cols(), 4u);
    EXPECT_EQ(v.col_stride(), 3u);
    EXPECT_EQ(v(2, 3), m(2, 3));
    EXPECT_EQ(total(m), arma::accu(m));
}

TEST(mat_view_arma, WritesThroughToMatrix)
{
    arma::fmat m = make_fmat(2, 2);
    scale(m, 2.f);
    EXPECT_EQ(m(1, 1), 6.f);
}

TEST(mat_view_arma, ConstnessFollowsMatrix)
{
    static_assert(std::is_convertible_v<arma::fmat &, vpp::mat_view<float>>);
    static_assert(std::is_convertible_v<const arma::fmat &, vpp::mat_view<const float>>);
    static_assert(!std::is_convertible_v<const arma::fmat &, vpp::mat_view<float>>);
    static_assert(!std::is_convertible_v<arma::fmat &, vpp::mat_view<double>>);
    static_assert(std::is_same_v<decltype(vpp::as_view(std::declval<const arma::mat &>())), vpp::mat_view<const double>>);
}

TEST(mat_view_arma, RejectsTemporaryMatrix)
{
    static_assert(!std::is_convertible_v<arma::fmat &&, vpp::mat_view<const float>>);
    static_assert(!std::is_convertible_v<arma::fmat, vpp::mat_view<const float>>);
}

TEST(mat_view_arma, ViewsColRowAndFixed)
{
    arma::fvec c = {1.f, 2.f, 3.f};
    arma::frowvec r = {1.f, 2.f, 3.f};
    arma::fmat::fixed<2, 2> f = {{1.f, 2.f}, {3.f, 4.f}};
    EXPECT_EQ(vpp::as_view(c).rows(), 3u);
    EXPECT_EQ(vpp::as_view(c).cols(), 1u);
    EXPECT_EQ(vpp::as_view(r).rows(), 1u);
    EXPECT_EQ(vpp::as_view(r).cols(), 3u);
    EXPECT_EQ(total(c), 6.f);
    EXPECT_EQ(total(r), 6.f);
    EXPECT_EQ(total(f), 10.f);
}

TEST(mat_view_arma, ExpressionsDoNotConvert)
{
    using expr = decltype(std::declval<arma::fmat &>() * 2.f);
    static_assert(!std::is_convertible_v<expr, vpp::mat_view<const float>>);
}

// ---------------------------------------------------------------------------
// subview: strided, temporaries accepted, no copy
// ---------------------------------------------------------------------------

TEST(mat_view_arma, RowsSubviewIsStridedAndUncopied)
{
    arma::fmat m = make_fmat(6, 2); // 6 frames, 2 channels
    auto check = [&](vpp::mat_view<const float> v) {
        EXPECT_EQ(v.data(), m.colptr(0) + 4);
        EXPECT_EQ(v.rows(), 2u);
        EXPECT_EQ(v.cols(), 2u);
        EXPECT_EQ(v.col_stride(), 6u);
        EXPECT_EQ(v(1, 1), m(5, 1));
    };
    check(m.rows(4, 5));
    check(m.tail_rows(2));
    EXPECT_EQ(total(m.rows(4, 5)), arma::accu(m.rows(4, 5)));
}

TEST(mat_view_arma, SubmatWritesThrough)
{
    arma::fmat m(4, 4, arma::fill::ones);
    scale(m.submat(1, 1, 2, 2), 3.f);
    EXPECT_EQ(arma::accu(m), 12.f + 4 * 3.f);
    EXPECT_EQ(m(1, 1), 3.f);
    EXPECT_EQ(m(0, 0), 1.f);
}

TEST(mat_view_arma, SubviewColAndRow)
{
    arma::fmat m = make_fmat(3, 4);
    vpp::mat_view<const float> c = m.col(2);
    vpp::mat_view<const float> r = m.row(1);
    EXPECT_EQ(c.data(), m.colptr(2));
    EXPECT_EQ(c.rows(), 3u);
    EXPECT_EQ(r.cols(), 4u);
    EXPECT_EQ(r.col_stride(), 3u);
    EXPECT_EQ(r(0, 3), m(1, 3));
}

TEST(mat_view_arma, SubviewOfConstMatrixIsReadOnly)
{
    const arma::fmat m = make_fmat(3, 3);
    static_assert(std::is_same_v<decltype(vpp::as_view(m.rows(0, 1))), vpp::mat_view<const float>>);
    static_assert(!std::is_convertible_v<decltype(m.rows(0, 1)), vpp::mat_view<float>>);
    EXPECT_EQ(total(m.rows(0, 1)), arma::accu(m.rows(0, 1)));
}

// ---------------------------------------------------------------------------
// Cube
// ---------------------------------------------------------------------------

TEST(mat_view_arma, ViewsCubeAndSubcube)
{
    arma::fcube q(3, 4, 5);
    for(arma::uword i = 0; i < q.n_elem; ++i)
        q(i) = static_cast<float>(i);

    vpp::cube_view<const float> v = q;
    EXPECT_EQ(v.slice_stride(), 12u);
    EXPECT_EQ(v(2, 3, 4), q(2, 3, 4));

    auto check = [&](vpp::cube_view<const float> s) {
        EXPECT_EQ(s.rows(), 2u);
        EXPECT_EQ(s.cols(), 2u);
        EXPECT_EQ(s.slices(), 3u);
        EXPECT_EQ(s.col_stride(), 3u);
        EXPECT_EQ(s.slice_stride(), 12u);
        EXPECT_EQ(s(1, 1, 2), q(2, 2, 3));
    };
    check(q.subcube(1, 1, 1, 2, 2, 3));

    // A Cube slice is a Mat, so it views as a mat_view.
    vpp::mat_view<const float> s = q.slice(2);
    EXPECT_EQ(s(1, 2), q(1, 2, 2));
}
