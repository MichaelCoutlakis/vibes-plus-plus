/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <armadillo>
#include <concepts>
#include <cstddef>
#include <vpp/mat_view.hpp>

// Armadillo adapters for mat_view / cube_view. Including this header makes
//
//   arma::Mat, Col, Row (and their ::fixed variants)  -> mat_view (lvalues only)
//   arma::subview, subview_col, subview_row           -> mat_view (temporaries too)
//   arma::Cube                                        -> cube_view (lvalues only)
//   arma::subview_cube                                -> cube_view (temporaries too)
//
// convert implicitly, with constness following the source. Expression
// templates (e.g. `A * 2`) have no memory to view and do not convert.

namespace vpp
{

namespace detail
{

template <typename M>
concept arma_mat = std::derived_from<M, arma::Mat<typename M::elem_type>>;

template <typename M>
concept arma_subview = std::derived_from<M, arma::subview<typename M::elem_type>>;

template <typename M>
concept arma_cube = std::derived_from<M, arma::Cube<typename M::elem_type>>;

template <typename M>
concept arma_subview_cube = std::derived_from<M, arma::subview_cube<typename M::elem_type>>;

} // namespace detail

template <detail::arma_mat M>
struct mat_view_traits<M>
{
    static constexpr bool borrowed = false;
    static auto *data(auto &m) noexcept { return m.memptr(); }
    static std::size_t rows(const auto &m) noexcept { return m.n_rows; }
    static std::size_t cols(const auto &m) noexcept { return m.n_cols; }
    static std::size_t col_stride(const auto &m) noexcept { return m.n_rows; }
};

template <detail::arma_subview M>
struct mat_view_traits<M>
{
    static constexpr bool borrowed = true;
    static auto *data(auto &sv) noexcept { return sv.colptr(0); }
    static std::size_t rows(const auto &sv) noexcept { return sv.n_rows; }
    static std::size_t cols(const auto &sv) noexcept { return sv.n_cols; }
    static std::size_t col_stride(const auto &sv) noexcept { return sv.m.n_rows; }
};

template <detail::arma_cube M>
struct cube_view_traits<M>
{
    static constexpr bool borrowed = false;
    static auto *data(auto &m) noexcept { return m.memptr(); }
    static std::size_t rows(const auto &m) noexcept { return m.n_rows; }
    static std::size_t cols(const auto &m) noexcept { return m.n_cols; }
    static std::size_t slices(const auto &m) noexcept { return m.n_slices; }
    static std::size_t col_stride(const auto &m) noexcept { return m.n_rows; }
    static std::size_t slice_stride(const auto &m) noexcept { return m.n_elem_slice; }
};

template <detail::arma_subview_cube M>
struct cube_view_traits<M>
{
    static constexpr bool borrowed = true;
    static auto *data(auto &sv) noexcept { return sv.slice_colptr(0, 0); }
    static std::size_t rows(const auto &sv) noexcept { return sv.n_rows; }
    static std::size_t cols(const auto &sv) noexcept { return sv.n_cols; }
    static std::size_t slices(const auto &sv) noexcept { return sv.n_slices; }
    static std::size_t col_stride(const auto &sv) noexcept { return sv.m.n_rows; }
    static std::size_t slice_stride(const auto &sv) noexcept { return sv.m.n_elem_slice; }
};

} // namespace vpp
