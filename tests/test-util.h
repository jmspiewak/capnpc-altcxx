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

#ifndef CAPNP_TEST_UTIL_H_
#define CAPNP_TEST_UTIL_H_

#include <iostream>
#include <test.capnp.h>
#include <gtest/gtest.h>

#if KJ_NO_EXCEPTIONS
#undef EXPECT_ANY_THROW
#define EXPECT_ANY_THROW(code) EXPECT_DEATH(code, ".")
#endif

#define EXPECT_NONFATAL_FAILURE(code) \
  EXPECT_TRUE(kj::runCatchingExceptions([&]() { code; }) != nullptr);

#ifdef KJ_DEBUG
#define EXPECT_DEBUG_ANY_THROW EXPECT_ANY_THROW
#else
#define EXPECT_DEBUG_ANY_THROW(EXP)
#endif

#define EXPECT_EQ_CAST(X, Y) EXPECT_EQ((X), static_cast<decltype(X)>(Y))

namespace capnp {

inline std::ostream& operator<<(std::ostream& os, const Data::Reader& value) {
  return os.write(reinterpret_cast<const char*>(value.begin()), value.size());
}
inline std::ostream& operator<<(std::ostream& os, const Data::Builder& value) {
  return os.write(reinterpret_cast<const char*>(value.begin()), value.size());
}
inline std::ostream& operator<<(std::ostream& os, const Text::Reader& value) {
  return os.write(value.begin(), value.size());
}
inline std::ostream& operator<<(std::ostream& os, const Text::Builder& value) {
  return os.write(value.begin(), value.size());
}
inline std::ostream& operator<<(std::ostream& os, Void) {
  return os << "void";
}

namespace _ {  // private

inline Data::Reader data(const char* str) {
  return Data::Reader(reinterpret_cast<const byte*>(str), strlen(str));
}

namespace test = capnproto_test::capnp::test;

// We don't use "using namespace" to pull these in because then things would still compile
// correctly if they were generated in the global namespace.
using ::capnproto_test::capnp::test::TestAllTypes;
using ::capnproto_test::capnp::test::TestDefaults;
using ::capnproto_test::capnp::test::TestEnum;
using ::capnproto_test::capnp::test::TestUnion;
using ::capnproto_test::capnp::test::TestUnionDefaults;
using ::capnproto_test::capnp::test::TestNestedTypes;
using ::capnproto_test::capnp::test::TestUsing;
using ::capnproto_test::capnp::test::TestListDefaults;

void initTestMessage(TestAllTypes::Builder builder);
void initTestMessage(TestDefaults::Builder builder);
void initTestMessage(TestListDefaults::Builder builder);

void checkTestMessage(TestAllTypes::Builder builder);
void checkTestMessage(TestDefaults::Builder builder);
void checkTestMessage(TestListDefaults::Builder builder);

void checkTestMessage(TestAllTypes::Reader reader);
void checkTestMessage(TestDefaults::Reader reader);
void checkTestMessage(TestListDefaults::Reader reader);

void checkTestMessageAllZero(TestAllTypes::Builder builder);
void checkTestMessageAllZero(TestAllTypes::Reader reader);

template <typename T, typename U>
void checkList(T reader, std::initializer_list<U> expected) {
  ASSERT_EQ(expected.size(), reader.size());
  for (uint i = 0; i < expected.size(); i++) {
    EXPECT_EQ(expected.begin()[i], reader[i]);
  }
}

template <typename T>
void checkList(T reader, std::initializer_list<float> expected) {
  ASSERT_EQ(expected.size(), reader.size());
  for (uint i = 0; i < expected.size(); i++) {
    EXPECT_FLOAT_EQ(expected.begin()[i], reader[i]);
  }
}

template <typename T>
void checkList(T reader, std::initializer_list<double> expected) {
  ASSERT_EQ(expected.size(), reader.size());
  for (uint i = 0; i < expected.size(); i++) {
    EXPECT_DOUBLE_EQ(expected.begin()[i], reader[i]);
  }
}

// =======================================================================================
// Interface implementations.

class TestInterfaceImpl final: public test::TestInterface::Server {
public:
  TestInterfaceImpl(int& callCount);

  kj::Promise<void> foo(FooContext context) override;

  kj::Promise<void> baz(BazContext context) override;

private:
  int& callCount;
};

class TestExtendsImpl final: public test::TestExtends::Server {
public:
  TestExtendsImpl(int& callCount);

  kj::Promise<void> foo(FooContext context) override;

  kj::Promise<void> grault(GraultContext context) override;

private:
  int& callCount;
};

class TestPipelineImpl final: public test::TestPipeline::Server {
public:
  TestPipelineImpl(int& callCount);

  kj::Promise<void> getCap(GetCapContext context) override;

private:
  int& callCount;
};

}  // namespace _ (private)
}  // namespace capnp

#endif  // TEST_UTIL_H_
