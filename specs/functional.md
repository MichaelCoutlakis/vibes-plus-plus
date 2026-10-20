# `functional`

**Header:** `<vpp/functional.hpp>`

A small set of function-object utilities that felt missing from the standard library.

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
