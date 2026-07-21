# Vibes++

Vibes++ is a small collection of C++ utilities that felt missing from the standard library.

My vibes are:
- Brevity

If we vibe, feel free to use this library.

See [SPEC.md](SPEC.md) for project-level specifications, and the [`specs/`](specs) folder for per-component specifications:

- [`keyed_vector`](specs/keyed_vector.md) — insertion-ordered container with keyed lookup over contiguous storage.
- [`ola_frame_buffer`](specs/ola_frame_buffer.md) — overlap-add frame buffer for STFT-style block processing.
- [`functional`](specs/functional.md) — function-object utilities; includes `bind_front`, a C++17 backport of C++20's `std::bind_front`.
- [`macros`](specs/macros.md) — small macro helpers; includes `VPP_FWD`, a terse `std::forward<decltype(x)>(x)`.
- [`thread`](specs/thread.md) — a `thread_pool` and `ordered_pipeline` for parallel execution with serial, in-order output.

See the [examples folder](examples) for example usage.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for the repository layout, how to add a
module, build instructions, and coding conventions.

## License
MIT — see [LICENSE](LICENSE) for details.