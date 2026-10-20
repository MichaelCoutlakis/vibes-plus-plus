# `mat_view`

**Header:** `<vpp/mat_view.hpp>`; Armadillo adapters in `<vpp/mat_view_arma.hpp>`

## Overview

`mat_view<T>` and `cube_view<T>` are non-owning views of column-major matrices
and cubes, or of a block of one. They let a function take "a matrix of floats"
without its interface depending on a matrix library, and without the caller
copying a sub-block first:

```cpp
void process(vpp::mat_view<const float> x);   // frames in rows, channels in columns

arma::fmat audio(48000, 2);
process(audio);                  // whole matrix
process(audio.rows(200, 999));   // frames 200..999 of every channel, no copy
process(audio.col(1));           // one channel
```

Passing an `arma::subview` to a `const arma::fmat &` parameter would copy it
into a temporary; a `mat_view` parameter holds a pointer and a stride instead.

`std::mdspan` (C++23) covers similar ground. These views trade its generality
for names that read as matrix code — `rows()`, `cols()`, `col(c)`,
`rows_from(r0)` rather than `extent(0)` and `submdspan` — and for a fixed
column-major layout in which every column is contiguous.

## Layout

```
mat_view:   (r, c)    ->  data()[r + c * col_stride()]
cube_view:  (r, c, s) ->  data()[r + c * col_stride() + s * slice_stride()]
```

`col_stride()` is BLAS's "leading dimension": it equals `rows()` for a whole
matrix and exceeds it for a block of a taller one. Row-major layouts and
transposed views are out of scope; each column being contiguous is what makes
`col(c)` a `std::span` and lets `is_contiguous()` views go straight to
`memcpy` or BLAS.

## Semantics

- **Non-owning.** The view dangles if the underlying storage is resized or
  destroyed.
- **Cheap to copy** (pointer plus sizes, trivially copyable): pass by value.
- **Shallow constness**, as for `std::span`: `mat_view<float>` writes,
  `mat_view<const float>` reads, and a `const mat_view<float>` still writes
  elements. `mat_view<float>` converts implicitly to `mat_view<const float>`,
  not the other way round.
- **O(1) sub-views.** `block`, `row_range`, `rows_from`, `col_range`,
  `cols_from`, `row`, and `cube_view::slice` / `slice_range` return views of the
  same storage, keeping the parent's strides.
- Preconditions are checked with `assert`.

## Converting from matrix types

Any type with a `vpp::mat_view_traits` (or `cube_view_traits`) specialisation
converts **implicitly**, so a view parameter accepts the matrix directly.
Constness follows the source: a `const arma::fmat &` yields only
`mat_view<const float>`.

A temporary is accepted only when its traits mark it `borrowed`, i.e. it refers
to memory it does not own. `audio.rows(200, 999)` (an `arma::subview`) is
borrowed; a temporary `arma::fmat` is rejected at compile time, since the view
would dangle. Armadillo expression templates (`audio * 2.f`) have no storage and
do not convert.

Implicit conversions do not take part in template argument deduction, so a
function template on `T` taking `mat_view<T>` needs `vpp::as_view(m)`, which
returns the view with the source's element type and constness. Class template
argument deduction also works: `vpp::mat_view v{audio};`.

### Writing an adapter

The core header never includes a matrix library. An adapter header specialises
the traits, for a type or (with a constrained partial specialisation) a family
of types:

```cpp
template <>
struct vpp::mat_view_traits<my_matrix>
{
    static constexpr bool borrowed = false;          // owns its memory
    static auto *data(auto &m) { return m.ptr(); }   // m is my_matrix or const my_matrix
    static std::size_t rows(const auto &m) { return m.rows(); }
    static std::size_t cols(const auto &m) { return m.cols(); }
    static std::size_t col_stride(const auto &m) { return m.rows(); }
};
```

`cube_view_traits` additionally provides `slices` and `slice_stride`.

### Armadillo (`<vpp/mat_view_arma.hpp>`)

| Source | View | Temporaries |
|--------|------|-------------|
| `Mat`, `Col`, `Row` and their `::fixed` variants | `mat_view` | rejected |
| `subview`, `subview_col`, `subview_row` (`rows`, `cols`, `submat`, `col`, `row`, `head_rows`, ...) | `mat_view` | accepted |
| `Cube` | `cube_view` | rejected |
| `subview_cube` (`subcube`, `slices`, `tube`, ...) | `cube_view` | accepted |

`Cube::slice(s)` returns a `Mat &`, so it views as a `mat_view`.

## API — `mat_view<T>`

| Expression | Description |
|-----------|-------------|
| `mat_view<T>()` | Empty view (`data() == nullptr`). |
| `mat_view<T>(p, rows, cols, col_stride)` | View of strided storage. Pre: `col_stride >= rows \|\| cols <= 1`. |
| `mat_view<T>(p, rows, cols)` | Contiguous storage (`col_stride == rows`). |
| `mat_view<const T>(mat_view<T>)` | Implicit conversion to read-only. |
| `mat_view<T>(m)` | Implicit conversion from an adapted matrix (see above). |
| `data()`, `rows()`, `cols()`, `col_stride()` | Pointer to `(0, 0)` and the shape. |
| `size()`, `empty()` | `rows() * cols()`; true when either is 0. |
| `is_contiguous()` | True when the elements form one unbroken block of `size()` elements. |
| `v(r, c)`, `v[r, c]` | Element reference. `v[r, c]` needs C++23 multidimensional subscript. |
| `col_ptr(c)` | Pointer to the top of column `c`. |
| `col(c)` | Column `c` as `std::span<T>`. |
| `block(r0, c0, nr, nc)` | `nr` x `nc` block at `(r0, c0)`. |
| `row_range(r0, n)`, `rows_from(r0)` | Rows `[r0, r0 + n)` / `[r0, rows())`, all columns. |
| `col_range(c0, n)`, `cols_from(c0)` | Columns `[c0, c0 + n)` / `[c0, cols())`, all rows. |
| `row(r)` | Row `r` as a 1 x `cols()` view (a strided gather when iterated). |

## API — `cube_view<T>`

| Expression | Description |
|-----------|-------------|
| `cube_view<T>(p, rows, cols, slices, col_stride, slice_stride)` | View of strided storage; slices must not overlap. |
| `cube_view<T>(p, rows, cols, slices)` | Contiguous storage. |
| `cube_view<const T>(cube_view<T>)`, `cube_view<T>(m)` | As for `mat_view`. |
| `data()`, `rows()`, `cols()`, `slices()`, `col_stride()`, `slice_stride()` | Pointer and shape. |
| `size()`, `empty()`, `is_contiguous()` | As for `mat_view`. |
| `v(r, c, s)`, `v[r, c, s]` | Element reference. |
| `slice(s)` | Slice `s` as `mat_view<T>`. |
| `block(r0, c0, s0, nr, nc, ns)` | Sub-cube at `(r0, c0, s0)`. |
| `row_range(r0, n)`, `col_range(c0, n)`, `slice_range(s0, n)` | Range along one dimension, all of the others. |

## API — free functions

| Expression | Description |
|-----------|-------------|
| `vpp::as_view(m)` | `mat_view` or `cube_view` of an adapted `m`, element type and constness deduced from `m`. |
