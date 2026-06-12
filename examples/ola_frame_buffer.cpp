/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <cstddef>

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <numeric>
#include <vpp/ola_frame_buffer.hpp>

int main()
{
    size_t fft_bins{4U};
    size_t hop_size = fft_bins / 2U;
    size_t input_channels{3U};
    size_t output_channels{1U};

    using my_ola = vpp::ola_frame_buffer<int>;

    auto do_process = [](const my_ola::process_context &ctx)
    {
        std::vector<int> v(ctx.data_in(), ctx.data_in() + ctx.num_in_samples());
        fmt::print(" Process block with input: {}\n", fmt::join(v, ", "));
        // Sum all channels framewise to output:
        for(size_t frame = 0U; frame != ctx.num_frames(); ++frame)
            for(size_t channel = 0U; channel != ctx.num_in_channels(); ++channel)
                ctx.at_out(frame, 0) += ctx.at_in(frame, channel);
    };
    auto do_output = [](const my_ola::output_context &ctx)
    {
        std::vector<int> v(ctx.data(), ctx.data() + ctx.num_samples());
        fmt::print("Process output block: {}\n", fmt::join(v, ", "));
    };

    my_ola ola_buffer(fft_bins, hop_size, input_channels, output_channels, do_process, do_output);

    size_t num_hops{5U}, num_frames{num_hops * input_channels};
    std::vector<int> input_data(num_frames * input_channels);
    std::iota(input_data.begin(), input_data.end(), 0);

    ola_buffer.push(input_data.data(), num_frames);

    return 0;
}