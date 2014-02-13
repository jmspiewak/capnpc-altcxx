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
  template <typename T> static uint16_t which(T) { return -1; }
  template <typename T> static bool isThis(T) { return true; }
  template <typename T> static void check(T) {}
  template <typename T> static void setDiscriminant(T) {}
};

template <uint32_t discriminantOffset, uint16_t discriminantValue>
struct InUnion {
  template <typename T>
  static uint16_t which(T s) {
    return s.template getDataField<uint16_t>(discriminantOffset * ELEMENTS);
  }

  template <typename T>
  static bool isThis(T s) {
    return which(s) == discriminantValue;
  }

  template <typename T>
  static void check(T s) {
    KJ_IREQUIRE(isThis(s), "Must check which() before accessing a union member.");
  }

  static void setDiscriminant(_::StructBuilder s) {
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
  static void init(_::StructBuilder builder) {
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
  static T    get(Struct s) { return s.template getDataField<T>(offset * ELEMENTS, mask); }
  static void set(Struct s, T val) { s.template setDataField<T>(offset * ELEMENTS, val, mask); }
};

template <typename Struct, uint offset, typename T>
struct MaybeMasked<Struct, offset, T, 0> {
  static T    get(Struct s) { return s.template getDataField<T>(offset * ELEMENTS); }
  static void set(Struct s, T val) { s.template setDataField<T>(offset * ELEMENTS, val); }
};

template <typename Struct>
struct MaybeMasked<Struct, 0, Void, 0> {
  static Void get(Struct) { return VOID; }
  static void set(Struct, Void) {}
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
// Impl transformations.

template <typename MaybeInUnion, typename Parent>
struct GroupTransform {
  typedef typename Parent::Helper Helper;
  static constexpr int DEPTH = Parent::DEPTH;

  static typename Helper::Struct transform(void* ptr) {
    typename Helper::Struct s = Parent::transform(ptr);
    MaybeInUnion::check(s);
    return s;
  }
};

template <typename Off, typename T, typename MaybeDefault, typename MaybeInUnion, typename Parent>
struct PointerTransform {
  typedef typename Parent::Helper Helper;
  static constexpr int DEPTH = Parent::DEPTH + 1;

  static typename Helper::Struct transform(void* ptr) {
    typename Helper::Struct s = Parent::transform(ptr);
    MaybeInUnion::check(s);
    return Helper::template getStruct<T>(s.getPointerField(Off::VALUE * POINTERS),
                                         MaybeDefault::ptr());
  }
};

// =======================================================================================
// Property types.

template <typename Impl, uint offset, typename T, typename SafeMask<T>::Type mask = 0,
          typename MaybeInUnion = NotInUnion>
struct PrimitiveProperty {
  KJ_DISALLOW_COPY(PrimitiveProperty);

  T get() {
    MaybeInUnion::check(Impl::asStruct(this));
    return MaybeMasked<typename Impl::Struct, offset, T, mask>::get(Impl::asStruct(this));
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(T val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    MaybeMasked<typename Impl::Struct, offset, T, mask>::set(Impl::asStruct(this), val);
  }

  operator T () { return get(); }
  T operator = (T val) { set(val); return val; }
};

template <typename Impl, typename T, typename Initializer, typename MaybeInUnion = NotInUnion>
struct GroupProperty: public T::template Base<typename Impl::template Push<
                             Apply<GroupTransform, MaybeInUnion>::template Result>> {
  KJ_DISALLOW_COPY(GroupProperty);

  typename Impl::template TypeFor<T> get() {
    MaybeInUnion::check(Impl::asStruct(this));
    return typename Impl::template TypeFor<T>(Impl::asStruct(this));
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  typename Impl::template TypeFor<T> init() {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    Initializer::init(Impl::asStruct(this));
    return typename Impl::template TypeFor<T>(Impl::asStruct(this));
  }

  operator typename Impl::template TypeFor<T> () { return get(); }
};

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct PointerProperty {
  KJ_DISALLOW_COPY(PointerProperty);

  typename Impl::template TypeFor<T> get() {
    MaybeInUnion::check(Impl::asStruct(this));
    return MaybeDefault::template get<T, Impl>(pointer());
  }

  ReaderFor<T> asReader() { return get(); }

  bool isNull() {
    if (!MaybeInUnion::isThis(Impl::asStruct(this))) return true;
    return pointer().isNull();
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(ReaderFor<T> val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::set(pointer(), val);
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void adopt(Orphan<T>&& val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::adopt(pointer(), kj::mv(val));
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  Orphan<T> disown() {
    MaybeInUnion::check(Impl::asStruct(this));
    return _::PointerHelpers<T>::disown(pointer());
  }

  bool operator == (decltype(nullptr)) { return isNull(); }
  bool operator != (decltype(nullptr)) { return !isNull(); }

  void operator = (ReaderFor<T> val) { set(val); }
  void operator = (Orphan<T>&& val)  { adopt(kj::mv(val)); }

  operator typename Impl::template TypeFor<T> () { return this->get(); }

protected:
  typename Impl::Pointer pointer() {
    return Impl::asStruct(this).getPointerField(offset * POINTERS);
  }
};

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct StructProperty: PointerProperty<Impl, offset, T, MaybeDefault, MaybeInUnion>,
    public Conditional<(Impl::DEPTH > 4), Void,
        typename T::template Base<typename Impl::template Push<Apply<PointerTransform,
            Constant<uint, offset>, T, MaybeDefault, MaybeInUnion>::template Result>>> {

  template <typename = kj::EnableIf<!Impl::CONST>>
  BuilderFor<T> init() {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    return _::PointerHelpers<T>::init(this->pointer());
  }

  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
};

#define INFER(expr) decltype(expr) { return expr; }
#define FORWARD_NULLARY(name) auto name() -> INFER(this->get().name())
#define FORWARD_UNARY(name, t0) auto name(t0 a0) -> INFER(this->get().name(a0))
#define FORWARD_BINARY(name, t0, t1) auto name(t0 a0, t1 a1) -> INFER(this->get().name(a0, a1))

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct ContainerProperty: PointerProperty<Impl, offset, T, MaybeDefault, MaybeInUnion> {
  FORWARD_NULLARY(size)

  template <typename = kj::EnableIf<!Impl::CONST>>
  BuilderFor<T> init(size_t n) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    return _::PointerHelpers<T>::init(this->pointer(), n);
  }

  template <typename U>
  bool operator == (U&& val) const { return this->get() == kj::fwd<U>(val); }

  template <typename U>
  bool operator != (U&& val) const { return this->get() == kj::fwd<U>(val); }
};

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct BlobProperty: ContainerProperty<Impl, offset, T, MaybeDefault, MaybeInUnion> {
  FORWARD_BINARY(slice, size_t, size_t)
  FORWARD_NULLARY(begin)
  FORWARD_NULLARY(end)

  auto operator [] (size_t n) -> INFER(this->get()[n])

  template <typename U,
            typename = kj::EnableIf<kj::canConvert<typename Impl::template TypeFor<T>, U>()>>
  operator U () { return this->get(); }

  void operator = (ReaderFor<T> val) { this->set(val); }
  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
};

template <typename Impl, uint offset, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct TextProperty: BlobProperty<Impl, offset, Text, MaybeDefault, MaybeInUnion> {
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

  void operator = (ReaderFor<Text> val) { this->set(val); }
  void operator = (Orphan<Text>&& val)  { this->adopt(kj::mv(val)); }
};

#undef FORWARD_BINARY
#undef FORWARD_UNARY
#undef FORWARD_NULLARY
#undef INFER

template <typename Impl, uint offset, typename T, typename MaybeDefault = NoDefault,
          typename MaybeInUnion = NotInUnion>
struct ListProperty: ContainerProperty<Impl, offset, List<T>, MaybeDefault, MaybeInUnion> {
  typedef typename Impl::template TypeFor<T> Element;

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(kj::ArrayPtr<const ReaderFor<T>> val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<List<T>>::set(this->pointer(), val);
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  typename List<T>::Builder init(size_t n) {
    return ContainerProperty<Impl, offset, List<T>, MaybeDefault, MaybeInUnion>::init(n);
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

  void operator = (ReaderFor<List<T>> val) { this->set(val); }
  void operator = (Orphan<List<T>>&& val)  { this->adopt(kj::mv(val)); }
  void operator = (kj::ArrayPtr<const ReaderFor<T>> val) { set(val); }
};

template <typename Impl, uint offset, typename T, typename MaybeInUnion = NotInUnion>
struct InterfaceProperty: PointerProperty<Impl, offset, T, NoDefault, MaybeInUnion> {
  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(typename T::Client&& val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::set(this->pointer(), kj::mv(val));
  }

  template <typename = kj::EnableIf<!Impl::CONST>>
  void set(typename T::Client& val) {
    MaybeInUnion::setDiscriminant(Impl::asStruct(this));
    _::PointerHelpers<T>::set(this->pointer(), val);
  }

  void operator = (Orphan<T>&& val)  { this->adopt(kj::mv(val)); }
  void operator = (typename T::Client& val) { set(val); }
  void operator = (typename T::Client&& val) { set(kj::mv(val)); }

  typename T::Client operator * () { return this->get(); }
  _::TemporaryPointer<typename T::Client> operator -> () { return this->get(); }
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
