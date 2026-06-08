# `ola_frame_buffer`

## Overview

A stream block formatter for overlap-add (OLA) DSP pipelines, such as STFT-based
processing. It sits between a continuous sample stream and a block-oriented
processing routine, handling the bookkeeping of:

- **buffering** a push-based input stream into fixed-size analysis blocks with a
  configurable hop (overlap),
- **invoking** a user processing callback for each completed block, handing it a
  zeroed scratch buffer to write its result into,
- **accumulating** (overlap-adding) those results and **emitting** completed
  hops of output to a downstream callback.

The class owns and manages the overlap-add accumulation buffer. The processing
callback is only responsible for transforming one block of input into one block
of output; the windowing, accumulation, and hop bookkeeping are handled here.

### Design properties

- **Header-only**, single class template, default element type `float`.
- **Zero-allocation steady state.** All buffers are sized at construction;
  `push()` and `flush()` perform no heap allocation.
- **Interleaved (frame-wise) layout** — WAV order: `s[0]ch0, s[0]ch1, …,
  s[1]ch0, …`. Input and output channel counts are independent.
- **Not thread-safe.** The caller synchronises if the buffer is shared.
- No windowing is applied internally — apply any analysis/synthesis window
  inside the processing callback. This keeps channel-count and windowing
  concerns out of the buffer.

### The one-hop-per-block invariant

For each completed input block, exactly one hop of output is emitted: the front
`hop_size` frames of the accumulation buffer. This is the only region guaranteed
to have received all of its overlap-add contributions — positions beyond
`hop_size` may still be added to by the next block. After emitting, the overlap
tail is shifted to the front and the vacated region is zeroed.

A consequence is latency: the first emitted hop is the "ramp-up" region and has
only a single block's contribution. In a symmetric windowed STFT pipeline this
is the expected startup transient.

## Template parameters

| Parameter | Default | Meaning |
|-----------|---------|---------|
| `T`       | `float` | Sample type. Must support `+=` and value-initialisation. |

## Construction

```cpp
ola_frame_buffer(std::size_t  block_size,
                 std::size_t  hop_size,
                 std::size_t  num_in_channels,
                 std::size_t  num_out_channels,
                 process_fn_t process_fn,
                 output_fn_t  output_fn);
```

| Argument           | Meaning |
|--------------------|---------|
| `block_size`       | Frames per analysis block (the FFT size). |
| `hop_size`         | Frames between successive blocks. Must satisfy `0 < hop_size <= block_size`. |
| `num_in_channels`  | Interleaved channel count of the input stream (> 0). |
| `num_out_channels` | Interleaved channel count of the output stream (> 0). |
| `process_fn`       | Called once per completed input block. |
| `output_fn`        | Called once per completed output hop. |

Preconditions are checked with `assert` (debug builds).

## Callbacks

### `process_fn` — `void(const process_context&)`

Invoked when a full input block is ready. The `scratch` buffer is zeroed before
each call; the callback writes its result into it. After the callback returns,
the buffer overlap-adds `scratch` into its accumulation buffer.

```cpp
struct process_context
{
    const T*    input;             // interleaved input block
    T*          scratch;           // zeroed output work area, write here
    std::size_t block_size;
    std::size_t num_in_channels;
    std::size_t num_out_channels;

    std::size_t num_input_samples()  const noexcept;  // block_size * num_in_channels
    std::size_t num_output_samples() const noexcept;  // block_size * num_out_channels

    // Interleaved (frame, channel) element access. assert-checked in debug.
    // Both are const: the context is passed to the callback by const&, and
    // scratch is a non-const pointer so writing through out() is well-formed.
    const T& in (std::size_t frame, std::size_t ch) const noexcept;  // input
    T&       out(std::size_t frame, std::size_t ch) const noexcept;  // scratch
};
```

### `output_fn` — `void(const output_context&)`

Invoked when one hop of accumulated output is ready to consume. `num_frames`
equals `hop_size` for every block-driven emission; it may be smaller only on the
final `flush()` drain of a zero-padded partial block.

```cpp
struct output_context
{
    const T*    output;            // interleaved output hop
    std::size_t num_frames;
    std::size_t num_out_channels;

    std::size_t num_samples() const noexcept;  // num_frames * num_out_channels

    // Interleaved (frame, channel) element access. assert-checked in debug.
    const T& out(std::size_t frame, std::size_t ch) const noexcept;  // output
};
```

## Member functions

| Member | Description |
|--------|-------------|
| `void push(const T* frames, std::size_t num_frames)` | Append `num_frames` interleaved frames. Fires `process_fn`/`output_fn` as blocks and hops complete. No allocation. |
| `void flush()` | Zero-pad any buffered partial block, process it, then reset the accumulation buffer. Call once at end-of-stream. |
| `std::size_t block_size() const noexcept` | Configured block size. |
| `std::size_t hop_size() const noexcept` | Configured hop size. |
| `std::size_t num_in_channels() const noexcept` | Configured input channel count. |
| `std::size_t num_out_channels() const noexcept` | Configured output channel count. |

## Example

```cpp
using ola = vpp::ola_frame_buffer<float>;

ola buffer(
    /*block_size=*/        1024,
    /*hop_size=*/          256,
    /*num_in_channels=*/   1,
    /*num_out_channels=*/  1,
    [](const ola::process_context& ctx) {
        // Transform one block: window, FFT, process, IFFT, window — writing
        // the synthesised block into ctx.scratch.
        process_block(ctx.input, ctx.scratch, ctx.block_size);
    },
    [](const ola::output_context& ctx) {
        // Consume one hop of reconstructed output.
        sink.write(ctx.output, ctx.num_frames);
    });

// Stream samples in arbitrarily sized chunks.
while (auto chunk = source.read())
    buffer.push(chunk.data(), chunk.num_frames());

buffer.flush();
```

For simple per-sample work the `in`/`out` accessors hide the interleave stride:

```cpp
[gain](const ola::process_context& ctx) {
    for (std::size_t f = 0; f < ctx.block_size; ++f)
        for (std::size_t ch = 0; ch < ctx.num_out_channels; ++ch)
            ctx.out(f, ch) = ctx.in(f, ch) * gain;
}
```

## Open questions / future work

- **Other layouts.** Only interleaved frame-wise layout is supported. The design
  keeps channel count explicit so a planar layout policy could be added later.
- **Startup/teardown windowing.** Currently the first and last hops are simply
  zero-padded ramp regions. A windowing strategy could be layered on top by the
  caller; whether the buffer should offer help here is undecided.
