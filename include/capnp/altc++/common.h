// Copyright (c) 2014, Jakub Spiewak <j.m.spiewak@gmail.com>
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef CAPNP_ALTCXX_COMMON_H_
#define CAPNP_ALTCXX_COMMON_H_

#include <kj/common.h>

namespace capnp {
namespace altcxx {

template <bool b, typename T, typename F> struct Conditional_ {typedef T Type;};
template <typename T, typename F> struct Conditional_<false, T, F> {typedef F Type;};
template <bool b, typename T, typename F> using Conditional = typename Conditional_<b, T, F>::Type;

template <typename T, typename U> struct Same { static constexpr bool VALUE = false; };
template <typename T> struct Same<T, T> { static constexpr bool VALUE = true; };
template <typename T, typename U> inline constexpr bool same() { return Same<T, U>::VALUE; }

template <template <typename...> class Template, typename... Args>
struct Apply {
  template <typename... FreeArgs>
  using Result = Template<Args..., FreeArgs...>;
};

template <typename T, T V>
struct Constant {
  static constexpr T VALUE = V;
};

// ---------------------------------------------------------------------------------------

template <typename... Ts>
struct Types {
  template <typename... Us>
  using Add = Types<Ts..., Us...>;
};

template <typename Types1, typename Types2> struct Cat_;

template <typename... T1s, typename... T2s>
struct Cat_<Types<T1s...>, Types<T2s...>> {
  typedef Types<T1s..., T2s...> Result;
};

template <typename Types1, typename Types2>
using Cat = typename Cat_<Types1, Types2>::Result;

template <typename T, typename Types> struct Contains;

template <typename T, typename T0, typename... Ts>
struct Contains<T, Types<T0, Ts...>> {
  static constexpr bool VALUE = (same<T, T0>() || Contains<T, Types<Ts...>>::VALUE);
};

template <typename T>
struct Contains<T, Types<>> {
  static constexpr bool VALUE = false;
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_COMMON_H_
