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

#ifndef CAPNP_ALTCXX_PROPERTY_H_
#define CAPNP_ALTCXX_PROPERTY_H_

#include "common.h"

namespace capnp {
namespace altcxx {

// =======================================================================================
// T::{Reader, Builder}, Struct{Reader, Builder}, Pointer{Reader, Builder} abstraction.

struct Reader {
  static constexpr bool isConst = true;
  typedef _::StructReader Struct;
  typedef _::PointerReader Pointer;

  template <typename T>
  using TypeFor = ReaderFor<T>;

  template <typename T>
  static Struct& asStruct(T* ptr) {
    // ptr must point to an object which is in union with a PrivateReader
    return *reinterpret_cast<Struct*>(ptr);
  }
};

struct Builder {
  static constexpr bool isConst = false;
  typedef _::StructBuilder Struct;
  typedef _::PointerBuilder Pointer;

  template <typename T>
  using TypeFor = BuilderFor<T>;

  template <typename T>
  static Struct& asStruct(T* ptr) {
    // ptr must point to an object which is in union with a PrivateBuilder
    return *reinterpret_cast<Struct*>(ptr);
  }
};

// =======================================================================================
// Union member trait.

struct NotInUnion {
  template <typename T> static uint16_t which(T&) { return -1; }
  template <typename T> static bool isThis(T&) { return true; }
  template <typename T> static void check(T&) {}
  template <typename T> static void setDiscriminant(T&) {}
};

template <uint32_t discriminantOffset, uint16_t discriminantValue>
struct InUnion {
  template <typename T>
  static uint16_t which(T& ref) {
    return ref.template getDataField<uint16_t>(discriminantOffset * ELEMENTS);
  }

  template <typename T>
  static bool isThis(T& ref) {
    return which(ref) == discriminantValue;
  }

  template <typename T>
  static void check(T& ref) {
    KJ_IREQUIRE(isThis(ref), "Must check which() before accessing a union member.");
  }

  static void setDiscriminant(_::StructBuilder& ref) {
    ref.setDataField<uint16_t>(discriminantOffset, discriminantValue);
  }
};

// =======================================================================================
// Default value trait for pointer fields.

struct NoDefault {
  template <typename T, typename Impl>
  static typename Impl::template TypeFor<T> get(typename Impl::Pointer ptr) {
    return _::PointerHelpers<T>::get(ptr);
  }
};

template <const _::RawSchema* raw, uint offset, uint size = 0>
struct PointerDefault {
  template <typename T, typename Impl>
  static typename Impl::template TypeFor<T> get(typename Impl::Pointer ptr) {
    return _::PointerHelpers<T>::get(ptr, raw->encodedNode + offset, size);
  }
};

template <const _::RawSchema* raw, uint offset>
struct PointerDefault<raw, offset, 0> {
  template <typename T, typename Impl>
  static typename Impl::template TypeFor<T> get(typename Impl::Pointer ptr) {
    return _::PointerHelpers<T>::get(ptr, raw->encodedNode + offset);
  }
};

// =======================================================================================
// Group initializer and its operations.

template <uint offset>
struct ClearPointer {
  static bool init(_::StructBuilder& builder) {
    builder.getPointerField(offset * POINTERS).clear();
    return true;
  }
};

template <uint offset, typename T>
struct ZeroElement {
  static bool init(_::StructBuilder& builder) {
    builder.setDataField<T>(offset * ELEMENTS, 0);
    return true;
  }
};

struct NoOp {
  static bool init(_::StructBuilder&) { return true; }
};

template <typename... FieldInitializers>
struct GroupInitializer {
  static void init(_::StructBuilder& builder) {
    doAll(FieldInitializers::init(builder)...);
  }

  static void doAll(...) {}
};

// =======================================================================================
// Primitive field masking.

template <typename T> struct SafeMask { typedef _::Mask<T> Type; };
template <> struct SafeMask<Void> { typedef uint Type; };

template <typename Struct, uint offset, typename T, typename SafeMask<T>::Type mask>
struct MaybeMasked {
  static T    get(Struct& s) { return s.template getDataField<T>(offset * ELEMENTS, mask); }
  static void set(Struct& s, T val) { s.template setDataField<T>(offset * ELEMENTS, val, mask); }
};

template <typename Struct, uint offset, typename T>
struct MaybeMasked<Struct, offset, T, 0> {
  static T    get(Struct& s) { return s.template getDataField<T>(offset * ELEMENTS); }
  static void set(Struct& s, T val) { s.template setDataField<T>(offset * ELEMENTS, val); }
};

template <typename Struct>
struct MaybeMasked<Struct, 0, Void, 0> {
  static Void get(Struct&) { return VOID; }
  static void set(Struct&, Void) {}
};

// =======================================================================================
// Pipeline operations.

struct PipelineNoOp {
  static AnyPointer::Pipeline get(AnyPointer::Pipeline& ptr) { return ptr.noop(); }
};

template <uint offset>
struct PipelineGetPointerOp {
  static AnyPointer::Pipeline get(AnyPointer::Pipeline& ptr) { return ptr.getPointerField(offset); }
};

template <uint offset>
struct PipelineGetPointerAsCapOp {
  static kj::Own<ClientHook> get(AnyPointer::Pipeline& ptr) {
    return ptr.getPointerField(offset).asCap();
  }
};

// =======================================================================================
// Property types.

template <typename Impl, uint offset, typename T, typename SafeMask<T>::Type mask = 0,
          typename MaybeInUnion = NotInUnion>
struct PrimitiveProperty {
  KJ_DISALLOW_COPY(PrimitiveProperty);

  operator T () {
    MaybeInUnion::check(Impl::asStruct(this));
    return MaybeMasked<typename Impl::Struct, offset, T, mask>::get(Impl::asStruct(this));
  }

  template <typename = kj::EnableIf<!Impl::isConst>>
  T operator = (T val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    MaybeMasked<typename Impl::Struct, offset, T, mask>::set(Impl::asStruct(this), val);
    return val;
  }
};

template <typename Impl, typename T, typename Initializer, typename MaybeInUnion = NotInUnion>
struct GroupProperty {
  KJ_DISALLOW_COPY(GroupProperty);

  typename Impl::template TypeFor<T> get() {
    MaybeInUnion::check(Impl::asStruct(this));
    return typename Impl::template TypeFor<T>(Impl::asStruct(this));
  }

  template <typename = kj::EnableIf<!Impl::isConst>>
  typename Impl::template TypeFor<T> init() {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    Initializer::init(Impl::asStruct(this));
    return typename Impl::template TypeFor<T>(Impl::asStruct(this));
  }

  typename Impl::template TypeFor<T> operator * () { return get(); }
  _::TemporaryPointer<typename Impl::template TypeFor<T>> operator -> () { return get(); }
};

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct PointerProperty {
  KJ_DISALLOW_COPY(PointerProperty);

  typename Impl::template TypeFor<T> get() {
    MaybeInUnion::check(Impl::asStruct(this));
    return MaybeDefault::template get<T, Impl>(pointer());
  }

  bool isNull() {
    if (!MaybeInUnion::isThis(Impl::asStruct(this))) return true;
    return pointer().isNull();
  }

  template <typename = kj::EnableIf<!Impl::isConst>>
  void set(ReaderFor<T> val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::set(pointer(), val);
  }

  template <typename = kj::EnableIf<!Impl::isConst>>
  void adopt(Orphan<T>&& val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::adopt(pointer(), kj::mv(val));
  }

  template <typename = kj::EnableIf<!Impl::isConst>>
  Orphan<T> disown() {
    MaybeInUnion::check(Impl::asStruct(this));
    return _::PointerHelpers<T>::disown(pointer());
  }

  template <typename = kj::EnableIf<!Impl::isConst && kind<T>() == Kind::STRUCT>>
  BuilderFor<T> init() {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    return _::PointerHelpers<T>::init(this->pointer());
  }

  template <typename = kj::EnableIf<!Impl::isConst && (kind<T>() == Kind::BLOB ||
                                    kind<T>() == Kind::LIST)>>
  BuilderFor<T> init(size_t n) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    return _::PointerHelpers<T>::init(this->pointer(), n);
  }

  explicit operator bool () { return !isNull(); }
  bool operator == (decltype(nullptr)) { return isNull(); }
  bool operator != (decltype(nullptr)) { return !isNull(); }

  void operator = (ReaderFor<T> val) { set(val); }
  void operator = (Orphan<T>&& val)  { adopt(kj::mv(val)); }

  typename Impl::template TypeFor<T> operator * () { return get(); }
  _::TemporaryPointer<typename Impl::template TypeFor<T>> operator -> () { return get(); }

protected:
  typename Impl::Pointer pointer() {
    return Impl::asStruct(this).getPointerField(offset * POINTERS);
  }
};

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct BlobProperty: PointerProperty<Impl, offset, T, MaybeDefault, MaybeInUnion> {
  auto operator [] (size_t n) -> decltype(this->get()[n]) { return this->get()[n]; }

  void operator = (ReaderFor<T> val) { this->set(val); }
  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
};

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct ListProperty: PointerProperty<Impl, offset, List<T>, MaybeDefault, MaybeInUnion> {
  template <typename = kj::EnableIf<!Impl::isConst>>
  void set(kj::ArrayPtr<const ReaderFor<T>> val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<List<T>>::set(this->pointer(), val);
  }

  typename Impl::template TypeFor<T> operator [] (size_t n) { return this->get()[n]; }

  void operator = (ReaderFor<List<T>> val) { this->set(val); }
  void operator = (Orphan<List<T>>&& val)  { this->adopt(kj::mv(val)); }
  void operator = (kj::ArrayPtr<const ReaderFor<T>> val) { set(val); }
};

template <typename Impl, uint offset, typename T, typename MaybeInUnion = NotInUnion>
struct InterfaceProperty: PointerProperty<Impl, offset, T, NoDefault, MaybeInUnion> {
  template <typename = kj::EnableIf<!Impl::isConst>>
  void set(typename T::Client&& val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::set(this->pointer(), kj::mv(val));
  }

  template <typename = kj::EnableIf<!Impl::isConst>>
  void set(typename T::Client& val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::set(this->pointer(), val);
  }

  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
  void operator = (typename T::Client& val) { set(val); }
  void operator = (typename T::Client&& val) { set(kj::mv(val)); }
};

template <typename Impl, uint offset, typename MaybeInUnion = NotInUnion>
using AnyPointerProperty = PointerProperty<Impl, offset, AnyPointer, NoDefault, MaybeInUnion>;

template <typename T, typename Operation = PipelineNoOp>
struct PipelineProperty {
  KJ_DISALLOW_COPY(PipelineProperty);

  PipelineFor<T> get() {
    return PipelineFor<T>(Operation::get(typeless()));
  }

  PipelineFor<T> operator * () { return get(); }
  _::TemporaryPointer<PipelineFor<T>> operator -> () { return get(); }

private:
  typename AnyPointer::Pipeline& typeless() {
    return *reinterpret_cast<AnyPointer::Pipeline*>(this);
  }
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_PROPERTY_H_
