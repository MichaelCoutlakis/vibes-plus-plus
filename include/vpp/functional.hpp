/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace vpp
{

// A C++17 backport of C++20's std::bind_front.
//
// bind_front(f, bound...) returns a call wrapper that stores a decay-copy of
// the callable f and of each leading (bound) argument. Invoking the wrapper
// calls f with the bound arguments followed by whatever is passed at the call
// site, which are perfectly forwarded:
//
//   g(call...)  ==  std::invoke(f, bound..., call...)
//
// Because std::invoke is the underlying primitive, f may be a free function
// pointer, a functor/lambda, or a pointer to member (function or data); in the
// member case the object is supplied as the first bound argument:
//
//   auto cb = vpp::bind_front(&my_class::my_member, this);
//   cb(a, b);                       // this->my_member(a, b)
//   auto h  = vpp::bind_front(&free_fn);
//   h(x, y, z);                     // free_fn(x, y, z)
//
// Notes / caveats (shared with std::bind_front):
//   - Bound arguments are stored by decay-copy. To bind a reference, wrap it
//     with std::ref / std::cref.
//   - Binding a raw pointer (e.g. this) stores a non-owning pointer; the
//     pointee must outlive the wrapper.
//   - The wrapper's value category propagates to the stored callable and
//     bound arguments: invoking an rvalue wrapper moves them into the call.
//
// On a standard library that provides std::bind_front (C++20 and later, as
// reported by the __cpp_lib_bind_front feature-test macro), this header simply
// pulls it into namespace vpp, so vpp::bind_front *is* std::bind_front. The
// hand-written backport below is compiled only on C++17.

#if defined(__cpp_lib_bind_front) && __cpp_lib_bind_front >= 201907L

using std::bind_front;

#else

namespace detail
{

template <typename F, typename Bound, std::size_t... I, typename... CallArgs>
constexpr decltype(auto) bind_front_call(
    F &&f,
    Bound &&bound,
    std::index_sequence<I...>,
    CallArgs &&...call_args)
{
    return std::invoke(
        std::forward<F>(f),
        std::get<I>(std::forward<Bound>(bound))...,
        std::forward<CallArgs>(call_args)...);
}

} // namespace detail

template <typename F, typename... BoundArgs>
class bind_front_t
{
    using indices = std::index_sequence_for<BoundArgs...>;

public:
    // Constructs the wrapper from the callable and bound arguments. The SFINAE
    // guard keeps this from hijacking the copy/move constructors when invoked
    // with a single bind_front_t argument.
    template <
        typename G,
        typename... Args,
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<G>, bind_front_t>>>
    explicit bind_front_t(G &&g, Args &&...args) :
        m_f(std::forward<G>(g)),
        m_bound(std::forward<Args>(args)...)
    {
    }

    template <typename... CallArgs>
    constexpr decltype(auto) operator()(CallArgs &&...call_args) &
    {
        return detail::bind_front_call(
            m_f,
            m_bound,
            indices{},
            std::forward<CallArgs>(call_args)...);
    }

    template <typename... CallArgs>
    constexpr decltype(auto) operator()(CallArgs &&...call_args) const &
    {
        return detail::bind_front_call(
            m_f,
            m_bound,
            indices{},
            std::forward<CallArgs>(call_args)...);
    }

    template <typename... CallArgs>
    constexpr decltype(auto) operator()(CallArgs &&...call_args) &&
    {
        return detail::bind_front_call(
            std::move(m_f),
            std::move(m_bound),
            indices{},
            std::forward<CallArgs>(call_args)...);
    }

    template <typename... CallArgs>
    constexpr decltype(auto) operator()(CallArgs &&...call_args) const &&
    {
        return detail::bind_front_call(
            std::move(m_f),
            std::move(m_bound),
            indices{},
            std::forward<CallArgs>(call_args)...);
    }

private:
    F m_f;
    std::tuple<BoundArgs...> m_bound;
};

// Factory: decay-copies the callable and each bound argument, matching the
// semantics of std::bind_front.
template <typename F, typename... BoundArgs>
constexpr auto bind_front(F &&f, BoundArgs &&...bound_args)
{
    return bind_front_t<std::decay_t<F>, std::decay_t<BoundArgs>...>(
        std::forward<F>(f),
        std::forward<BoundArgs>(bound_args)...);
}

#endif // __cpp_lib_bind_front

} // namespace vpp
