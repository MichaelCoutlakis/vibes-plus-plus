/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once

// Short, namespace-free helper macros. Macros leak across the whole
// translation unit, so they live in their own header rather than mixed in with
// the vpp:: function objects, and the unprefixed alias (FWD) is opt-in.
//
// VPP_FWD(x) forwards x according to its declared value category, equivalent to
// std::forward<decltype(x)>(x) but without naming the type or including
// <utility>. It is meant for forwarding-reference parameters: for such a
// parameter decltype(x) is T& or T&&, and the &&-cast collapses to the right
// reference. Used on a plain (non-reference) variable it will MOVE, by design.
//
//   auto wrap = [](auto &&f, auto &&...args)
//   {
//       return std::invoke(VPP_FWD(f), VPP_FWD(args)...);
//   };
//
// There is deliberately no VPP_MOV: it would be identical to std::move, which is
// already short and universally recognized — reach for std::move directly.

#define VPP_FWD(x) static_cast<decltype(x) &&>(x)

// Opt in to the unprefixed spelling by defining VPP_DEFINE_SHORT_MACROS before
// including this header. The alias is guarded so it yields to any existing
// definition rather than clobbering it.
#ifdef VPP_DEFINE_SHORT_MACROS
#  ifndef FWD
#    define FWD(x) VPP_FWD(x)
#  endif
#endif
