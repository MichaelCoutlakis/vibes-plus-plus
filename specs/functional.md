# `functional`

**Header:** `<vpp/functional.hpp>`

A small set of function-object utilities that felt missing from the standard library:
`compare_proj`, `abs`, and `distance_to`.

## `compare_proj`

A stateless comparator that orders objects by a *projected* value rather than
the whole object.

```cpp
template <auto Proj, class Compare = std::less<>>
struct compare_proj;
```

### Overview

`compare_proj<Proj, Compare>::operator()(lhs, rhs)` returns
`Compare{}(std::invoke(Proj, lhs), std::invoke(Proj, rhs))`. Because the
projection goes through `std::invoke`, `Proj` may be a pointer-to-data-member, a
pointer-to-member-function, or any other callable; `Compare` defaults to the
transparent `std::less<>`.

The type is empty (`std::is_empty_v` is `true`), so it carries no per-object
cost as a container comparator.

```cpp
struct player { std::string name; int score; };

// As an associative-container comparator (the type, not just a predicate):
std::set<player, vpp::compare_proj<&player::score>> by_score;

// As a one-off predicate, ascending then descending:
std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score>{});
std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score, std::greater<>>{});

// Project through a member function just as easily:
std::sort(v.begin(), v.end(), vpp::compare_proj<&player::rank>{});
```

### Relationship to ranges projections

The algorithm cases overlap with ranges projections —
`std::ranges::sort(v, std::less{}, &player::score)` does the same job. `compare_proj`
exists because it provides a reusable comparator **type** suitable for
`std::set` / `std::map` / `std::priority_queue`, which projections (a per-call
argument) do not give you.

### API

| Expression | Description |
|-----------|-------------|
| `vpp::compare_proj<Proj, Compare>` | Empty comparator type projecting through `Proj` and comparing with `Compare` (default `std::less<>`). |
| `cmp(lhs, rhs)` | Returns `Compare{}(std::invoke(Proj, lhs), std::invoke(Proj, rhs))`. |

## `abs`

Absolute value as a function object, so it can be passed as a projection or
callback. `std::abs` cannot be: it is an overload set, and the standard library
does not permit taking the address of its functions.

```cpp
inline constexpr abs_fn abs{};

auto i = vpp::index_max(spectrum, vpp::abs);              // largest-magnitude bin
std::ranges::sort(v, {}, vpp::abs);                       // sort by magnitude
```

- Unsigned values are returned unchanged.
- Other arithmetic types are negated when below zero. This is `constexpr`,
  unlike `std::abs` before C++23.
- Anything else, e.g. `std::complex`, goes to `abs` found in `std::` or by
  argument-dependent lookup, which for `std::complex` gives the magnitude.
- As with `std::abs`, the absolute value of the most negative signed integer is
  not representable.

## `distance_to`

A projection giving `|x - target|` for a fixed target.

```cpp
template <typename T>
class distance_to;

auto i = vpp::index_min(v, vpp::distance_to(x));  // see vpp::index_nearest
```

- For totally ordered operands, the distance is the larger value minus the
  smaller, so it does not wrap for unsigned types.
- Otherwise, e.g. for `std::complex`, it is `vpp::abs(x - target)`.
- With unsigned elements, use an unsigned target: comparing a signed target with
  unsigned elements applies the usual arithmetic conversions.

| Expression | Description |
|-----------|-------------|
| `vpp::distance_to(target)` | Projection holding `target`; the element type `T` is deduced. |
| `d(x)` | Returns the distance between `x` and `target`, as above. |
