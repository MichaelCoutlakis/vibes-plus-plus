/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <numeric>
#include <vector>
#include <vpp/ola_frame_buffer.hpp>

using namespace testing;
using buf_t = vpp::ola_frame_buffer<float>;
using buf_planar = vpp::ola_frame_buffer<float, vpp::ola_layout::channel_wise>;

// Generic so it converts to either layout's process_fn_t. Copies the whole
// block bytewise, which is layout-agnostic (in/out share the same ordering).
static const auto passthrough = [](const auto &ctx)
{ std::memcpy(ctx.data_out(), ctx.data_in(), ctx.num_in_samples() * sizeof(float)); };

static auto make_collector(std::vector<float> &out)
{
    return [&out](const auto &ctx)
    { out.insert(out.end(), ctx.data(), ctx.data() + ctx.num_samples()); };
}

// ---------------------------------------------------------------------------

TEST(ola_frame_buffer, ProcessCallCount)
{
    int n = 0;
    buf_t buf(4, 2, 1, 1, [&](const auto &) { ++n; }, [](const auto &) { });
    std::vector<float> frames(8);
    buf.push(frames.data(), 8);
    EXPECT_EQ(n, 3);
}

TEST(ola_frame_buffer, OutputCallCount)
{
    // One hop emitted per block: 3 blocks => 3 output calls.
    int n = 0;
    buf_t buf(4, 2, 1, 1, [](const auto &) { }, [&](const auto &) { ++n; });
    std::vector<float> frames(8);
    buf.push(frames.data(), 8);
    EXPECT_EQ(n, 3);
}

TEST(ola_frame_buffer, PassthroughNoOverlap)
{
    std::vector<float> out;
    buf_t buf(4, 4, 1, 1, passthrough, make_collector(out));
    std::vector<float> in = {1, 2, 3, 4, 5, 6, 7, 8};
    buf.push(in.data(), in.size());
    EXPECT_THAT(out, Pointwise(FloatEq(), in));
}

TEST(ola_frame_buffer, PassthroughWithOverlapScales)
{
    // block=4, hop=2 => 50% overlap. Each steady-state output sample is
    // accumulated from 2 blocks, so the value is 2.0f.
    // Block 0 emits hop [0,2) with only one contribution (1.0f) — ramp-up.
    // From block 1 onwards the front hop has two contributions (2.0f).
    std::vector<float> out;
    buf_t buf(4, 2, 1, 1, passthrough, make_collector(out));
    std::vector<float> in(16, 1.0f);
    buf.push(in.data(), in.size());
    // Skip the first hop (ramp-up), then all samples should be 2.0f.
    EXPECT_THAT(std::vector<float>(out.begin() + 2, out.end()), Each(FloatEq(2.0f)));
}

TEST(ola_frame_buffer, PushChunkedMatchesSinglePush)
{
    std::vector<float> in(32);
    std::iota(in.begin(), in.end(), 0.0f);

    std::vector<float> single_out, chunked_out;

    auto make_buf = [&](std::vector<float> &o)
    { return buf_t(8, 4, 1, 1, passthrough, make_collector(o)); };

    auto buf_s = make_buf(single_out);
    buf_s.push(in.data(), in.size());

    auto buf_c = make_buf(chunked_out);
    for(std::size_t i = 0; i < in.size(); i += 3)
        buf_c.push(in.data() + i, std::min<std::size_t>(3, in.size() - i));

    EXPECT_THAT(chunked_out, Pointwise(FloatEq(), single_out));
}

TEST(ola_frame_buffer, FlushDrainsPartialBlock)
{
    int n = 0;
    buf_t buf(4, 4, 1, 1, [&](const auto &) { ++n; }, [](const auto &) { });
    std::vector<float> in(6, 1.0f);
    buf.push(in.data(), 6);
    EXPECT_EQ(n, 1);
    buf.flush();
    EXPECT_EQ(n, 2);
}

TEST(ola_frame_buffer, MultiChannelPassthrough)
{
    std::vector<float> out;
    buf_t buf(4, 4, 2, 2, passthrough, make_collector(out));
    std::vector<float> in = {1, 2, 3, 4, 5, 6, 7, 8};
    buf.push(in.data(), 4);
    EXPECT_THAT(out, Pointwise(FloatEq(), in));
}

TEST(ola_frame_buffer, ContextAccessorsApplyGain)
{
    // Use the (frame, channel) accessors to apply a per-sample gain.
    // 2 channels, block=4, hop=4 (no overlap) so output == input * gain.
    constexpr float gain = 3.0f;
    std::vector<float> out;
    buf_t buf(
        4,
        4,
        2,
        2,
        [](const buf_t::process_context &ctx)
        {
            for(std::size_t f = 0; f < ctx.num_frames(); ++f)
                for(std::size_t ch = 0; ch < ctx.num_out_channels(); ++ch)
                    ctx.at_out(f, ch) = ctx.at_in(f, ch) * gain;
        },
        make_collector(out));

    std::vector<float> in = {1, 2, 3, 4, 5, 6, 7, 8};
    buf.push(in.data(), 4);

    std::vector<float> expected;
    for(float v : in)
        expected.push_back(v * gain);
    EXPECT_THAT(out, Pointwise(FloatEq(), expected));
}

// ---------------------------------------------------------------------------
// planar (channel_wise) layout
// ---------------------------------------------------------------------------

TEST(ola_frame_buffer, PlanarPassthroughNoOverlap)
{
    // 2 channels, block=4, hop=4. Planar input: [c0:0..3][c1:0..3]. Passthrough
    // preserves planar layout, so output matches input exactly.
    std::vector<float> out;
    buf_planar buf(4, 4, 2, 2, passthrough, make_collector(out));
    std::vector<float> in = {1, 2, 3, 4, 5, 6, 7, 8};
    buf.push(in.data(), 4);
    EXPECT_THAT(out, Pointwise(FloatEq(), in));
}

TEST(ola_frame_buffer, PlanarAccessorsApplyGain)
{
    // The (frame, channel) accessors hide the planar stride. block=4, hop=4.
    constexpr float gain = 2.0f;
    std::vector<float> out;
    buf_planar buf(
        4,
        4,
        2,
        2,
        [](const buf_planar::process_context &ctx)
        {
            for(std::size_t f = 0; f < ctx.num_frames(); ++f)
                for(std::size_t ch = 0; ch < ctx.num_out_channels(); ++ch)
                    ctx.at_out(f, ch) = ctx.at_in(f, ch) * gain;
        },
        make_collector(out));

    std::vector<float> in = {1, 2, 3, 4, 5, 6, 7, 8}; // planar, 2ch x 4 frames
    buf.push(in.data(), 4);

    std::vector<float> expected;
    for(float v : in)
        expected.push_back(v * gain);
    EXPECT_THAT(out, Pointwise(FloatEq(), expected));
}

TEST(ola_frame_buffer, PlanarOverlapScales)
{
    // block=4, hop=2, 2 channels, planar. Steady-state OLA scales by 2.
    std::vector<float> out;
    buf_planar buf(4, 2, 2, 2, passthrough, make_collector(out));
    std::vector<float> in(2 * 16, 1.0f); // 2 channels x 16 frames, planar
    buf.push(in.data(), 16);
    // Skip the first hop per channel (ramp-up = 2 frames x 2 ch = 4 samples).
    EXPECT_THAT(std::vector<float>(out.begin() + 4, out.end()), Each(FloatEq(2.0f)));
}
