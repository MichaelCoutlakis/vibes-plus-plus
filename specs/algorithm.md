# `algorithm`

**Header:** `<vpp/algorithm.hpp>`

Index-returning counterparts of `std::ranges::max_element` / `min_element`, in
the spirit of Armadillo's `index_max` / `index_min`.

## Overview

The standard library returns iterators, so getting an index takes a
`std::ranges::distance(v.begin(), ...)` wrapper on every call. These functions
return the index directly:

```cpp
std::vector<double> v{3.0, 7.5, -2.0, 7.5, 4.1};

vpp::index_max(v);                        // 1
vpp::index_min(v);                        // 2
vpp::index_max(v, vpp::abs);              // 1 (largest magnitude)
vpp::index_nearest(v, 4.0);               // 4
vpp::index_min(v, vpp::distance_to(4.0)); // 4, the long form

std::vector<double> edges{0, 100, 200, 300};  // sorted
vpp::index_nearest_sorted(edges, 140.0);      // 1, by binary search

// Nearest by a member, for types without arithmetic of their own:
vpp::index_nearest(bins, 445.0, &bin::freq);

// Any other metric: index_min with a projection that returns the distance.
vpp::index_min(bins, [&](const bin &b) { return std::abs(std::log2(b.freq() / x)); });
```

`vpp::abs` and `vpp::distance_to` are projections from
[`functional`](functional.md).

## Behaviour

- **Input:** any `std::ranges::forward_range`; `index_nearest_sorted` needs a
  `std::ranges::bidirectional_range`. The range may be a temporary, since only
  an index is returned. The index counts from the range's own `begin`, so for a
  view (e.g. `std::views::reverse(v)`) it is a position in the view.
- **Return type:** `std::size_t`.
- **Empty range:** throws `std::invalid_argument` (Armadillo throws
  `std::logic_error`, which is its base class).
- **Ties:** the lowest index wins, as with `std::ranges::max_element` /
  `min_element`. For `index_nearest`, an element equally far below and above `x`
  resolves to the lower index; `index_nearest_sorted` returns the same index as
  `index_nearest` on sorted input, including the first of repeated values.
- **NaN:** values that do not form a strict weak order give unspecified results.
- **Distances** in `index_nearest` / `index_nearest_sorted` are computed by
  `vpp::distance_to` on the projected value, so unsigned elements do not wrap
  and `std::complex` elements are measured by magnitude. The metric is not a
  parameter: a default could not depend on `x`, and `index_min` with a
  distance-returning projection already covers custom metrics.

## API

| Expression | Description |
|-----------|-------------|
| `vpp::index_max(r, proj = std::identity{})` | Index of the largest element of `r`, compared by `proj`. O(n). |
| `vpp::index_min(r, proj = std::identity{})` | Index of the smallest element of `r`, compared by `proj`. O(n). |
| `vpp::index_nearest(r, x, proj = std::identity{})` | Index of the element whose projection is nearest to `x`, by `vpp::distance_to(x)`. O(n). |
| `vpp::index_nearest_sorted(r, x, proj = std::identity{})` | As `index_nearest`, for `r` sorted ascending by `proj`. O(log n) comparisons. Unsorted input gives an unspecified index. |
