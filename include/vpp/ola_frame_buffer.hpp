/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <cassert>
#include <cstddef>
#include <cstring>
#include <functional>
#include <vector>

namespace vpp
{

// Frames a continuous push-based stream into overlapping blocks for use in
// overlap-add (OLA) DSP pipelines (e.g. STFT processing).
//
// Two callbacks are invoked by the buffer:
//
//   process_fn  — called when a full input block is ready.
//                 Receives a process_context describing the input block and a
//                 zeroed scratch output buffer to write results into. The
//                 buffer accumulates scratch into the OLA output buffer after
//                 the callback returns.
//
//   output_fn   — called when one hop's worth of accumulated output is ready.
//                 Receives an output_context with a pointer to the interleaved
//                 output data [num_frames * num_out_channels].
//                 num_frames == hop_size, except on the final flush() drain.
//
// Data layout: interleaved frames (sample[0][ch0], sample[0][ch1], ...,
//              sample[1][ch0], ...) — i.e. WAV order.
//
// All memory is allocated at construction; push() and flush() are zero-alloc.
// Not thread-safe; the caller is responsible for synchronisation if needed.
template <typename T = float>
class ola_frame_buffer final
{
public:
    struct process_context
    {
        const T *input;
        T *scratch;
        std::size_t block_size;
        std::size_t num_in_channels;
        std::size_t num_out_channels;

        std::size_t num_input_samples() const noexcept { return block_size * num_in_channels; }
        std::size_t num_output_samples() const noexcept { return block_size * num_out_channels; }

        // Interleaved (frame, channel) access into the input block.
        const T &in(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < block_size && ch < num_in_channels);
            return input[frame * num_in_channels + ch];
        }

        // Interleaved (frame, channel) access into the scratch output block.
        // const because the callback receives the context by const&; scratch
        // is a non-const pointer, so writing through it remains well-formed.
        T &out(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < block_size && ch < num_out_channels);
            return scratch[frame * num_out_channels + ch];
        }
    };

    struct output_context
    {
        const T *output;
        std::size_t num_frames;
        std::size_t num_out_channels;

        std::size_t num_samples() const noexcept { return num_frames * num_out_channels; }

        // Interleaved (frame, channel) access into the output hop.
        const T &out(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < num_frames && ch < num_out_channels);
            return output[frame * num_out_channels + ch];
        }
    };

    using process_fn_t = std::function<void(const process_context &)>;
    using output_fn_t = std::function<void(const output_context &)>;

    // block_size       : number of frames per analysis block (the FFT size).
    // hop_size         : number of frames between successive blocks.
    //                    Must satisfy 0 < hop_size <= block_size.
    // num_in_channels  : number of interleaved channels in the input stream.
    // num_out_channels : number of interleaved channels in the output stream.
    // process_fn       : called with each completed input block + scratch buffer.
    // output_fn        : called when each hop of output is ready to consume.
    ola_frame_buffer(
        std::size_t block_size,
        std::size_t hop_size,
        std::size_t num_in_channels,
        std::size_t num_out_channels,
        process_fn_t process_fn,
        output_fn_t output_fn) :
        m_block_size(block_size),
        m_hop_size(hop_size),
        m_num_in_ch(num_in_channels),
        m_num_out_ch(num_out_channels),
        m_process_fn(std::move(process_fn)),
        m_output_fn(std::move(output_fn)),
        m_input_buf(block_size * num_in_channels, T{}),
        m_scratch_buf(block_size * num_out_channels, T{}),
        m_ola_buf(block_size * num_out_channels, T{}),
        m_frames_in_buf(0)
    {
        assert(hop_size > 0 && hop_size <= block_size);
        assert(num_in_channels > 0);
        assert(num_out_channels > 0);
    }

    // Push num_frames interleaved frames into the buffer.
    // Fires process_fn and output_fn internally as blocks/hops complete.
    // Does not allocate.
    void push(const T *frames, std::size_t num_frames)
    {
        for(std::size_t offset = 0; offset < num_frames;)
        {
            const std::size_t space = m_block_size - m_frames_in_buf;
            const std::size_t to_copy =
                (num_frames - offset) < space ? (num_frames - offset) : space;

            std::memcpy(
                m_input_buf.data() + m_frames_in_buf * m_num_in_ch,
                frames + offset * m_num_in_ch,
                to_copy * m_num_in_ch * sizeof(T));

            offset += to_copy;
            m_frames_in_buf += to_copy;

            if(m_frames_in_buf == m_block_size)
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
            std::memset(
                m_input_buf.data() + m_frames_in_buf * m_num_in_ch,
                0,
                (m_block_size - m_frames_in_buf) * m_num_in_ch * sizeof(T));
            m_frames_in_buf = m_block_size;
            process_block();
        }

        std::fill(m_ola_buf.begin(), m_ola_buf.end(), T{});
    }

    // ---- accessors ----

    std::size_t block_size() const noexcept { return m_block_size; }
    std::size_t hop_size() const noexcept { return m_hop_size; }
    std::size_t num_in_channels() const noexcept { return m_num_in_ch; }
    std::size_t num_out_channels() const noexcept { return m_num_out_ch; }

private:
    void process_block()
    {
        std::fill(m_scratch_buf.begin(), m_scratch_buf.end(), T{});

        m_process_fn(
            {m_input_buf.data(), m_scratch_buf.data(), m_block_size, m_num_in_ch, m_num_out_ch});

        // Accumulate scratch into the OLA buffer.
        const std::size_t ola_samples = m_block_size * m_num_out_ch;
        for(std::size_t i = 0; i < ola_samples; ++i)
            m_ola_buf[i] += m_scratch_buf[i];

        // Emit only the front hop — it is the only region guaranteed to have
        // received all of its OLA contributions. Positions beyond hop_size may
        // still receive additions from the next block.
        m_output_fn({m_ola_buf.data(), m_hop_size, m_num_out_ch});

        // Shift the overlap tail to the front for the next block.
        const std::size_t remaining = m_block_size - m_hop_size;
        if(remaining > 0)
        {
            std::memmove(
                m_ola_buf.data(),
                m_ola_buf.data() + m_hop_size * m_num_out_ch,
                remaining * m_num_out_ch * sizeof(T));
        }
        std::memset(
            m_ola_buf.data() + remaining * m_num_out_ch,
            0,
            m_hop_size * m_num_out_ch * sizeof(T));

        // Slide the input buffer forward by one hop (retain the overlap tail).
        if(remaining > 0)
        {
            std::memmove(
                m_input_buf.data(),
                m_input_buf.data() + m_hop_size * m_num_in_ch,
                remaining * m_num_in_ch * sizeof(T));
        }

        m_frames_in_buf = remaining;
    }

    const std::size_t m_block_size;
    const std::size_t m_hop_size;
    const std::size_t m_num_in_ch;
    const std::size_t m_num_out_ch;

    process_fn_t m_process_fn;
    output_fn_t m_output_fn;

    std::vector<T> m_input_buf;
    std::vector<T> m_scratch_buf;
    std::vector<T> m_ola_buf;

    std::size_t m_frames_in_buf{0};
};

} // namespace vpp
