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
#include "impl.h"
#include "list-utils.h"

namespace capnp {
namespace altcxx {

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
  static uint16_t which(T& s) {
    return s.template getDataField<uint16_t>(discriminantOffset * ELEMENTS);
  }

  template <typename T>
  static bool isThis(T& s) {
    return which(s) == discriminantValue;
  }

  template <typename T>
  static void check(T& s) {
    KJ_IREQUIRE(isThis(s), "Must check which() before accessing a union member.");
  }

  static void setDiscriminant(_::StructBuilder& s) {
    s.setDataField<uint16_t>(discriminantOffset, discriminantValue);
  }
};

// =======================================================================================
// Default value trait for pointer fields.

struct NoDefault {
  template <typename T, typename Impl>
  static typename Impl::template TypeFor<T> get(typename Impl::Pointer ptr) {
    return _::PointerHelpers<T>::get(ptr);
  }

  static decltype(nullptr) ptr() { return nullptr; }
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

  static const word* ptr() { return raw->encodedNode + offset; }
};

// =======================================================================================
// Group initializer and its operations.

template <uint offset>
struct ClearPointer {
  static bool init(_::StructBuilder& builder) {
    builder.getPointerField(offset).clear();
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

struct GiNoOp {
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
// Impl transformations.

template <typename Union, typename Parent>
struct GroupTransform {
  typedef typename Parent::Helper Helper;
  static constexpr int DEPTH = Parent::DEPTH;

  template <typename T>
  static typename Helper::Struct transform(T* ptr) {
    typename Helper::Struct s = Parent::transform(ptr);
    Union::check(s);
    return s;
  }
};

template <typename Off, typename Size, typename Default, typename Union, typename Parent>
struct PointerTransform {
  typedef typename Parent::Helper Helper;
  static constexpr int DEPTH = Parent::DEPTH + 1;

  template <typename T>
  static typename Helper::Struct transform(T* ptr) {
    typename Helper::Struct s = Parent::transform(ptr);
    Union::check(s);
    return Helper::getStruct(s.getPointerField(Off::VALUE), Size::value, Default::ptr());
  }
};

// =======================================================================================
// Property types.

template <typename Impl, uint offset, typename T, typename SafeMask<T>::Type mask = 0,
          typename Union = NotInUnion>
struct PrimitiveProperty {
  KJ_DISALLOW_COPY(PrimitiveProperty);

  T get() {
    auto s = Impl::asStruct(this);
    Union::check(s);
    return MaybeMasked<typename Impl::Struct, offset, T, mask>::get(s);
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(T val) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    MaybeMasked<typename Impl::Struct, offset, T, mask>::set(s, val);
  }

  operator T () { return get(); }
  T operator = (T val) { set(val); return val; }
};

template <typename Impl, typename T, typename Initializer, typename Union = NotInUnion>
struct GroupProperty: public T::template Base<typename Impl::template Push<
                             Apply<GroupTransform, Union>::template Result>> {
  KJ_DISALLOW_COPY(GroupProperty);

  typename Impl::template TypeFor<T> get() {
    auto s = Impl::asStruct(this);
    Union::check(s);
    return typename Impl::template TypeFor<T>(s);
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  typename Impl::template TypeFor<T> init() {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    Initializer::init(s);
    return typename Impl::template TypeFor<T>(s);
  }

  operator typename Impl::template TypeFor<T> () { return get(); }
};

template <typename Impl, uint offset, typename T, typename Default = NoDefault,
          typename Union = NotInUnion>
struct PointerProperty {
  KJ_DISALLOW_COPY(PointerProperty);

  typename Impl::template TypeFor<T> get() {
    auto s = Impl::asStruct(this);
    Union::check(s);
    return Default::template get<T, Impl>(s.getPointerField(offset));
  }

  ReaderFor<T> asReader() { return get(); }

  bool isNull() {
    auto s = Impl::asStruct(this);
    if (!Union::isThis(s)) return true;
    return s.getPointerField(offset).isNull();
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(ReaderFor<T> val) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    _::PointerHelpers<T>::set(s.getPointerField(offset), val);
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void adopt(Orphan<T>&& val) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    _::PointerHelpers<T>::adopt(s.getPointerField(offset), kj::mv(val));
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  Orphan<T> disown() {
    auto s = Impl::asStruct(this);
    Union::check(s);
    return _::PointerHelpers<T>::disown(s.getPointerField(offset));
  }

  bool operator == (decltype(nullptr)) { return isNull(); }
  bool operator != (decltype(nullptr)) { return !isNull(); }

  operator typename Impl::template TypeFor<T> () { return get(); }
};

// ---------------------------------------------------------------------------------------

template <typename Impl, uint offset, typename T, typename Default = NoDefault,
          typename Union = NotInUnion>
struct StructProperty: PointerProperty<Impl, offset, T, Default, Union>,
    public Conditional<(Impl::DEPTH > 4), Void,
        typename T::template Base<typename Impl::template Push<Apply<PointerTransform,
            Constant<uint, offset>, _::StructSize_<T>, Default, Union>::template Result>>> {

  template <typename = kj::EnableIf<!Impl::CONST>>
  BuilderFor<T> init() {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    return _::PointerHelpers<T>::init(s.getPointerField(offset));
  }

  StructProperty& operator = (ReaderFor<T> val) { this->set(val); return *this; }
  StructProperty& operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); return *this; }
};

#define INFER(expr) decltype(expr) { return expr; }
#define FORWARD_NULLARY(name) auto name() -> INFER(this->get().name())
#define FORWARD_UNARY(name, t0) auto name(t0 a0) -> INFER(this->get().name(a0))
#define FORWARD_BINARY(name, t0, t1) auto name(t0 a0, t1 a1) -> INFER(this->get().name(a0, a1))

template <typename Impl, uint offset, typename T, typename Default = NoDefault,
          typename Union = NotInUnion>
struct ContainerProperty: PointerProperty<Impl, offset, T, Default, Union> {
  FORWARD_NULLARY(size)

  template <typename = kj::EnableIf<!Impl::CONST>>
  BuilderFor<T> init(size_t n) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    return _::PointerHelpers<T>::init(s.getPointerField(offset), n);
  }

  template <typename U>
  bool operator == (U&& val) const { return this->get() == kj::fwd<U>(val); }

  template <typename U>
  bool operator != (U&& val) const { return this->get() == kj::fwd<U>(val); }
};

template <typename Impl, uint offset, typename T, typename Default = NoDefault,
          typename Union = NotInUnion>
struct BlobProperty: ContainerProperty<Impl, offset, T, Default, Union> {
  FORWARD_BINARY(slice, size_t, size_t)
  FORWARD_NULLARY(begin)
  FORWARD_NULLARY(end)

  auto operator [] (size_t n) -> INFER(this->get()[n])

  template <typename U,
            typename = kj::EnableIf<kj::canConvert<typename Impl::template TypeFor<T>, U>()>>
  operator U () { return this->get(); }

  BlobProperty& operator = (ReaderFor<T> val) { this->set(val); return *this; }
  BlobProperty& operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); return *this; }
};

template <typename Impl, uint offset, typename Default = NoDefault,
          typename Union = NotInUnion>
struct TextProperty: BlobProperty<Impl, offset, Text, Default, Union> {
  FORWARD_NULLARY(asArray)
  FORWARD_NULLARY(cStr)

  template <typename = kj::EnableIf<!Impl::CONST>>
  kj::StringPtr asString() { return this->get().asString(); }

#define SELF typename Impl::template TypeFor<Text>
  FORWARD_UNARY(operator <, SELF)
  FORWARD_UNARY(operator >, SELF)
  FORWARD_UNARY(operator <=, SELF)
  FORWARD_UNARY(operator >=, SELF)
  FORWARD_UNARY(slice, size_t)
#undef SELF

  TextProperty& operator = (ReaderFor<Text> val) { this->set(val); return *this; }
  TextProperty& operator = (Orphan<Text>&& val)  { this->adopt(kj::mv(val)); return *this; }
};

template <typename Impl, uint offset, typename T, typename Default = NoDefault,
          typename Union = NotInUnion>
struct ListProperty: ContainerProperty<Impl, offset, List<T>, Default, Union> {
  typedef typename Impl::template TypeFor<T> Element;

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(kj::ArrayPtr<const ReaderFor<T>> val) {
    auto s = Impl::asStruct(this);
    Union::setDiscriminant(s);
    _::PointerHelpers<List<T>>::set(s.getPointerField(offset), val);
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  typename List<T>::Builder init(size_t n) {
    return ContainerProperty<Impl, offset, List<T>, Default, Union>::init(n);
  }

  template <typename U, typename = kj::EnableIf<!Impl::CONST>>
  void set(uint index, U&& value) { this->get().set(index, kj::fwd<U>(value)); }

  template <typename = kj::EnableIf<!Impl::CONST && kind<T>() == Kind::STRUCT>>
  void adoptWithCaveats(uint i, Orphan<T>&& o) { this->get().adoptWithCaveats(i, kj::mv(o)); }

  template <typename = kj::EnableIf<!Impl::CONST && kind<T>() == Kind::STRUCT>>
  void setWithCaveats(uint i, const ReaderFor<T>& r) { this->get().setWithCaveats(i, r); }

  template <typename = kj::EnableIf<!Impl::CONST>,
            typename = kj::EnableIf<kind<T>() == Kind::LIST || kind<T>() == Kind::BLOB>>
  BuilderFor<T> init(uint i, uint s) { return this->get().init(i, s); }

  template <typename = kj::EnableIf<!Impl::CONST>,
            typename = kj::EnableIf<kind<T>() == Kind::LIST || kind<T>() == Kind::BLOB>>
  void adopt(uint i, Orphan<T>&& o) { this->get().adopt(i, kj::mv(o)); }

  template <typename = kj::EnableIf<!Impl::CONST>,
            typename = kj::EnableIf<kind<T>() == Kind::LIST || kind<T>() == Kind::BLOB>>
  Orphan<T> disown(uint i) { this->get().disown(i); }

  typedef IndexingIterator<typename Impl::template TypeFor<List<T>>,
                           Element, TemporaryPointer, ListProperty> Iterator;

  Iterator begin() { return Iterator(this->get(), 0); }

  Iterator end() {
    auto real = this->get();
    return Iterator(real, real.size());
  }

  Element operator [] (size_t n) { return this->get()[n]; }

  ListProperty& operator = (ReaderFor<List<T>> val) { this->set(val); return *this; }
  ListProperty& operator = (Orphan<List<T>>&& val)  { this->adopt(kj::mv(val)); return *this; }
  ListProperty& operator = (kj::ArrayPtr<const ReaderFor<T>> val) { set(val); return *this; }
};

// ---------------------------------------------------------------------------------------

template <typename Impl, uint offset, typename Union = NotInUnion>
struct AnyPointerProperty {
  KJ_DISALLOW_COPY(AnyPointerProperty);

  typename Impl::template TypeFor<AnyPointer> get() {
    auto s = Impl::asStruct(this);
    Union::check(s);
    return s.getPointerField(offset);
  }

  bool isNull() {
    auto s = Impl::asStruct(this);
    if (!Union::isThis(s)) return true;
    return s.getPointerField(offset).isNull();
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void clear() { get().clear(); }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(AnyPointer::Reader value) { get().set(value); }

  template <typename = kj::EnableIf<!Impl::CONST>>
  Orphan<AnyPointer> disown() { return get().disown(); }

  template <typename = kj::EnableIf<!Impl::CONST>>
  AnyPointer::Reader asReader() { return get().asReader(); }

  template <typename T>
  typename Impl::template TypeFor<T> getAs() { return get().getAs<T>(); }

  template <typename T, typename U>
  typename Impl::template TypeFor<T> getAs(U&& u) { return get().getAs<T>(kj::fwd<U>(u)); }

  template <typename T, typename... Args, typename = kj::EnableIf<!Impl::CONST>>
  BuilderFor<T> initAs(Args&&... args) { return get().initAs<T>(kj::fwd<Args>(args)...); }

  template <typename T, typename = kj::EnableIf<!Impl::CONST>>
  void setAs(ReaderFor<T> value) { get().setAs<T>(value); }

  template <typename T, typename = kj::EnableIf<!Impl::CONST>>
  void setAs(std::initializer_list<ReaderFor<ListElementType<T>>> l) { get().setAs<T>(l); }

  template <typename T, typename = kj::EnableIf<!Impl::CONST>>
  void adopt(Orphan<T>&& orphan) { get().adopt(kj::mv(orphan)); }

  template <typename T, typename... Args, typename = kj::EnableIf<!Impl::CONST>>
  Orphan<T> disownAs(Args&&... args) { return get().disownAs<T>(kj::fwd<Args>(args)...); }

  FORWARD_NULLARY(targetSize)

  bool operator == (decltype(nullptr)) { return isNull(); }
  bool operator != (decltype(nullptr)) { return !isNull(); }

  AnyPointerProperty& operator = (AnyPointer::Reader val) { set(val); return *this; }
  AnyPointerProperty& operator = (Orphan<AnyPointer>&& val)  { adopt(kj::mv(val)); return *this; }

  operator typename Impl::template TypeFor<AnyPointer> () { return get(); }
};

#undef FORWARD_BINARY
#undef FORWARD_UNARY
#undef FORWARD_NULLARY
#undef INFER

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_PROPERTY_H_
