// Copyright (c) 2013, Kenton Varda <temporal@gmail.com>
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

#ifndef CAPNP_ALTCXX_LIST_UTILS_H_
#define CAPNP_ALTCXX_LIST_UTILS_H_

namespace capnp {
namespace altcxx {

template <typename T>
class TemporaryPointer {
public:
  TemporaryPointer(T&& value): value(kj::mv(value)) {}
  TemporaryPointer(const T& value): value(value) {}

  T* operator -> () { return &value; }
  const T* operator -> () const { return &value; }

  T& operator * () { return value; }
  const T& operator * () const { return value; }

private:
  T value;
};

template <typename T>
using NormalPointer = T*;

template <typename Container, typename Element, template <typename> class Pointer = NormalPointer,
          typename Friend = Container>
class IndexingIterator {
public:
  IndexingIterator() = default;

  inline Element operator*() const { return (*container)[index]; }
  inline TemporaryPointer<Element> operator->() const {
    return TemporaryPointer<Element>((*container)[index]);
  }
  inline Element operator[]( int off) const { return (*container)[index]; }
  inline Element operator[](uint off) const { return (*container)[index]; }

  inline IndexingIterator& operator++() { ++index; return *this; }
  inline IndexingIterator operator++(int) { IndexingIterator other = *this; ++index; return other; }
  inline IndexingIterator& operator--() { --index; return *this; }
  inline IndexingIterator operator--(int) { IndexingIterator other = *this; --index; return other; }

  inline IndexingIterator operator+(uint amount) const { return IndexingIterator(container, index + amount); }
  inline IndexingIterator operator-(uint amount) const { return IndexingIterator(container, index - amount); }
  inline IndexingIterator operator+( int amount) const { return IndexingIterator(container, index + amount); }
  inline IndexingIterator operator-( int amount) const { return IndexingIterator(container, index - amount); }

  inline int operator-(const IndexingIterator& other) const { return index - other.index; }

  inline IndexingIterator& operator+=(uint amount) { index += amount; return *this; }
  inline IndexingIterator& operator-=(uint amount) { index -= amount; return *this; }
  inline IndexingIterator& operator+=( int amount) { index += amount; return *this; }
  inline IndexingIterator& operator-=( int amount) { index -= amount; return *this; }

  // STL says comparing iterators of different containers is not allowed, so we only compare
  // indices here.
  inline bool operator==(const IndexingIterator& other) const { return index == other.index; }
  inline bool operator!=(const IndexingIterator& other) const { return index != other.index; }
  inline bool operator<=(const IndexingIterator& other) const { return index <= other.index; }
  inline bool operator>=(const IndexingIterator& other) const { return index >= other.index; }
  inline bool operator< (const IndexingIterator& other) const { return index <  other.index; }
  inline bool operator> (const IndexingIterator& other) const { return index >  other.index; }

private:
  Pointer<Container> container;
  uint index;

  friend Friend;
  inline IndexingIterator(Pointer<Container> container, uint index)
      : container(container), index(index) {}
};

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_LIST_UTILS_H_

