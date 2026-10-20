/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <cassert>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

namespace vpp
{

/// Customisation point that lets a matrix type convert implicitly to mat_view.
///
/// Leave the primary template undefined; specialise it (or constrain a partial
/// specialisation) for a matrix type `M` with:
///
///   static constexpr bool borrowed;         // may a temporary M be viewed?
///   static auto *data(auto &m);             // m is M or const M; pointer constness follows it
///   static std::size_t rows(const auto &m);
///   static std::size_t cols(const auto &m);
///   static std::size_t col_stride(const auto &m);
///
/// `borrowed` is true only for types that refer to memory they do not own
/// (e.g. a subview), so a view of a temporary of that type does not dangle.
/// See <vpp/mat_view_arma.hpp>.
template <typename M>
struct mat_view_traits;

/// As mat_view_traits, for cube_view; additionally provides
/// `slices(const auto &m)` and `slice_stride(const auto &m)`.
template <typename M>
struct cube_view_traits;

namespace detail
{

// T* -> U* is a qualification conversion only (e.g. float* -> const float*).
template <typename From, typename To>
concept qualification_convertible = std::is_convertible_v<From (*)[], To (*)[]>;

template <typename M, template <typename> class Traits>
using view_traits_t = Traits<std::remove_cvref_t<M>>;

template <typename M, template <typename> class Traits>
concept viewable = requires(std::remove_reference_t<M> &m) {
    { view_traits_t<M, Traits>::data(m) };
    requires std::is_pointer_v<decltype(view_traits_t<M, Traits>::data(m))>;
    requires std::is_lvalue_reference_v<M> || view_traits_t<M, Traits>::borrowed;
};

// Element type (with constness) that viewing M yields.
template <typename M, template <typename> class Traits>
using view_element_t = std::remove_pointer_t<decltype(view_traits_t<M, Traits>::data(
    std::declval<std::remove_reference_t<M> &>()))>;

template <typename M, typename T, template <typename> class Traits>
concept viewable_as =
    viewable<M, Traits> && qualification_convertible<view_element_t<M, Traits>, T>;

} // namespace detail

/// Non-owning view of a column-major matrix, or of a block of one.
///
/// Element `(r, c)` lives at `data()[r + c * col_stride()]`; each column is
/// contiguous, and `col_stride() > rows()` describes a block of a taller
/// matrix (BLAS's "leading dimension"). `mat_view<float>` is writable,
/// `mat_view<const float>` read-only, and the former converts to the latter.
///
/// Cheap to copy (a pointer and three sizes), so pass by value. Constness is
/// shallow, as for std::span: a `const mat_view<float>` still writes elements.
///
/// @warning The view dangles if the underlying matrix is resized or destroyed.
template <typename T>
class mat_view
{
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;

    constexpr mat_view() noexcept = default;

    /// @pre `col_stride >= rows || cols <= 1`
    constexpr mat_view(T *data, size_type rows, size_type cols, size_type col_stride) noexcept :
        m_data(data),
        m_rows(rows),
        m_cols(cols),
        m_col_stride(col_stride)
    {
        assert(col_stride >= rows || cols <= 1);
    }

    /// Contiguous matrix: `col_stride == rows`.
    constexpr mat_view(T *data, size_type rows, size_type cols) noexcept :
        mat_view(data, rows, cols, rows)
    {
    }

    template <typename U>
        requires detail::qualification_convertible<U, T>
    constexpr mat_view(const mat_view<U> &other) noexcept :
        mat_view(other.data(), other.rows(), other.cols(), other.col_stride())
    {
    }

    /// Implicit view of any matrix with a mat_view_traits specialisation, so a
    /// `mat_view` parameter accepts the matrix directly. Temporaries are
    /// accepted only when the traits mark the type `borrowed` (e.g. a subview).
    template <typename M>
        requires detail::viewable_as<M, T, mat_view_traits>
    constexpr mat_view(M &&m) noexcept :
        mat_view(
            detail::view_traits_t<M, mat_view_traits>::data(m),
            detail::view_traits_t<M, mat_view_traits>::rows(m),
            detail::view_traits_t<M, mat_view_traits>::cols(m),
            detail::view_traits_t<M, mat_view_traits>::col_stride(m))
    {
    }

    // ---- shape ----

    constexpr T *data() const noexcept { return m_data; }
    constexpr size_type rows() const noexcept { return m_rows; }
    constexpr size_type cols() const noexcept { return m_cols; }
    constexpr size_type col_stride() const noexcept { return m_col_stride; }
    constexpr size_type size() const noexcept { return m_rows * m_cols; }
    constexpr bool empty() const noexcept { return m_rows == 0 || m_cols == 0; }

    /// True when the elements form one unbroken block of `size()` elements.
    constexpr bool is_contiguous() const noexcept
    {
        return empty() || m_cols == 1 || m_col_stride == m_rows;
    }

    // ---- element access ----

    /// @pre `r < rows() && c < cols()`
    constexpr T &operator()(size_type r, size_type c) const noexcept
    {
        assert(r < m_rows && c < m_cols);
        return m_data[r + c * m_col_stride];
    }

#if defined(__cpp_multidimensional_subscript)
    /// @pre `r < rows() && c < cols()`
    constexpr T &operator[](size_type r, size_type c) const noexcept { return (*this)(r, c); }
#endif

    /// @pre `c < cols()`
    constexpr T *col_ptr(size_type c) const noexcept
    {
        assert(c < m_cols);
        return m_data + c * m_col_stride;
    }

    /// Column `c`; contiguous even when the view is not.
    /// @pre `c < cols()`
    constexpr std::span<T> col(size_type c) const noexcept { return {col_ptr(c), m_rows}; }

    // ---- sub-views (O(1), no allocation) ----

    /// The `nr` x `nc` block whose top-left element is `(r0, c0)`.
    /// @pre `r0 + nr <= rows() && c0 + nc <= cols()`
    constexpr mat_view block(size_type r0, size_type c0, size_type nr, size_type nc) const noexcept
    {
        assert(r0 + nr <= m_rows && c0 + nc <= m_cols);
        return {m_data + r0 + c0 * m_col_stride, nr, nc, m_col_stride};
    }

    /// Rows `[r0, r0 + n)`, all columns.
    constexpr mat_view row_range(size_type r0, size_type n) const noexcept
    {
        return block(r0, 0, n, m_cols);
    }

    /// Rows `[r0, rows())`, all columns: e.g. skip the first `r0` frames of every channel.
    constexpr mat_view rows_from(size_type r0) const noexcept
    {
        assert(r0 <= m_rows);
        return row_range(r0, m_rows - r0);
    }

    /// Columns `[c0, c0 + n)`, all rows.
    constexpr mat_view col_range(size_type c0, size_type n) const noexcept
    {
        return block(0, c0, m_rows, n);
    }

    /// Columns `[c0, cols())`, all rows.
    constexpr mat_view cols_from(size_type c0) const noexcept
    {
        assert(c0 <= m_cols);
        return col_range(c0, m_cols - c0);
    }

    /// Row `r` as a 1 x cols() view.
    /// @note Its elements are `col_stride()` apart, so iterating it is a strided gather.
    constexpr mat_view row(size_type r) const noexcept { return row_range(r, 1); }

private:
    T *m_data = nullptr;
    size_type m_rows = 0;
    size_type m_cols = 0;
    size_type m_col_stride = 0;
};

template <typename M>
    requires detail::viewable<M, mat_view_traits>
mat_view(M &&) -> mat_view<detail::view_element_t<M, mat_view_traits>>;

/// Non-owning view of a column-major cube (a stack of matrices), or of a block of one.
///
/// Element `(r, c, s)` lives at `data()[r + c * col_stride() + s * slice_stride()]`.
/// Each slice is a mat_view. Copying, constness and lifetime are as for mat_view.
template <typename T>
class cube_view
{
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;

    constexpr cube_view() noexcept = default;

    /// @pre `col_stride >= rows || cols <= 1`
    /// @pre slices do not overlap: `slice_stride >= (cols - 1) * col_stride + rows`
    ///      when `slices > 1 && cols > 0`
    constexpr cube_view(
        T *data,
        size_type rows,
        size_type cols,
        size_type slices,
        size_type col_stride,
        size_type slice_stride) noexcept :
        m_data(data),
        m_rows(rows),
        m_cols(cols),
        m_slices(slices),
        m_col_stride(col_stride),
        m_slice_stride(slice_stride)
    {
        assert(col_stride >= rows || cols <= 1);
        assert(slices <= 1 || cols == 0 || slice_stride >= (cols - 1) * col_stride + rows);
    }

    /// Contiguous cube: `col_stride == rows`, `slice_stride == rows * cols`.
    constexpr cube_view(T *data, size_type rows, size_type cols, size_type slices) noexcept :
        cube_view(data, rows, cols, slices, rows, rows * cols)
    {
    }

    template <typename U>
        requires detail::qualification_convertible<U, T>
    constexpr cube_view(const cube_view<U> &other) noexcept :
        cube_view(
            other.data(),
            other.rows(),
            other.cols(),
            other.slices(),
            other.col_stride(),
            other.slice_stride())
    {
    }

    /// Implicit view of any cube with a cube_view_traits specialisation; see
    /// the matching mat_view constructor.
    template <typename M>
        requires detail::viewable_as<M, T, cube_view_traits>
    constexpr cube_view(M &&m) noexcept :
        cube_view(
            detail::view_traits_t<M, cube_view_traits>::data(m),
            detail::view_traits_t<M, cube_view_traits>::rows(m),
            detail::view_traits_t<M, cube_view_traits>::cols(m),
            detail::view_traits_t<M, cube_view_traits>::slices(m),
            detail::view_traits_t<M, cube_view_traits>::col_stride(m),
            detail::view_traits_t<M, cube_view_traits>::slice_stride(m))
    {
    }

    // ---- shape ----

    constexpr T *data() const noexcept { return m_data; }
    constexpr size_type rows() const noexcept { return m_rows; }
    constexpr size_type cols() const noexcept { return m_cols; }
    constexpr size_type slices() const noexcept { return m_slices; }
    constexpr size_type col_stride() const noexcept { return m_col_stride; }
    constexpr size_type slice_stride() const noexcept { return m_slice_stride; }
    constexpr size_type size() const noexcept { return m_rows * m_cols * m_slices; }
    constexpr bool empty() const noexcept { return m_rows == 0 || m_cols == 0 || m_slices == 0; }

    /// True when the elements form one unbroken block of `size()` elements.
    constexpr bool is_contiguous() const noexcept
    {
        return empty()
               || ((m_cols == 1 || m_col_stride == m_rows)
                   && (m_slices == 1 || m_slice_stride == m_rows * m_cols));
    }

    // ---- element access ----

    /// @pre `r < rows() && c < cols() && s < slices()`
    constexpr T &operator()(size_type r, size_type c, size_type s) const noexcept
    {
        assert(r < m_rows && c < m_cols && s < m_slices);
        return m_data[r + c * m_col_stride + s * m_slice_stride];
    }

#if defined(__cpp_multidimensional_subscript)
    /// @pre `r < rows() && c < cols() && s < slices()`
    constexpr T &operator[](size_type r, size_type c, size_type s) const noexcept
    {
        return (*this)(r, c, s);
    }
#endif

    /// Slice `s` as a matrix view.
    /// @pre `s < slices()`
    constexpr mat_view<T> slice(size_type s) const noexcept
    {
        assert(s < m_slices);
        return {m_data + s * m_slice_stride, m_rows, m_cols, m_col_stride};
    }

    // ---- sub-views (O(1), no allocation) ----

    /// The `nr` x `nc` x `ns` block whose first element is `(r0, c0, s0)`.
    /// @pre `r0 + nr <= rows() && c0 + nc <= cols() && s0 + ns <= slices()`
    constexpr cube_view block(
        size_type r0,
        size_type c0,
        size_type s0,
        size_type nr,
        size_type nc,
        size_type ns) const noexcept
    {
        assert(r0 + nr <= m_rows && c0 + nc <= m_cols && s0 + ns <= m_slices);
        return {
            m_data + r0 + c0 * m_col_stride + s0 * m_slice_stride,
            nr,
            nc,
            ns,
            m_col_stride,
            m_slice_stride};
    }

    /// Rows `[r0, r0 + n)` of every column and slice.
    constexpr cube_view row_range(size_type r0, size_type n) const noexcept
    {
        return block(r0, 0, 0, n, m_cols, m_slices);
    }

    /// Columns `[c0, c0 + n)` of every slice.
    constexpr cube_view col_range(size_type c0, size_type n) const noexcept
    {
        return block(0, c0, 0, m_rows, n, m_slices);
    }

    /// Slices `[s0, s0 + n)`.
    constexpr cube_view slice_range(size_type s0, size_type n) const noexcept
    {
        return block(0, 0, s0, m_rows, m_cols, n);
    }

private:
    T *m_data = nullptr;
    size_type m_rows = 0;
    size_type m_cols = 0;
    size_type m_slices = 0;
    size_type m_col_stride = 0;
    size_type m_slice_stride = 0;
};

template <typename M>
    requires detail::viewable<M, cube_view_traits>
cube_view(M &&) -> cube_view<detail::view_element_t<M, cube_view_traits>>;

/// View `m` as a mat_view or cube_view, with constness following `m`.
///
/// Rarely needed, since both views convert implicitly; use it where nothing
/// names the element type, e.g. when calling a function template on `T`.
template <typename M>
    requires detail::viewable<M, mat_view_traits>
constexpr auto as_view(M &&m) noexcept
{
    return mat_view<detail::view_element_t<M, mat_view_traits>>(std::forward<M>(m));
}

template <typename M>
    requires detail::viewable<M, cube_view_traits>
constexpr auto as_view(M &&m) noexcept
{
    return cube_view<detail::view_element_t<M, cube_view_traits>>(std::forward<M>(m));
}

} // namespace vpp
