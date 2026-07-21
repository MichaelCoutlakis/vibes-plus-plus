# Software Specification: Vibes++

## Overview

Project level specification for Vibes++:

- Coding standard
- Documentation convention
- Build and install
- Project requirements

For other information, see the [README](README.md)

## Coding Standard

- Follow the
  [Boost/C++ coding conventions](https://www.boost.org/development/requirements.html):
  `snake_case` for types, functions, and variables. Members are prefixed `m_`.
- Two `PascalCase` exceptions: template parameters (e.g.
  `template <typename Result>`, mirroring the standard library `std::vector<T>`)
  and the case name in `TEST(Suite, CaseName)` per GTest convention.
- Formatting is governed by `.clang-format`; static analysis by `.clang-tidy`.

### Documentation

- Document the public API with Doxygen comments, but **only where the comment
  adds information the code does not already convey.** Do not restate the
  obvious — a comment such as "`begin()`: returns an iterator to the first
  element" earns its place nowhere.
- Use `///` comment blocks and `@`-style tags (not `\`).
- Omit `@brief`; rely on the autobrief convention where the first sentence is
  the brief (`JAVADOC_AUTOBRIEF`). Likewise omit `@param`/`@return` when the
  name and signature already make the meaning plain.
- Prefer tags that capture what the signature cannot: `@tparam` for template
  parameter intent, `@pre` for preconditions (mirroring `assert`s), and
  `@note`/`@warning` for non-obvious caveats. Add `@param`/`@return` only when
  the meaning is genuinely non-obvious from the name.

## Project Requirements

- The library shall support C++ >= 17 on GCC, Clang, and MSVC.
- The library shall use `CMake` (>= 3.20) for build and install.
- The library shall be header-only where practical; the CMake target is
  `vpp::vpp` (INTERFACE).
- The library shall provide test coverage via `gtest` (fetched via CMake
  `FetchContent`).

- The library shall be licensed under the MIT License.

## Repository Layout

```
vibes-plus-plus/
  include/vpp/       — <module>.hpp per module
  specs/             — <module>.md per module
  tests/             — test_<module>.cpp per module, plus CMakeLists.txt
  examples/          — <module>.cpp per module, plus CMakeLists.txt
  CMakeLists.txt
  CMakePresets.json
  README.md
  SPEC.md
  LICENSE
  CHANGELOG.md
```

## Build & Install

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /usr/local
```

To run tests:

```sh
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
```

With presets (configure `CMakeUserPresets.json` for your toolchain first):

```sh
cmake --preset dev
cmake --build --preset dev-debug
ctest --preset dev
```

Installing exports a `vppConfig.cmake` so consumers can write:

```cmake
find_package(vpp REQUIRED)
target_link_libraries(my_app PRIVATE vpp::vpp)
```

---

## Components

Each component has its own specification under [`specs/`](specs). See the
component list in [README.md](README.md) for the current set.
