/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <functional>
#include <vector>

namespace vpp
{

// Element ordering of the input/output sample buffers. The interleaved/planar
// and frame_wise/channel_wise spellings are aliases for the same two layouts.
enum class ola_layout
{
    interleaved, // frame-wise / WAV order: s0c0, s0c1, s1c0, ...
    frame_wise = interleaved,
    planar, // channel-wise: c0s0, c0s1, ..., c1s0, ...
    channel_wise = planar,
};

// Frames a continuous push-based stream into overlapping blocks for use in
// overlap-add (OLA) DSP pipelines (e.g. STFT processing).
//
// Two callbacks are invoked by the buffer:
//
//   process_fn  — called when a full input block is ready. Receives a
//                 process_context exposing the input block and a zeroed scratch
//                 output block to write results into. The buffer accumulates
//                 scratch into the OLA output buffer after the callback returns.
//
//   output_fn   — called when one hop's worth of accumulated output is ready.
//                 Receives an output_context exposing the output hop.
//                 num_frames() == hop_size, except on the final flush() drain.
//
// Layout: the Layout template parameter selects interleaved (frame-wise, WAV
// order) or planar (channel-wise) ordering. It applies uniformly to the input
// stream passed to push(), the buffers exposed by the contexts, and the output
// hop. The at_*() element accessors and the data_*() pointers all honour it.
//
// All memory is allocated at construction; push() and flush() are zero-alloc.
// Not thread-safe; the caller is responsible for synchronisation if needed.
template <typename T = float, ola_layout Layout = ola_layout::interleaved>
class ola_frame_buffer final
{
public:
    struct process_context
    {
        process_context(
            const T *input,
            T *scratch,
            std::size_t num_frames,
            std::size_t num_in_channels,
            std::size_t num_out_channels) noexcept :
            m_input(input),
            m_scratch(scratch),
            m_num_frames(num_frames),
            m_num_in_ch(num_in_channels),
            m_num_out_ch(num_out_channels)
        {
        }

        // Raw pointer to the input block. The element order (interleaved
        // frame-wise vs planar channel-wise) follows the Layout parameter.
        const T *data_in() const noexcept { return m_input; }

        // Raw pointer to the zeroed scratch output block to write results into.
        // Same layout rules as data_in().
        T *data_out() const noexcept { return m_scratch; }

        std::size_t num_frames() const noexcept { return m_num_frames; }
        std::size_t num_in_channels() const noexcept { return m_num_in_ch; }
        std::size_t num_out_channels() const noexcept { return m_num_out_ch; }
        std::size_t num_in_samples() const noexcept { return m_num_frames * m_num_in_ch; }
        std::size_t num_out_samples() const noexcept { return m_num_frames * m_num_out_ch; }

        // (frame, channel) element access into the input block. Layout-aware.
        const T &at_in(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < m_num_frames && ch < m_num_in_ch);
            if constexpr(Layout == ola_layout::interleaved)
                return m_input[frame * m_num_in_ch + ch];
            else
                return m_input[ch * m_num_frames + frame];
        }

        // (frame, channel) element access into the scratch output block.
        // const because the context is delivered by const&; m_scratch is a
        // non-const pointer, so writing through it remains well-formed.
        T &at_out(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < m_num_frames && ch < m_num_out_ch);
            if constexpr(Layout == ola_layout::interleaved)
                return m_scratch[frame * m_num_out_ch + ch];
            else
                return m_scratch[ch * m_num_frames + frame];
        }

    private:
        const T *m_input;
        T *m_scratch;
        std::size_t m_num_frames;
        std::size_t m_num_in_ch;
        std::size_t m_num_out_ch;
    };

    struct output_context
    {
        output_context(
            const T *output,
            std::size_t num_frames,
            std::size_t num_out_channels) noexcept :
            m_output(output),
            m_num_frames(num_frames),
            m_num_out_ch(num_out_channels)
        {
        }

        // Raw pointer to the output hop. The element order (interleaved
        // frame-wise vs planar channel-wise) follows the Layout parameter.
        const T *data() const noexcept { return m_output; }

        std::size_t num_frames() const noexcept { return m_num_frames; }
        std::size_t num_out_channels() const noexcept { return m_num_out_ch; }
        std::size_t num_samples() const noexcept { return m_num_frames * m_num_out_ch; }

        // (frame, channel) element access into the output hop. Layout-aware.
        const T &at(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < m_num_frames && ch < m_num_out_ch);
            if constexpr(Layout == ola_layout::interleaved)
                return m_output[frame * m_num_out_ch + ch];
            else
                return m_output[ch * m_num_frames + frame];
        }

    private:
        const T *m_output;
        std::size_t m_num_frames;
        std::size_t m_num_out_ch;
    };

    using process_fn_t = std::function<void(const process_context &)>;
    using output_fn_t = std::function<void(const output_context &)>;

    // frame_size       : number of frames per analysis block (the FFT size).
    // hop_size         : number of frames between successive blocks.
    //                    Must satisfy 0 < hop_size <= frame_size.
    // num_in_channels  : number of channels in the input stream.
    // num_out_channels : number of channels in the output stream.
    // process_fn       : called with each completed input block + scratch buffer.
    // output_fn        : called when each hop of output is ready to consume.
    ola_frame_buffer(
        std::size_t frame_size,
        std::size_t hop_size,
        std::size_t num_in_channels,
        std::size_t num_out_channels,
        process_fn_t process_fn,
        output_fn_t output_fn) :
        m_frame_size(frame_size),
        m_hop_size(hop_size),
        m_num_in_ch(num_in_channels),
        m_num_out_ch(num_out_channels),
        m_process_fn(std::move(process_fn)),
        m_output_fn(std::move(output_fn)),
        m_input_buf(frame_size * num_in_channels, T{}),
        m_scratch_buf(frame_size * num_out_channels, T{}),
        m_ola_buf(frame_size * num_out_channels, T{}),
        m_hop_buf(Layout == ola_layout::planar ? hop_size * num_out_channels : 0, T{}),
        m_frames_in_buf(0)
    {
        assert(hop_size > 0 && hop_size <= frame_size);
        assert(num_in_channels > 0);
        assert(num_out_channels > 0);
    }

    // Push num_frames frames (in the configured layout) into the buffer. Data
    // is handled a whole frame at a time: `frames` must address
    // num_frames * num_in_channels samples; partial frames are not representable.
    // Fires process_fn and output_fn internally as blocks/hops complete.
    // Does not allocate.
    void push(const T *frames, std::size_t num_frames)
    {
        for(std::size_t offset = 0; offset < num_frames;)
        {
            const std::size_t space = m_frame_size - m_frames_in_buf;
            const std::size_t to_copy = std::min(num_frames - offset, space);

            move_frames(
                frames,
                num_frames,
                offset,
                m_input_buf.data(),
                m_frame_size,
                m_frames_in_buf,
                to_copy,
                m_num_in_ch);

            offset += to_copy;
            m_frames_in_buf += to_copy;

            if(m_frames_in_buf == m_frame_size)
                process_block();
        }
    }

    // Flushes any buffered input by zero-padding to a full block, then drains
    // any remaining accumulated output. Call once at end-of-stream.
    // Apply analysis/synthesis windows inside process_fn if needed.
    void flush()
    {
        if(m_frames_in_buf > 0)
        {
            zero_frames(
                m_input_buf.data(),
                m_frame_size,
                m_frames_in_buf,
                m_frame_size - m_frames_in_buf,
                m_num_in_ch);
            m_frames_in_buf = m_frame_size;
            process_block();
        }

        std::fill(m_ola_buf.begin(), m_ola_buf.end(), T{});
    }

    // ---- accessors ----

    std::size_t frame_size() const noexcept { return m_frame_size; }
    std::size_t hop_size() const noexcept { return m_hop_size; }
    std::size_t num_in_channels() const noexcept { return m_num_in_ch; }
    std::size_t num_out_channels() const noexcept { return m_num_out_ch; }
    static constexpr ola_layout layout() noexcept { return Layout; }

private:
    // Move `count` frames for every channel from src into dst. `src` holds
    // `src_frames` frames per channel, `dst` holds `dst_frames`. Layout-aware;
    // uses memmove so in-place overlapping shifts are safe.
    static void move_frames(
        const T *src,
        std::size_t src_frames,
        std::size_t src_start,
        T *dst,
        std::size_t dst_frames,
        std::size_t dst_start,
        std::size_t count,
        std::size_t num_ch) noexcept
    {
        if(count == 0)
            return;
        if constexpr(Layout == ola_layout::interleaved)
        {
            (void)src_frames;
            (void)dst_frames;
            std::memmove(
                dst + dst_start * num_ch,
                src + src_start * num_ch,
                count * num_ch * sizeof(T));
        }
        else
        {
            for(std::size_t c = 0; c < num_ch; ++c)
                std::memmove(
                    dst + c * dst_frames + dst_start,
                    src + c * src_frames + src_start,
                    count * sizeof(T));
        }
    }

    // Zero `count` frames for every channel starting at frame `start`, in a
    // buffer holding `frames_per_ch` frames per channel. Layout-aware.
    static void zero_frames(
        T *buf,
        std::size_t frames_per_ch,
        std::size_t start,
        std::size_t count,
        std::size_t num_ch) noexcept
    {
        if(count == 0)
            return;
        if constexpr(Layout == ola_layout::interleaved)
        {
            (void)frames_per_ch;
            std::memset(buf + start * num_ch, 0, count * num_ch * sizeof(T));
        }
        else
        {
            for(std::size_t c = 0; c < num_ch; ++c)
                std::memset(buf + c * frames_per_ch + start, 0, count * sizeof(T));
        }
    }

    void emit_front_hop()
    {
        if constexpr(Layout == ola_layout::interleaved)
        {
            // The front hop is the leading contiguous region of the OLA buffer.
            m_output_fn(output_context{m_ola_buf.data(), m_hop_size, m_num_out_ch});
        }
        else
        {
            // Each channel's leading hop is strided by frame_size; compact them
            // into the contiguous planar hop buffer before emitting.
            for(std::size_t c = 0; c < m_num_out_ch; ++c)
                std::memcpy(
                    m_hop_buf.data() + c * m_hop_size,
                    m_ola_buf.data() + c * m_frame_size,
                    m_hop_size * sizeof(T));
            m_output_fn(output_context{m_hop_buf.data(), m_hop_size, m_num_out_ch});
        }
    }

    void process_block()
    {
        std::fill(m_scratch_buf.begin(), m_scratch_buf.end(), T{});

        m_process_fn(
            process_context{
                m_input_buf.data(),
                m_scratch_buf.data(),
                m_frame_size,
                m_num_in_ch,
                m_num_out_ch
            });

        // Accumulate scratch into the OLA buffer (layout-agnostic, same dims).
        const std::size_t ola_samples = m_frame_size * m_num_out_ch;
        for(std::size_t i = 0; i < ola_samples; ++i)
            m_ola_buf[i] += m_scratch_buf[i];

        // Emit only the front hop — it is the only region guaranteed to have
        // received all of its OLA contributions. Positions beyond hop_size may
        // still receive additions from the next block.
        emit_front_hop();

        const std::size_t remaining = m_frame_size - m_hop_size;

        // Shift the overlap tail to the front and zero the vacated hop.
        move_frames(
            m_ola_buf.data(),
            m_frame_size,
            m_hop_size,
            m_ola_buf.data(),
            m_frame_size,
            0,
            remaining,
            m_num_out_ch);
        zero_frames(m_ola_buf.data(), m_frame_size, remaining, m_hop_size, m_num_out_ch);

        // Slide the input buffer forward by one hop (retain the overlap tail).
        move_frames(
            m_input_buf.data(),
            m_frame_size,
            m_hop_size,
            m_input_buf.data(),
            m_frame_size,
            0,
            remaining,
            m_num_in_ch);

        m_frames_in_buf = remaining;
    }

    const std::size_t m_frame_size;
    const std::size_t m_hop_size;
    const std::size_t m_num_in_ch;
    const std::size_t m_num_out_ch;

    process_fn_t m_process_fn;
    output_fn_t m_output_fn;

    std::vector<T> m_input_buf;
    std::vector<T> m_scratch_buf;
    std::vector<T> m_ola_buf;
    std::vector<T> m_hop_buf; // planar compaction scratch; empty when interleaved

    std::size_t m_frames_in_buf{0};
};

} // namespace vpp
