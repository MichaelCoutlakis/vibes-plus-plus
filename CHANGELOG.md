# Changelog

All notable changes to this project will be documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `keyed_vector<T, Key, key_func_t>`: insertion-ordered container with keyed lookup
  - `key_func_t` is a type template parameter accepting any callable `Key(const T&)` — free functions, functors, and lambdas all work
  - CTAD deduction guide for free-function pointer construction
  - `insert` (returns `std::pair<iterator, bool>`), `insert_or_assign`, `erase` (by key and by pointer), `clear`
  - `operator[]` (unchecked, asserts in debug), `at(key)` (throws `std::out_of_range`)
  - `find`, `find_ptr`, `contains`
  - `size`, `empty`, full range iteration in insertion order
- CMake build: header-only `vpp::vpp` INTERFACE target with install and `vppConfig.cmake`
- CMake presets: shared logic in `CMakePresets.json`; machine-specific toolchain in `CMakeUserPresets.json`
- Example program: `examples/keyed_vector.cpp`
- GTest suite via `FetchContent` (`-DBUILD_TESTING=ON`)
- MIT License
