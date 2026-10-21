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

// As an associative-container comparator (the type, not just a predicate).
// Key on something unique: a std::set drops elements whose keys compare equal,
// so use a std::multiset when keys repeat (e.g. scores).
std::set<player, vpp::compare_proj<&player::name>> by_name;
std::multiset<player, vpp::compare_proj<&player::score>> by_score;

// As a one-off predicate, ascending then descending:
std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score>{});
std::sort(v.begin(), v.end(), vpp::compare_proj<&player::score, std::greater<>>{});

// Project through a member function just as easily:
std::sort(v.begin(), v.end(), vpp::compare_proj<&player::rank>{});
```

### Transparent lookup

`compare_proj` declares `is_transparent`, and each operand is projected only if
`Proj` can be applied to it; any other operand is taken to be an
already-projected key and passed to `Compare` as is. The two operands may
therefore differ in type: an element and a key, in either order, or a derived
and a base element.

This enables the containers' heterogeneous overloads of `find`, `contains`,
`count`, `lower_bound`, `upper_bound`, and `equal_range`, so lookup by key needs
no dummy element:

```cpp
by_name.find(std::string_view{"ada"});  // O(log n); builds no player and no std::string
auto [lo, hi] = by_score.equal_range(20);
auto first = by_score.lower_bound(25);  // first player scoring at least 25
```

Requirements and caveats:

- `Proj` must be SFINAE-friendly, as pointers to members are. An unconstrained
  generic lambda (`[](const auto &p) { return p.score; }`) is not: asking
  whether it applies to a key fails to compile inside its body. Give it a
  constraint or an explicit parameter type.
- For keyed lookup, `Compare` must accept the key type alongside the projected
  type, as `std::less<>` does for e.g. `std::string` and `std::string_view`.
- "Can `Proj` be applied" is decided by `std::invoke`. A pointer to member
  applies only to its class (or a derived class), but a free-function `Proj`
  taking `const T &` also accepts anything that converts implicitly to `T`, and
  such a key is converted and projected rather than compared as is.

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
| `cmp(lhs, rhs)` | Returns `Compare{}(std::invoke(Proj, lhs), std::invoke(Proj, rhs))`, projecting only operands `Proj` applies to. |
| `compare_proj<...>::is_transparent` | Present; enables heterogeneous lookup in associative containers. |

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
