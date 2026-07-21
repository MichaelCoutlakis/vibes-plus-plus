# Contributing

## Purpose

Vibes++ collects small, useful C++17 utilities that are:

- not available in C++17,
- lighter than Boost or the alternatives, or
- too small to justify a separate project.

## Repository layout

Header-only. Each module (or small collection of related utilities) is one
header, with a matching spec, tests, and optionally an example:

| Kind    | Location                          |
|---------|-----------------------------------|
| Header  | `include/vpp/<module>.hpp`        |
| Spec    | `specs/<module>.md`               |
| Tests   | `tests/test_<module>.cpp` (GTest) |
| Example | `examples/<module>.cpp`           |

Project-level specifications live in [SPEC.md](SPEC.md); it is the source of
truth for the coding standard, documentation style, build, and requirements.

## Adding a module

1. Write the header under `include/vpp/`, the spec under `specs/`, and GTest
   coverage under `tests/`.
2. Register the tests in [`tests/CMakeLists.txt`](tests/CMakeLists.txt): add the
   `add_executable` / `target_link_libraries` pair and a `gtest_discover_tests`
   line.
3. If you add an example, register it in
   [`examples/CMakeLists.txt`](examples/CMakeLists.txt) (`vpp_example(<module>)`).
4. Add the module to the component list in [README.md](README.md).

## Building and testing

See **Build & Install** in [SPEC.md](SPEC.md). In short, with presets:

```sh
cmake --preset dev
cmake --build --preset dev-debug
ctest --preset dev
```

Configure your toolchain in `CMakeUserPresets.json` first. Tests and examples
are opt-in via `BUILD_TESTING` / `BUILD_EXAMPLES`.

## Conventions

Follow the **Coding Standard** and **Documentation** sections of
[SPEC.md](SPEC.md). Highlights:

- `snake_case` for types, functions, and variables; members prefixed `m_`.
- `PascalCase` for template parameters and for the case name in
  `TEST(Suite, CaseName)`.
- Every file starts with the SPDX MIT header block.
- Public API documented with Doxygen `///` comments — only where the comment
  adds what the code does not already convey.

Choose names with brevity, meaning, and clarity. Before adding a utility, check
whether a permissive C++17 project already provides it.

## Process

Ideas are discussed before code is written. If you are working with an
assistant, it should get confirmation before editing code, and is welcome to
critique a request and suggest alternatives.
