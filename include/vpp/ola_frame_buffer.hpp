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

/// Element ordering of the input/output sample buffers.
enum class ola_layout
{
    interleaved, ///< frame-wise / WAV order: s0c0, s0c1, s1c0, ...
    frame_wise = interleaved,
    planar, ///< channel-wise: c0s0, c0s1, ..., c1s0, ...
    channel_wise = planar,
};

/// Frames a continuous push-based stream into overlapping blocks for
/// overlap-add (OLA) DSP pipelines such as STFT processing.
///
/// Two callbacks drive the buffer. `process_fn` is called when a full input
/// block is ready, receiving a process_context that exposes the input block and
/// a zeroed output block to write results into; the buffer overlap-adds that
/// output into its accumulation buffer after the callback returns. `output_fn`
/// is called when one hop of accumulated output is ready, receiving an
/// output_context (`num_frames() == hop_size`, except on the final flush() drain).
///
/// All working storage is allocated at construction; push() and flush() are
/// zero-alloc. Not thread-safe; the caller synchronises if needed.
///
/// @tparam T       Sample type; must support `+=` and value-initialisation.
/// @tparam Layout  Element ordering of every sample buffer (see ola_layout).
///                 Applies uniformly to the push() stream, the context buffers,
///                 and the emitted hop; the at_*() accessors and data_*()
///                 pointers all honour it.
template <typename T = float, ola_layout Layout = ola_layout::interleaved>
class ola_frame_buffer final
{
public:
    struct process_context
    {
        process_context(
            const T *input,
            T *output,
            std::size_t num_frames,
            std::size_t num_in_channels,
            std::size_t num_out_channels) noexcept :
            m_input(input),
            m_output(output),
            m_num_frames(num_frames),
            m_num_in_ch(num_in_channels),
            m_num_out_ch(num_out_channels)
        {
        }

        /// Pointer to the input block; element order follows the Layout parameter.
        const T *data_in() const noexcept { return m_input; }

        /// Pointer to the zeroed output block to write results into; same layout
        /// as data_in().
        T *data_out() const noexcept { return m_output; }

        std::size_t num_frames() const noexcept { return m_num_frames; }
        std::size_t num_in_channels() const noexcept { return m_num_in_ch; }
        std::size_t num_out_channels() const noexcept { return m_num_out_ch; }
        std::size_t num_in_samples() const noexcept { return m_num_frames * m_num_in_ch; }
        std::size_t num_out_samples() const noexcept { return m_num_frames * m_num_out_ch; }

        /// (frame, channel) element access into the input block; honours the layout.
        const T &at_in(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < m_num_frames && ch < m_num_in_ch);
            if constexpr(Layout == ola_layout::interleaved)
                return m_input[frame * m_num_in_ch + ch];
            else
                return m_input[ch * m_num_frames + frame];
        }

        /// Mutable (frame, channel) element access into the output block; honours
        /// the layout.
        ///
        /// @note `const` because the context is delivered by `const&`; the output
        ///       pointer is non-const, so writing through the returned reference
        ///       is well-formed.
        T &at_out(std::size_t frame, std::size_t ch) const noexcept
        {
            assert(frame < m_num_frames && ch < m_num_out_ch);
            if constexpr(Layout == ola_layout::interleaved)
                return m_output[frame * m_num_out_ch + ch];
            else
                return m_output[ch * m_num_frames + frame];
        }

    private:
        const T *m_input;
        T *m_output;
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

        /// Pointer to the output hop; element order follows the Layout parameter.
        const T *data() const noexcept { return m_output; }

        std::size_t num_frames() const noexcept { return m_num_frames; }
        std::size_t num_out_channels() const noexcept { return m_num_out_ch; }
        std::size_t num_samples() const noexcept { return m_num_frames * m_num_out_ch; }

        /// (frame, channel) element access into the output hop; honours the layout.
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

    /// Construct and pre-allocate all working storage.
    ///
    /// `frame_size` is the analysis block length (the FFT size) and `hop_size`
    /// the stride between successive blocks, both counted in frames.
    ///
    /// @pre `0 < hop_size <= frame_size`
    /// @pre `num_in_channels > 0 && num_out_channels > 0`
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
        m_output_buf(frame_size * num_out_channels, T{}),
        m_ola_buf(frame_size * num_out_channels, T{}),
        m_hop_buf(Layout == ola_layout::planar ? hop_size * num_out_channels : 0, T{}),
        m_frames_in_buf(0)
    {
        assert(hop_size > 0 && hop_size <= frame_size);
        assert(num_in_channels > 0);
        assert(num_out_channels > 0);
    }

    /// Append `num_frames` frames (in the configured layout), firing the
    /// callbacks as blocks and hops complete. Does not allocate.
    ///
    /// Data is handled a whole frame at a time; partial frames are not
    /// representable.
    /// @pre `frames` addresses at least `num_frames * num_in_channels()` samples.
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

    /// Zero-pad any buffered partial block, process it, then reset the
    /// accumulation buffer. Call once at end-of-stream.
    ///
    /// @note The buffer applies no windowing of its own; apply any
    ///       analysis/synthesis window inside process_fn.
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
        std::fill(m_output_buf.begin(), m_output_buf.end(), T{});

        m_process_fn(
            process_context{
                m_input_buf.data(),
                m_output_buf.data(),
                m_frame_size,
                m_num_in_ch,
                m_num_out_ch
            });

        // Accumulate the output block into the OLA buffer (same dims, layout-agnostic).
        const std::size_t ola_samples = m_frame_size * m_num_out_ch;
        for(std::size_t i = 0; i < ola_samples; ++i)
            m_ola_buf[i] += m_output_buf[i];

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
    std::vector<T> m_output_buf;
    std::vector<T> m_ola_buf;
    std::vector<T> m_hop_buf; // planar compaction buffer; empty when interleaved

    std::size_t m_frames_in_buf{0};
};

} // namespace vpp
