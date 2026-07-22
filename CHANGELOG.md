# Changelog

All notable changes to this project will be documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-07-22

### Added
- `keyed_vector<T, KeyFn, Key>`: insertion-ordered container with keyed lookup
  - `KeyFn` is a non-type template parameter (free-function pointer, pointer-to-data-member, pointer-to-const-member-function); `Key` is deduced automatically
  - Idiomatic usage via type alias: `using my_container = vpp::keyed_vector<my_data, &my_data::id>;`
  - Initializer list constructor (first occurrence wins on duplicate keys)
  - `insert` (returns `std::pair<iterator, bool>`), `insert_or_assign`, `erase` (by key and by pointer), `clear`
  - `operator[]` (unchecked, asserts in debug), `at(key)` (throws `std::out_of_range`)
  - `find`, `find_ptr`, `contains`
  - `size`, `empty`, full range iteration in insertion order
- `ola_frame_buffer`: overlap-add frame buffer for STFT-style block processing
  - Accumulates overlapping frames and pops completed hop-sized output
  - Supports both frame-wise and channel-wise data layouts
- `functional`: function-object utilities
  - `bind_front`: C++17 backport of C++20's `std::bind_front` (aliases `std::bind_front` when available)
  - `compare_proj<Proj, Compare>`: comparator that projects through a member or projection, for container comparators derived from members
- `macros`: small macro helpers
  - `VPP_FWD(x)`: terse `std::forward<decltype(x)>(x)`
- `thread`: utilities for parallel execution with serial, in-order output
  - `thread_pool`: fixed-size worker pool; FIFO `post`, `post_to_all` for once-per-worker setup, accepts move-only tasks, drains queued work on destruction, and swallows task exceptions to keep workers alive
  - `ordered_pipeline<Result>`: runs tasks in parallel on a `thread_pool` and delivers results to a sink strictly in submission order (worker-driven emission); optional bounded backpressure via `max_pending`
  - `task_result<T>`: an expected-lite (value or captured exception) handed to the sink so task failures are delivered as data, in order
- CMake build: header-only `vpp::vpp` INTERFACE target with install and `vppConfig.cmake`
- CMake presets: shared logic in `CMakePresets.json`; machine-specific toolchain in `CMakeUserPresets.json`
- Example programs under `examples/` for each module
- GTest suite via `FetchContent` (`-DBUILD_TESTING=ON`)
- MIT License
