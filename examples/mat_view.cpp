/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <algorithm>
#include <fmt/base.h>
#include <type_traits>
#include <vpp/mat_view_arma.hpp>

// Functions take a mat_view, so their interfaces do not depend on Armadillo
// (only the caller includes <vpp/mat_view_arma.hpp>), and a block of a matrix
// is passed without copying it.

// Peak absolute sample per channel, for audio held as frames x channels.
void print_peaks(vpp::mat_view<const float> audio)
{
    for(std::size_t c = 0; c < audio.cols(); ++c)
    {
        float peak = 0;
        for(float x : audio.col(c))
            peak = std::max(peak, x < 0 ? -x : x);
        fmt::println("  channel {}: peak {}", c, peak);
    }
}

void apply_gain(vpp::mat_view<float> audio, float gain)
{
    for(std::size_t c = 0; c < audio.cols(); ++c)
        for(float &x : audio.col(c))
            x *= gain;
}

// A function template cannot deduce T through the implicit conversion;
// vpp::as_view at the call site supplies the view type.
template <typename T>
std::remove_cv_t<T> sum(vpp::mat_view<T> m)
{
    std::remove_cv_t<T> total{};
    for(std::size_t c = 0; c < m.cols(); ++c)
        for(T x : m.col(c))
            total += x;
    return total;
}

// Back to Armadillo is a copy: a block's columns are not adjacent in memory,
// which an arma::Mat requires.
arma::fmat to_fmat(vpp::mat_view<const float> v)
{
    arma::fmat m(v.rows(), v.cols());
    for(std::size_t c = 0; c < v.cols(); ++c)
        std::ranges::copy(v.col(c), m.colptr(c));
    return m;
}

int main()
{
    arma::fmat audio(8, 3); // 8 frames, 3 channels
    for(arma::uword i = 0; i < audio.n_elem; ++i)
        audio(i) = static_cast<float>(i);

    fmt::println("whole matrix:");
    print_peaks(audio);

    // Halve frames 4.. of channels 1 and 2 only: a strided block, no copy.
    apply_gain(audio.submat(4, 1, 7, 2), 0.5f);

    fmt::println("after halving the tail of channels 1-2:");
    print_peaks(audio);

    fmt::println("frames 0-3 only:");
    print_peaks(audio.rows(0, 3));

    fmt::println("channel 2 only:");
    print_peaks(audio.col(2));

    // A view is cheap to slice further, e.g. skipping the first 2 frames.
    vpp::mat_view<const float> view = audio;
    fmt::println("frames 2.., channels 0-1:");
    print_peaks(view.rows_from(2).col_range(0, 2));

    fmt::println("sum of channel 0: {}", sum(vpp::as_view(audio.col(0))));

    const arma::fmat tail = to_fmat(view.rows_from(6));
    fmt::println("copied tail is {} x {}", tail.n_rows, tail.n_cols);

    // Does not compile: the temporary matrix would die before the view.
    //   print_peaks(arma::fmat(4, 4));
    return 0;
}
