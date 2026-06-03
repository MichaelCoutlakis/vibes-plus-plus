# Changelog

All notable changes to this project will be documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- `ordered_registry<T, RegKey, KeyFunc>`: insertion-ordered registry container
  - `KeyFunc` is a type template parameter accepting any callable `RegKey(const T&)` — free functions, functors, and lambdas all work
  - CTAD deduction guide for free-function pointer construction
  - `insert` (returns `std::pair<iterator, bool>`), `insert_or_assign`, `erase` (by key and by pointer), `clear`
  - `operator[]` (unchecked, asserts in debug), `at(key)` (throws `std::out_of_range`)
  - `find`, `find_ptr`, `contains`
  - `size`, `empty`, full range iteration in insertion order
- CMake build: header-only `vpp::vpp` INTERFACE target with install and `vppConfig.cmake`
- GTest suite via `FetchContent` (`-DBUILD_TESTING=ON`)
- MIT License
