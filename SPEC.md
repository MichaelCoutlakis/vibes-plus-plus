# Software Specification: Vibes++

## Overview

* **Purpose** A C++ utilities library providing small, useful primitives not found in the C++17 standard library, without the weight of Boost or other large dependencies.
* **Scope** Individually useful, header-only components added as needed.
* **Target Audience** Anyone with matching vibes.

## Coding Standard

- Follow the [Boost/C++ coding conventions](https://www.boost.org/development/requirements.html): `snake_case` for all identifiers including types, functions, variables, and template parameters.
- Exception: `TEST(Suite, CaseName)` GTest macros use `PascalCase` for the case name per GTest convention.
- Formatting is governed by `.clang-format`; static analysis by `.clang-tidy`.

### Documentation

- Document the public API with Doxygen comments, but **only where the comment adds
  information the code does not already convey.** Do not restate the obvious — a
  comment such as "`begin()`: returns an iterator to the first element" earns its
  place nowhere.
- Use `///` comment blocks and `@`-style tags (not `\`).
- Omit `@brief`; rely on the autobrief convention where the first sentence is the
  brief (`JAVADOC_AUTOBRIEF`). Likewise omit `@param`/`@return` when the name and
  signature already make the meaning plain.
- Prefer tags that capture what the signature cannot: `@tparam` for template
  parameter intent, `@pre` for preconditions (mirroring `assert`s), and
  `@note`/`@warning` for non-obvious caveats. Add `@param`/`@return` only when the
  meaning is genuinely non-obvious from the name.

## Project Requirements

- The library shall support C++ >= 17 on GCC, Clang, and MSVC.
- The library shall use `CMake` (>= 3.20) for build and install.
- The library shall be header-only where practical; the CMake target is `vpp::vpp` (INTERFACE).
- The library shall provide test coverage via `gtest` (fetched via CMake `FetchContent`).
- The library shall use `vpp` as its namespace ("Vibes Plus Plus").
- The library shall be licensed under the MIT License.

## Repository Layout

```
vibes-plus-plus/
  include/
    vpp/
      keyed_vector.hpp
      ola_frame_buffer.hpp
  tests/
    CMakeLists.txt
    test_keyed_vector.cpp
    test_ola_frame_buffer.cpp
  examples/
    CMakeLists.txt
    keyed_vector.cpp
    ola_frame_buffer.cpp
  specs/
    keyed_vector.md
    ola_frame_buffer.md
  CMakeLists.txt
  CMakePresets.json
  LICENSE
  SPEC.md
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

Each component has its own specification under [`specs/`](specs):

| Component | Header | Specification |
|-----------|--------|---------------|
| `keyed_vector` | `<vpp/keyed_vector.hpp>` | [specs/keyed_vector.md](specs/keyed_vector.md) |
| `ola_frame_buffer` | `<vpp/ola_frame_buffer.hpp>` | [specs/ola_frame_buffer.md](specs/ola_frame_buffer.md) |
| `functional` | `<vpp/functional.hpp>` | [specs/functional.md](specs/functional.md) |