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

#ifndef CAPNP_ALTCXX_IMPL_PIPELINE_H_
#define CAPNP_ALTCXX_IMPL_PIPELINE_H_

#include <capnp/any.h>

namespace capnp {
namespace altcxx {
namespace {

struct RootOp {
  static constexpr int DEPTH = 0;

  template <typename T>
  static AnyPointer::Pipeline& get(T* ptr) {
    return *reinterpret_cast<AnyPointer::Pipeline*>(ptr);
  }
};

} // namespace

template <typename T>
struct DummyPipelineBase {
  typedef T Pipelines;

  DummyPipelineBase(decltype(nullptr)) {}
  explicit DummyPipelineBase(AnyPointer::Pipeline&&) {}
};

template <typename T>
using Pipeline = typename T::template PipelineBase<RootOp, AnyPointer::Pipeline>;

} // namespace altcxx
} // namespace capnp

#endif // CAPNP_ALTCXX_IMPL_PIPELINE_H_
