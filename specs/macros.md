# `macros`

**Header:** `<vpp/macros.hpp>`

A short, namespace-free helper macro. Macros are not scoped to a namespace and
leak across the whole translation unit, so they are kept in their own header —
deliberately separate from the `vpp::` function objects — and the unprefixed
alias is opt-in.

## `VPP_FWD`

```cpp
#define VPP_FWD(x) static_cast<decltype(x) &&>(x)
```

A terse perfect-forward, equivalent to `std::forward<decltype(x)>(x)` but
without spelling the type or including `<utility>`. It is intended for
**forwarding-reference parameters**: for such a parameter `decltype(x)` is `T&`
(lvalue) or `T&&` (rvalue), and the `&&`-cast collapses to the correct
reference, so the caller's value category is preserved.

```cpp
auto wrap = [](auto &&f, auto &&...args)
{
    return std::invoke(VPP_FWD(f), VPP_FWD(args)...);
};
```

> **Caveat:** used on a plain (non-reference) named variable, `decltype(x)` is
> the bare type `T`, so `VPP_FWD(x)` casts to `T&&` and **moves**. Reach for
> `VPP_FWD` only on forwarding references; to move a concrete variable use
> `std::move` directly.

### Why no `VPP_MOV`?

A move counterpart would be `static_cast<std::remove_reference_t<decltype(x)>&&>(x)`
— exactly what `std::move(x)` already does. `std::move` is short, standard, and
universally recognized, so there is nothing to gain by aliasing it; use it
directly.

## Short alias (opt-in)

Define `VPP_DEFINE_SHORT_MACROS` *before* including the header to also get the
unprefixed spelling `FWD`:

```cpp
#define VPP_DEFINE_SHORT_MACROS
#include <vpp/macros.hpp>

auto wrap = [](auto &&f, auto &&...a) { return std::invoke(FWD(f), FWD(a)...); };
```

The alias is guarded with `#ifndef`, so it defers to any existing definition of
`FWD` rather than clobbering it. The prefixed `VPP_FWD` is always available.

## API

| Macro | Description |
|-------|-------------|
| `VPP_FWD(x)` | `static_cast<decltype(x)&&>(x)` — forward a forwarding-reference parameter. |
| `FWD(x)` | Unprefixed alias, defined only when `VPP_DEFINE_SHORT_MACROS` is set and `FWD` is not already defined. |
