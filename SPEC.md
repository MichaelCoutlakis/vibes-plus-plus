# Software Specification: Vibes++

## Overview

* **Purpose** A C++ utilities library providing small, useful primitives not found in the C++17 standard library, without the weight of Boost or other large dependencies.
* **Scope** Individually useful, header-only components added as needed.
* **Target Audience** Anyone with matching vibes.

## Coding Standard

- Follow the [Boost/C++ coding conventions](https://www.boost.org/development/requirements.html): `snake_case` for all identifiers including types, functions, variables, and template parameters.
- Exception: `TEST(Suite, CaseName)` GTest macros use `PascalCase` for the case name per GTest convention.
- Formatting is governed by `.clang-format`; static analysis by `.clang-tidy`.

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
  tests/
    CMakeLists.txt
    test_keyed_vector.cpp
  examples/
    CMakeLists.txt
    keyed_vector.cpp
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

## Component Requirements

### `keyed_vector`

**Header:** `<vpp/keyed_vector.hpp>`

An insertion-ordered container that stores elements of type `T` in contiguous memory (`std::vector`) and provides keyed lookup via a caller-supplied callable. Suitable for small collections where stable insertion order and contiguous storage matter more than lookup speed.

#### Template parameters

| Parameter | Description |
|-----------|-------------|
| `T` | Element type |
| `Key` | Key type; must be equality-comparable |
| `key_func_t` | Callable: `Key(const T&)`. May be a free-function pointer, a stateless functor, or a lambda. |

Stateless functors are default-constructed inside the container. For lambdas (which are not default-constructible in C++17), pass the instance to the constructor.

A CTAD deduction guide is provided for the common case of a free-function pointer:

```cpp
vpp::keyed_vector kv{&get_my_key};
```

#### Invariants

- Keys are unique; `insert()` rejects duplicates.
- Insertion order is preserved across all operations.
- Lookup is O(n); suited for small collections where stable order and contiguous storage matter more than lookup speed.

#### API

| Expression | Description |
|-----------|-------------|
| `kv.insert(t)` | Insert element; returns `{iterator, true}` on insertion, `{iterator-to-existing, false}` on duplicate. |
| `kv.insert_or_assign(t)` | Insert or overwrite; returns `{iterator, true}` on insertion, `{iterator, false}` on assignment. Position in insertion order is preserved on assignment. |
| `kv.erase(key)` | Remove by key; no-op on miss. |
| `kv.erase(ptr)` | Remove by pointer; throws `std::out_of_range` if not owned. |
| `kv.clear()` | Remove all elements. |
| `kv[key]` | Unchecked access; asserts in debug builds on miss. |
| `kv.at(key)` | Checked access by key; throws `std::out_of_range` on miss. |
| `kv.find(key)` | Returns iterator; `end()` on miss. Const overload provided. |
| `kv.find_ptr(key)` | Returns `T*`; `nullptr` on miss. Const overload provided. |
| `kv.contains(key)` | Returns `bool`. |
| `kv.size()` | Number of elements. |
| `kv.empty()` | True when no elements. |
| `begin/end/cbegin/cend` | Standard range iteration in insertion order. |
