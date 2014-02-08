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

#ifndef CAPNP_ALTCXX_GENERATED_HEADER_SUPPORT_H
#define CAPNP_ALTCXX_GENERATED_HEADER_SUPPORT_H

#include "property.h"

namespace capnp {
namespace altcxx {

struct ReaderPropertyImpl {
  static constexpr bool isConst = true;
  typedef _::StructReader Struct;
  typedef _::PointerReader Pointer;

  template <typename T>
  using TypeFor = ReaderFor<T>;

  template <typename T>
  static Struct& asStruct(T* ptr) {
    return *reinterpret_cast<Struct*>(ptr);
  }
};

struct BuilderPropertyImpl {
  static constexpr bool isConst = false;
  typedef _::StructBuilder Struct;
  typedef _::PointerBuilder Pointer;

  template <typename T>
  using TypeFor = BuilderFor<T>;

  template <typename T>
  static Struct& asStruct(T* ptr) {
    return *reinterpret_cast<Struct*>(ptr);
  }
};

template <typename Friend>
struct PrivateReader: private _::StructReader {
  friend Friend;

  typedef ReaderPropertyImpl PropertyImpl;

  template <typename T, Kind k>
  friend struct ::capnp::ToDynamic_;
  template <typename T, Kind k>
  friend struct ::capnp::_::PointerHelpers;
  template <typename T, Kind k>
  friend struct ::capnp::List;
  friend class ::capnp::MessageBuilder;
  friend class ::capnp::Orphanage;

  PrivateReader() = default;
  PrivateReader(const StructReader& r): StructReader(r) {}

private:
  _::StructReader asReader() const { return *this; }
};

template <typename Friend>
struct PrivateBuilder: private _::StructBuilder {
  friend Friend;

  typedef BuilderPropertyImpl PropertyImpl;

  template <typename T, Kind k>
  friend struct ::capnp::ToDynamic_;
  friend class ::capnp::Orphanage;

  PrivateBuilder() = default;
  PrivateBuilder(StructBuilder& b): StructBuilder(b) {}
  PrivateBuilder(StructBuilder&& b): StructBuilder(b) {}

private:
  _::StructReader asReader() const { return StructBuilder::asReader(); }
};

template <typename Friend>
struct PrivatePipeline: private AnyPointer::Pipeline {
  friend Friend;

  template <typename T, Kind k>
  friend struct ::capnp::ToDynamic_;

  PrivatePipeline(decltype(nullptr)): AnyPointer::Pipeline(nullptr) {}
  PrivatePipeline(AnyPointer::Pipeline&& p): AnyPointer::Pipeline(kj::mv(p)) {}
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_GENERATED_HEADER_SUPPORT_H
