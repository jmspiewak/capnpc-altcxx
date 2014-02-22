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

#ifndef CAPNP_ALTCXX_IMPL_RPC_H_
#define CAPNP_ALTCXX_IMPL_RPC_H_

#include <capnp/generated-header-support.h>
#include <capnp/capability.h>
#include "common.h"

namespace capnp {
namespace altcxx {

struct ClientRoot {
  template <typename = void>
  using ClientBase = Capability::Client;
};

template <typename T>
class Client : public T::template ClientBase<typename T::Extends::template Add<ClientRoot>> {
  typedef typename T::template ClientBase<typename T::Extends::template Add<ClientRoot>> Base;

public:
  Client() = delete;
  Client(Client&&) = default;
  Client(decltype(nullptr)) : Base(nullptr) {}
  Client(kj::Exception&& e) : Base(kj::mv(e)) {}
  Client(kj::Own<ClientHook>&& hook) : Base(kj::mv(hook)) {}
  Client(kj::Own<typename T::Server>&& server) : Base(kj::mv(server)) {}
  Client(kj::Promise<typename T::Client>&& promise) : Base(kj::mv(promise)) {}

  template <typename U, typename = kj::EnableIf<Contains<T, typename U::Extends>::VALUE>>
  Client(kj::Own<U>&& server) : Base(kj::mv(server)) {}

  template <typename U, typename = kj::EnableIf<Contains<T, typename U::Extends>::VALUE>>
  Client(kj::Promise<U>&& promise) : Base(kj::mv(promise)) {}

  Client& operator = (Client&&) = default;

  template <typename U>
  Client& operator = (U&& val) {
    return *this = Client(kj::fwd<U>(val));
  }
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_IMPL_RPC_H_

