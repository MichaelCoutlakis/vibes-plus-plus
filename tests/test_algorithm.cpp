/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/algorithm.hpp>
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <complex>
#include <forward_list>
#include <list>
#include <ranges>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// index_max / index_min
// ---------------------------------------------------------------------------

TEST(index_max, ReturnsIndexOfLargest)
{
    std::vector<double> v{3.0, 7.5, -2.0, 4.1};
    EXPECT_EQ(vpp::index_max(v), 1u);
}

TEST(index_max, ReturnsFirstOnTie)
{
    std::vector<int> v{1, 9, 3, 9};
    EXPECT_EQ(vpp::index_max(v), 1u);
}

TEST(index_max, ComparesByProjection)
{
    std::vector<int> v{3, -8, 5};
    EXPECT_EQ(vpp::index_max(v, vpp::abs), 1u);
    EXPECT_EQ(vpp::index_max(v, [](int x) { return -x; }), 1u);
}

TEST(index_max, ProjectsThroughDataMember)
{
    struct bin { int id; double power; };
    std::vector<bin> v{{0, 0.5}, {1, 2.0}, {2, 1.0}};
    EXPECT_EQ(vpp::index_max(v, &bin::power), 1u);
}

TEST(index_max, MeasuresComplexByMagnitude)
{
    std::vector<std::complex<double>> spectrum{{1, 1}, {0, -3}, {2, 0}};
    EXPECT_EQ(vpp::index_max(spectrum, vpp::abs), 1u);
}

TEST(index_max, AcceptsNonRandomAccessRanges)
{
    std::list<int> l{4, 1, 7, 2};
    std::forward_list<int> fl{4, 1, 7, 2};
    EXPECT_EQ(vpp::index_max(l), 2u);
    EXPECT_EQ(vpp::index_max(fl), 2u);
}

TEST(index_max, AcceptsTemporariesAndViews)
{
    EXPECT_EQ(vpp::index_max(std::vector{1, 5, 2}), 1u);

    // The index is a position in the view, not the underlying container.
    std::vector<int> v{1, 5, 2, 0};
    EXPECT_EQ(vpp::index_max(v | std::views::reverse), 2u);
    EXPECT_EQ(vpp::index_max(v | std::views::drop(2)), 0u);
}

TEST(index_max, IsConstexpr)
{
    constexpr std::array a{2, 8, 3};
    static_assert(vpp::index_max(a) == 1);
    static_assert(vpp::index_min(a) == 0);
}

TEST(index_max, ThrowsOnEmpty)
{
    std::vector<int> v;
    EXPECT_THROW(vpp::index_max(v), std::invalid_argument);
}

TEST(index_min, ReturnsIndexOfSmallest)
{
    std::vector<double> v{3.0, 7.5, -2.0, 4.1};
    EXPECT_EQ(vpp::index_min(v), 2u);
}

TEST(index_min, ReturnsFirstOnTie)
{
    std::vector<int> v{4, 1, 3, 1};
    EXPECT_EQ(vpp::index_min(v), 1u);
}

TEST(index_min, ComparesByProjection)
{
    std::vector<int> v{-3, 8, 2};
    EXPECT_EQ(vpp::index_min(v, vpp::abs), 2u);
    EXPECT_EQ(vpp::index_min(v, vpp::distance_to(7)), 1u);
}

TEST(index_min, ThrowsOnEmpty)
{
    std::vector<int> v;
    EXPECT_THROW(vpp::index_min(v), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// index_nearest
// ---------------------------------------------------------------------------

TEST(index_nearest, ReturnsIndexOfClosest)
{
    std::vector<double> v{3.0, 7.5, -2.0, 7.5, 4.1};
    EXPECT_EQ(vpp::index_nearest(v, 4.0), 4u);
    EXPECT_EQ(vpp::index_nearest(v, -10.0), 2u);
    EXPECT_EQ(vpp::index_nearest(v, 100.0), 1u);
}

TEST(index_nearest, PrefersLowerIndexWhenEquidistant)
{
    std::vector<int> v{6, 2, 4};
    EXPECT_EQ(vpp::index_nearest(v, 5), 0u); // 6 and 4 are both 1 away
}

TEST(index_nearest, DoesNotWrapForUnsigned)
{
    // A naive |e - x| on unsigned would make 3u - 4u huge and pick 10u.
    std::vector<unsigned> v{10u, 3u, 7u};
    EXPECT_EQ(vpp::index_nearest(v, 4u), 1u);
}

TEST(index_nearest, AcceptsMixedArithmeticTypes)
{
    std::vector<int> v{0, 10, 20};
    EXPECT_EQ(vpp::index_nearest(v, 12.5), 1u);
}

TEST(index_nearest, MeasuresComplexByMagnitude)
{
    using c = std::complex<double>;
    std::vector<c> v{{0, 0}, {1, 1}, {5, 5}};
    EXPECT_EQ(vpp::index_nearest(v, c{1.2, 0.9}), 1u);
}

namespace {

// A type with no arithmetic or abs of its own: distances come through the
// freq() member-function projection.
class bin
{
public:
    bin(int id, double freq) :
        m_id(id),
        m_freq(freq)
    {
    }

    int id() const { return m_id; }
    double freq() const { return m_freq; }

private:
    int m_id;
    double m_freq;
};

} // namespace

TEST(index_nearest, ProjectsThroughMemberFunction)
{
    std::vector<bin> bins{{0, 880.0}, {1, 220.0}, {2, 450.0}, {3, 430.0}};
    EXPECT_EQ(vpp::index_nearest(bins, 445.0, &bin::freq), 2u);
    EXPECT_EQ(vpp::index_nearest(bins, 440.0, &bin::freq), 2u); // tie with 430 goes lower
    EXPECT_EQ(vpp::index_nearest(bins, 0.0, &bin::freq), 1u);
}

TEST(index_nearest, CustomMetricViaIndexMin)
{
    // Nearest in pitch (log frequency) differs from nearest in Hz.
    std::vector<bin> bins{{0, 200.0}, {1, 600.0}};
    const double target = 380.0;
    EXPECT_EQ(vpp::index_nearest(bins, target, &bin::freq), 0u);
    EXPECT_EQ(
        vpp::index_min(bins, [&](const bin &b) { return std::abs(std::log2(b.freq() / target)); }),
        1u);
}

TEST(index_nearest, ThrowsOnEmpty)
{
    std::vector<double> v;
    EXPECT_THROW(vpp::index_nearest(v, 1.0), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// index_nearest_sorted
// ---------------------------------------------------------------------------

TEST(index_nearest_sorted, ReturnsIndexOfClosest)
{
    std::vector<double> v{0, 100, 200, 300};
    EXPECT_EQ(vpp::index_nearest_sorted(v, 140.0), 1u);
    EXPECT_EQ(vpp::index_nearest_sorted(v, 160.0), 2u);
    EXPECT_EQ(vpp::index_nearest_sorted(v, 200.0), 2u);
}

TEST(index_nearest_sorted, ClampsOutsideTheRange)
{
    std::vector<int> v{10, 20, 30};
    EXPECT_EQ(vpp::index_nearest_sorted(v, -5), 0u);
    EXPECT_EQ(vpp::index_nearest_sorted(v, 99), 2u);
}

TEST(index_nearest_sorted, PrefersLowerIndexWhenEquidistant)
{
    std::vector<int> v{10, 20, 30};
    EXPECT_EQ(vpp::index_nearest_sorted(v, 15), 0u);
    EXPECT_EQ(vpp::index_nearest_sorted(v, 25), 1u);
}

TEST(index_nearest_sorted, ReturnsFirstOfRepeatedValues)
{
    std::vector<int> v{1, 5, 5, 5, 9};
    EXPECT_EQ(vpp::index_nearest_sorted(v, 4), 1u); // nearest below the run
    EXPECT_EQ(vpp::index_nearest_sorted(v, 5), 1u);
    EXPECT_EQ(vpp::index_nearest_sorted(v, 6), 1u); // nearest above the run
    EXPECT_EQ(vpp::index_nearest_sorted(v, 7), 1u); // tie with 9 goes lower
}

TEST(index_nearest_sorted, HandlesSingleElement)
{
    std::vector<int> v{42};
    EXPECT_EQ(vpp::index_nearest_sorted(v, 0), 0u);
    EXPECT_EQ(vpp::index_nearest_sorted(v, 100), 0u);
}

TEST(index_nearest_sorted, DoesNotWrapForUnsigned)
{
    std::vector<unsigned> v{2u, 3u, 10u};
    EXPECT_EQ(vpp::index_nearest_sorted(v, 4u), 1u);
}

TEST(index_nearest_sorted, AgreesWithIndexNearest)
{
    std::vector<int> v{-7, -3, -3, 0, 2, 2, 2, 8, 11, 20};
    for (int x = -15; x <= 25; ++x)
        EXPECT_EQ(vpp::index_nearest_sorted(v, x), vpp::index_nearest(v, x)) << "x = " << x;
}

TEST(index_nearest_sorted, ProjectsThroughMemberFunction)
{
    std::vector<bin> bins{{7, 100.0}, {3, 200.0}, {5, 200.0}, {9, 400.0}};
    EXPECT_EQ(vpp::index_nearest_sorted(bins, 120.0, &bin::freq), 0u);
    EXPECT_EQ(vpp::index_nearest_sorted(bins, 250.0, &bin::freq), 1u); // first of the 200s
    EXPECT_EQ(vpp::index_nearest_sorted(bins, 300.0, &bin::freq), 1u); // tie goes lower
    EXPECT_EQ(vpp::index_nearest_sorted(bins, 999.0, &bin::freq), 3u);

    for (double x = 0.0; x <= 500.0; x += 12.5)
    {
        EXPECT_EQ(
            vpp::index_nearest_sorted(bins, x, &bin::freq),
            vpp::index_nearest(bins, x, &bin::freq))
            << "x = " << x;
    }
}

TEST(index_nearest_sorted, AcceptsBidirectionalRanges)
{
    std::list<int> l{10, 20, 30};
    EXPECT_EQ(vpp::index_nearest_sorted(l, 22), 1u);
}

TEST(index_nearest_sorted, IsConstexpr)
{
    constexpr std::array a{1, 4, 9, 16};
    static_assert(vpp::index_nearest_sorted(a, 10) == 2);
    static_assert(vpp::index_nearest(a, 10) == 2);
}

TEST(index_nearest_sorted, ThrowsOnEmpty)
{
    std::vector<int> v;
    EXPECT_THROW(vpp::index_nearest_sorted(v, 1), std::invalid_argument);
}
