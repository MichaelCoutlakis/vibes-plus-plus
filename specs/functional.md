# `functional`

**Header:** `<vpp/functional.hpp>`

A small set of function-object utilities that felt missing from C++17.

## `bind_front`

A C++17 backport of [`std::bind_front`](https://en.cppreference.com/w/cpp/utility/functional/bind_front)
(C++20). Semantics are identical, so for the full reference defer to cppreference;
this section only states what is specific to this implementation.

### Overview

`vpp::bind_front(f, bound...)` returns a call wrapper that stores a decay-copy
of the callable `f` and of each leading (bound) argument. Invoking the wrapper
forwards the bound arguments followed by the call-site arguments to `f`:

```
g(call...)  ==  std::invoke(f, bound..., call...)
```

Because `std::invoke` is the underlying primitive, `f` may be a free-function
pointer, a functor/lambda, or a pointer to member (function or data); in the
member case the object is supplied as the first bound argument.

```cpp
auto cb = vpp::bind_front(&my_class::my_member, this);
cb(a, b, c);                    // this->my_member(a, b, c)

auto h  = vpp::bind_front(&free_fn);
h(x, y, z);                     // free_fn(x, y, z)
```

### Behaviour notes (same as `std::bind_front`)

- Bound arguments are stored by **decay-copy**. To bind a reference, wrap it
  with `std::ref` / `std::cref`.
- Binding a raw pointer (e.g. `this`) stores a **non-owning** pointer; the
  pointee must outlive the wrapper.
- The wrapper's value category propagates to the stored callable and bound
  arguments: invoking an **rvalue** wrapper moves them into the call. The call
  operator is overloaded on all four ref-qualifiers (`&`, `const &`, `&&`,
  `const &&`).

### Standard-library passthrough

When the standard library provides `std::bind_front` (C++20 and later, detected
via the `__cpp_lib_bind_front` feature-test macro), `<vpp/functional.hpp>`
introduces it into namespace `vpp` with a using-declaration — so `vpp::bind_front`
*is* `std::bind_front`, with no wrapper of our own in the way. The hand-written
backport is compiled only on C++17. Either way, `vpp::bind_front` call sites are
source-compatible.

### Limitations of the C++17 backport

These apply only on the backport path; on C++20+ you get the standard
guarantees instead:

- No conditional `noexcept` specification.
- `constexpr` invocation depends on the callable; no extra guarantees are made
  beyond what `std::invoke` provides in C++17.

### API

| Expression | Description |
|-----------|-------------|
| `vpp::bind_front(f, bound...)` | Returns a call wrapper holding decay-copies of `f` and `bound...`. |
| `g(call...)` | Invokes the wrapped callable as `std::invoke(f, bound..., call...)`; `call...` are perfectly forwarded. |

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

### Relationship to C++20 ranges projections

On C++20 the algorithm cases overlap with ranges projections —
`std::ranges::sort(v, std::less{}, &player::score)` does the same job. `compare_proj`
exists for two reasons: it works on C++17, and it provides a reusable comparator
**type** suitable for `std::set` / `std::map` / `std::priority_queue`, which
projections (a per-call argument) do not give you.

### API

| Expression | Description |
|-----------|-------------|
| `vpp::compare_proj<Proj, Compare>` | Empty comparator type projecting through `Proj` and comparing with `Compare` (default `std::less<>`). |
| `cmp(lhs, rhs)` | Returns `Compare{}(std::invoke(Proj, lhs), std::invoke(Proj, rhs))`. |
