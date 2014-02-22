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

#ifndef CAPNP_ALTCXX_PROPERTY_RPC_H_
#define CAPNP_ALTCXX_PROPERTY_RPC_H_

#include "property.h"
#include "impl-rpc.h"

namespace capnp {
namespace altcxx {

template <uint offset, typename Union, typename Parent>
struct ClientPointer {
  class Client : public Capability::Client {
  public:
    Client(kj::Own<ClientHook>&& hook) : Capability::Client(kj::mv(hook)) {}

    template <typename Params, typename Results>
    Request<Params, Results> newCall(uint64_t iId, uint16_t mId, kj::Maybe<MessageSize> hint) {
      return Capability::Client::newCall<Params, Results>(iId, mId, hint);
    }
  };

  template <typename = void>
  class ClientBase {
  protected:
    template <typename Params, typename Results>
    Request<Params, Results> newCall(uint64_t iId, uint16_t mId, kj::Maybe<MessageSize> hint) {
      typename Parent::Helper::Struct s = Parent::transform(this);
      Union::check(s);
      typename Parent::Helper::Pointer ptr = s.getPointerField(offset);
      return Client(ptr.getCapability()).newCall<Params, Results>(iId, mId, hint);
    }
  };
};

template <typename Impl, uint offset, typename T, typename Union = NotInUnion>
struct InterfaceProperty: PointerProperty<Impl, offset, T, NoDefault, Union>,
    public T::template ClientBase<typename T::Extends::template Add<
        ClientPointer<offset, Union, typename Impl::Transform>>> {
  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(typename T::Client&& val) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    _::PointerHelpers<T>::set(s.getPointerField(offset), kj::mv(val));
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(typename T::Client& val) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    _::PointerHelpers<T>::set(s.getPointerField(offset), val);
  }

  template <typename U, typename... A>
  typename U::Client castAs(A&&... a) { return this->get().castAs<U>(kj::fwd<A>(a)...); }

  InterfaceProperty& operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); return *this; }
  InterfaceProperty& operator = (typename T::Client& val) { set(val); return *this; }
  InterfaceProperty& operator = (typename T::Client&& val) { set(kj::mv(val)); return *this; }

  template <typename U>
  InterfaceProperty& operator = (U&& val) {
    return *this = typename T::Client(kj::mv(val));
  }
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_PROPERTY_RPC_H_
