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

#ifndef CAPNP_ALTCXX_IMPL_H_
#define CAPNP_ALTCXX_IMPL_H_

#include <capnp/generated-header-support.h>

namespace capnp {
namespace altcxx {

struct ReaderHelper {
  static constexpr bool CONST = true;
  typedef _::StructReader Struct;
  typedef _::PointerReader Pointer;

  template <typename T>
  using TypeFor = ReaderFor<T>;

  template <typename T>
  static Struct getStruct(Pointer ptr, const word* def) {
    return ptr.getStruct(def);
  }

  static _::StructReader asReader(Struct s) {
    return s;
  }
};

struct BuilderHelper {
  static constexpr bool CONST = false;
  typedef _::StructBuilder Struct;
  typedef _::PointerBuilder Pointer;

  template <typename T>
  using TypeFor = BuilderFor<T>;

  template <typename T>
  static Struct getStruct(Pointer ptr, const word* def) {
    return ptr.getStruct(_::structSize<T>(), def);
  }

  static _::StructReader asReader(Struct s) {
    return s.asReader();
  }
};

template <typename HelperT>
struct RootTransorm {
  typedef HelperT Helper;
  static constexpr int DEPTH = 0;

  static typename Helper::Struct transform(void* ptr) {
    return *reinterpret_cast<typename Helper::Struct*>(ptr);
  }
};

// =======================================================================================

template <typename Transform>
struct BasicImpl: public Transform::Helper {
  typedef typename Transform::Helper Helper;
  static constexpr int DEPTH = Transform::DEPTH;

  template <template <typename...> class NewTransform>
  using Push = BasicImpl<NewTransform<Transform> >;

  template <typename Friend>
  class UnionMember {
    friend Friend;

    template <typename T>
    T getDataField(ElementCount offset) {
      return reinterpret_cast<typename Helper::Struct*>(this)->getDataField<T>(offset);
    }

    _::StructReader asReader() {
      return Helper::asReader(*reinterpret_cast<typename Helper::Struct*>(this));
    }
  };

  template <typename T>
  static typename Helper::Struct asStruct(T* ptr) {
    return Transform::transform(reinterpret_cast<void*>(ptr));
  }
};

struct ReaderImpl: public BasicImpl<RootTransorm<ReaderHelper> > {
  template <typename Friend>
  struct UnionMember: private _::StructReader {
    friend Friend;

    template <typename T, Kind k>
    friend struct ::capnp::ToDynamic_;
    template <typename T, Kind k>
    friend struct ::capnp::_::PointerHelpers;
    template <typename T, Kind k>
    friend struct ::capnp::List;
    friend class ::capnp::MessageBuilder;
    friend class ::capnp::Orphanage;

    UnionMember() = default;
    UnionMember(const StructReader& r): StructReader(r) {}

  private:
    _::StructReader asReader() const { return *this; }
  };
};

struct BuilderImpl: public BasicImpl<RootTransorm<BuilderHelper> > {
  template <typename Friend>
  struct UnionMember: private _::StructBuilder {
    friend Friend;

    template <typename T, Kind k>
    friend struct ::capnp::ToDynamic_;
    friend class ::capnp::Orphanage;

    UnionMember() = default;
    UnionMember(StructBuilder& b): StructBuilder(b) {}
    UnionMember(StructBuilder&& b): StructBuilder(b) {}

  private:
    _::StructReader asReader() const { return StructBuilder::asReader(); }
  };
};

// =======================================================================================

template <typename Friend>
class PrivatePipeline: private AnyPointer::Pipeline {
  friend Friend;

  template <typename T, Kind k>
  friend struct ::capnp::ToDynamic_;

  PrivatePipeline(decltype(nullptr)): AnyPointer::Pipeline(nullptr) {}
  PrivatePipeline(AnyPointer::Pipeline&& p): AnyPointer::Pipeline(kj::mv(p)) {}
};

template <typename T>
struct DummyPipeline {
  typedef T Pipelines;

  DummyPipeline(decltype(nullptr)) {}
  explicit DummyPipeline(AnyPointer::Pipeline&&) {}
};

// =======================================================================================

template <typename T>
class Reader: public T::template Base<ReaderImpl> {
public:
  typedef T Reads;

  Reader() = default;
  explicit Reader(::capnp::_::StructReader base): T::template Base<ReaderImpl>(base) {}
};

template <typename T>
class Builder: public T::template Base<BuilderImpl> {
public:
  typedef T Builds;

  Builder() = delete;
  Builder(decltype(nullptr)) {}
  explicit Builder(::capnp::_::StructBuilder base): T::template Base<BuilderImpl>(base) {}
  operator typename T::Reader() const { return typename T::Reader(this->_reader()); }
  typename T::Reader asReader() const { return *this; }
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_IMPL_H_
