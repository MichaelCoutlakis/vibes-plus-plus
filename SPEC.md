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
| `KeyFn` | Compile-time key accessor (non-type template parameter). May be a free-function pointer, pointer to data member, or pointer to const member function. |
| `Key` | Key type; deduced from `KeyFn` via `std::invoke_result` — override only when needed. Must be equality-comparable. |

Because `KeyFn` is a non-type template parameter, lambdas and stateful functors are not supported (C++17 restriction).

The idiomatic usage is a type alias:

```cpp
using my_container = vpp::keyed_vector<my_data, &my_data::id>;
```

#### Invariants

- Keys are unique; `insert()` rejects duplicates.
- Insertion order is preserved across all operations.
- Lookup is O(n); suited for small collections where stable order and contiguous storage matter more than lookup speed.

#### API

| Expression | Description |
|-----------|-------------|
| `keyed_vector{a, b, ...}` | Construct from initializer list; duplicate keys are silently ignored (first wins). |
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
| `kv.as_vec()` | Returns a reference to the underlying `std::vector`, giving direct access to the full vector API (positional indexing, `data()`, etc.). Const overload provided. Mutating key fields through the non-const overload can violate key uniqueness — same caveat as mutation via iterators or `find_ptr`. |
| `kv.size()` | Number of elements. |
| `kv.empty()` | True when no elements. |
| `begin/end/cbegin/cend` | Standard range iteration in insertion order. |
