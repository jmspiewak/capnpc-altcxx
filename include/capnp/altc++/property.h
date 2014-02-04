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

enum PropertyOf {
  READER,
  BUILDER
};

template <typename T, PropertyOf p>
using ReaderOrBuilder = Conditional<p == READER, ReaderFor<T>, BuilderFor<T>>;

template <PropertyOf p>
using PointerRoB = Conditional<p == READER, _::PointerReader, _::PointerBuilder>;

template <PropertyOf p>
using StructRoB  = Conditional<p == READER, _::StructReader,  _::StructBuilder>;

// =======================================================================================
// Element type abstraction for containters.

template <typename T, PropertyOf p>
struct ElementType_ {
  typedef decltype(kj::instance<ReaderOrBuilder<T, p>>()[0]) Type;
};

template <typename T, PropertyOf p>
struct ElementType_<List<T>, p> {
  typedef ReaderOrBuilder<T, p> Type;
  // We can't decltype it because some element types may be incomplete at this point
};

template <typename T, PropertyOf p>
using ElementType = typename ElementType_<T, p>::Type;

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
  template <typename T, PropertyOf p>
  static ReaderOrBuilder<T, p> get(PointerRoB<p> ptr) {
    return _::PointerHelpers<T>::get(ptr);
  }
};

template <const _::RawSchema* raw, uint offset, uint size = 0>
struct PointerDefault {
  template <typename T, PropertyOf p>
  static ReaderOrBuilder<T, p> get(PointerRoB<p> ptr) {
    return _::PointerHelpers<T>::get(ptr, raw->encodedNode + offset, size);
  }
};

template <const _::RawSchema* raw, uint offset>
struct PointerDefault<raw, offset, 0> {
  template <typename T, PropertyOf p>
  static ReaderOrBuilder<T, p> get(PointerRoB<p> ptr) {
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

template <typename RoB, uint offset, typename T, typename SafeMask<T>::Type mask>
struct MaybeMasked {
  static T    get(RoB& rob) { return rob.template getDataField<T>(offset * ELEMENTS, mask); }
  static void set(RoB& rob, T val) { rob.template setDataField<T>(offset * ELEMENTS, val, mask); }
};

template <typename RoB, uint offset, typename T>
struct MaybeMasked<RoB, offset, T, 0> {
  static T    get(RoB& rob) { return rob.template getDataField<T>(offset * ELEMENTS); }
  static void set(RoB& rob, T val) { rob.template setDataField<T>(offset * ELEMENTS, val); }
};

template <typename RoB>
struct MaybeMasked<RoB, 0, Void, 0> {
  static Void get(RoB&) { return VOID; }
  static void set(RoB&, Void) {}
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
// The base of all properties.

template <PropertyOf p>
struct PropertyBase: protected StructRoB<p> {
  KJ_DISALLOW_COPY(PropertyBase);

protected:
  StructRoB<p>& cast() {
    // Useful when *this can't be implicitly cast
    return *static_cast<StructRoB<p>*>(this);
  }
};

// =======================================================================================
// Shared property types used directly by Readers and inherited by Builder properties.

template <uint offset, typename T, typename SafeMask<T>::Type mask = 0,
          typename MaybeInUnion = NotInUnion, PropertyOf p = READER>
struct ConstPrimitiveProperty: PropertyBase<p> {
  operator T () {
    MaybeInUnion::check(this->cast());
    return MaybeMasked<StructRoB<p>, offset, T, mask>::get(*this);
  }
};

template <typename T, typename MaybeInUnion = NotInUnion, PropertyOf p = READER>
struct ConstGroupProperty: PropertyBase<p> {
  ReaderOrBuilder<T, p> get() {
    MaybeInUnion::check(this->cast());
    return ReaderOrBuilder<T, p>(*this);
  }

  ReaderOrBuilder<T, p> operator * () { return get(); }
  _::TemporaryPointer<ReaderOrBuilder<T, p>> operator -> () { return get(); }
};

template <uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion, PropertyOf p = READER>
struct ConstPointerProperty: PropertyBase<p> {
  ReaderOrBuilder<T, p> get() {
    MaybeInUnion::check(this->cast());
    return MaybeDefault::template get<T, p>(pointer());
  }

  bool isNull() {
    if (!MaybeInUnion::isThis(this->cast())) return true;
    return pointer().isNull();
  }

  explicit operator bool () { return !isNull(); }
  bool operator == (decltype(nullptr)) { return isNull(); }
  bool operator != (decltype(nullptr)) { return !isNull(); }

  ReaderOrBuilder<T, p> operator * () { return get(); }
  _::TemporaryPointer<ReaderOrBuilder<T, p>> operator -> () { return get(); }

protected:
  PointerRoB<p> pointer() {
    return this->getPointerField(offset * POINTERS);
  }
};

template <uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion, PropertyOf p = READER>
struct ConstContainerProperty: ConstPointerProperty<offset, T, MaybeDefault, MaybeInUnion, p> {
  ElementType<T, p> operator [] (size_t n) {
    return this->get()[n];
  }
};

template <uint offset, typename MaybeInUnion = NotInUnion, PropertyOf p = READER>
using ConstAnyPointerProperty = ConstPointerProperty<offset, AnyPointer, NoDefault,
                                                     MaybeInUnion, p>;

// =======================================================================================
// Builder property types.

template <uint offset, typename T, typename SafeMask<T>::Type mask = 0,
          typename MaybeInUnion = NotInUnion>
struct PrimitiveProperty: ConstPrimitiveProperty<offset, T, mask, MaybeInUnion, BUILDER> {
  T operator = (T val) {
    MaybeInUnion::setDiscriminant(*this);
    MaybeMasked<_::StructBuilder, offset, T, mask>::set(*this, val);
    return val;
  }
};

template <typename T, typename Initializer, typename MaybeInUnion = NotInUnion>
struct GroupProperty: ConstGroupProperty<T, MaybeInUnion, BUILDER> {
  typename T::Builder init() {
    MaybeInUnion::setDiscriminant(*this);
    Initializer::init(*this);
    return typename T::Builder(*this);
  }
};

template <uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct PointerProperty: ConstPointerProperty<offset, T, MaybeDefault, MaybeInUnion, BUILDER> {
  void set(ReaderFor<T> val) {
    MaybeInUnion::setDiscriminant(*this);
    _::PointerHelpers<T>::set(this->pointer(), val);
  }

  void adopt(Orphan<T>&& val) {
    MaybeInUnion::setDiscriminant(*this);
    _::PointerHelpers<T>::adopt(this->pointer(), kj::mv(val));
  }

  Orphan<T> disown() {
    MaybeInUnion::check(this->cast());
    return _::PointerHelpers<T>::disown(this->pointer());
  }

  void operator = (ReaderFor<T> val) { set(val); }
  void operator = (Orphan<T>&& val)  { adopt(kj::mv(val)); }
};

template <uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct StructProperty: PointerProperty<offset, T, MaybeDefault, MaybeInUnion> {
  typename T::Builder init() {
    MaybeInUnion::setDiscriminant(*this);
    return _::PointerHelpers<T>::init(this->pointer());
  }

  void operator = (ReaderFor<T> val) { this->set(val); }
  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
};

template <uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct ContainerProperty: PointerProperty<offset, T, MaybeDefault, MaybeInUnion> {
  typename T::Builder init(size_t n) {
    MaybeInUnion::setDiscriminant(*this);
    return _::PointerHelpers<T>::init(this->pointer(), n);
  }

  ElementType<T, BUILDER> operator [] (size_t n) { return this->get()[n]; }

  void operator = (ReaderFor<T> val) { this->set(val); }
  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
};

template <uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct ListProperty: ContainerProperty<offset, T, MaybeDefault, MaybeInUnion> {
  void set(kj::ArrayPtr<const ElementType<T, READER>> val) {
    MaybeInUnion::setDiscriminant(*this);
    _::PointerHelpers<T>::set(this->pointer(), val);
  }

  void operator = (ReaderFor<T> val) { this->set(val); }
  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
  void operator = (kj::ArrayPtr<const ElementType<T, READER>> val) { set(val); }
};

template <uint offset, typename T, typename MaybeInUnion = NotInUnion>
struct InterfaceProperty: PointerProperty<offset, T, NoDefault, MaybeInUnion> {
  void set(typename T::Client&& val) {
    MaybeInUnion::setDiscriminant(*this);
    _::PointerHelpers<T>::set(this->pointer(), kj::mv(val));
  }

  void set(typename T::Client& val) {
    MaybeInUnion::setDiscriminant(*this);
    _::PointerHelpers<T>::set(this->pointer(), val);
  }

  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
  void operator = (typename T::Client& val) { set(val); }
  void operator = (typename T::Client&& val) { set(kj::mv(val)); }
};

template <uint offset, typename MaybeInUnion = NotInUnion>
using AnyPointerProperty = PointerProperty<offset, AnyPointer, NoDefault, MaybeInUnion>;

// =======================================================================================
// Pipeline property types.

template <typename T, typename Operation = PipelineNoOp>
struct PipelineProperty: private AnyPointer::Pipeline {
  KJ_DISALLOW_COPY(PipelineProperty);

  PipelineFor<T> get() {
    return PipelineFor<T>(Operation::get(*this));
  }

  PipelineFor<T> operator * () { return get(); }
  _::TemporaryPointer<PipelineFor<T>> operator -> () { return get(); }
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_PROPERTY_H_
