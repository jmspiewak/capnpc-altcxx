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

#include "test-util.h"
#include <kj/debug.h>
#include <gtest/gtest.h>

namespace capnp {
namespace _ {  // private
namespace {

template <typename Builder>
void genericInitTestMessage(Builder builder) {
  builder.voidField = VOID;
  builder.boolField = true;
  builder.int8Field = -123;
  builder.int16Field = -12345;
  builder.int32Field = -12345678;
  builder.int64Field = -123456789012345ll;
  builder.uInt8Field = 234u;
  builder.uInt16Field = 45678u;
  builder.uInt32Field = 3456789012u;
  builder.uInt64Field = 12345678901234567890ull;
  builder.float32Field = 1234.5;
  builder.float64Field = -123e45;
  builder.textField = "foo";
  builder.dataField = data("bar");
  {
    auto subBuilder = builder.structField.init();
    subBuilder.voidField = VOID;
    subBuilder.boolField = true;
    subBuilder.int8Field = -12;
    subBuilder.int16Field = 3456;
    subBuilder.int32Field = -78901234;
    subBuilder.int64Field = 56789012345678ll;
    subBuilder.uInt8Field = 90u;
    subBuilder.uInt16Field = 1234u;
    subBuilder.uInt32Field = 56789012u;
    subBuilder.uInt64Field = 345678901234567890ull;
    subBuilder.float32Field = -1.25e-10;
    subBuilder.float64Field = 345;
    subBuilder.textField = "baz";
    subBuilder.dataField = data("qux");
    {
      auto subSubBuilder = subBuilder.structField.init();
      subSubBuilder.textField = "nested";
      subSubBuilder.structField.init().textField = "really nested";
    }
    subBuilder.enumField = TestEnum::BAZ;

    subBuilder.voidList = {VOID, VOID, VOID};
    subBuilder.boolList = {false, true, false, true, true};
    subBuilder.int8List = {12, -34, -0x80, 0x7f};
    subBuilder.int16List = {1234, -5678, -0x8000, 0x7fff};
    // gcc warns on -0x800... and the only work-around I could find was to do -0x7ff...-1.
    subBuilder.int32List = {12345678, -90123456, -0x7fffffff - 1, 0x7fffffff};
    subBuilder.int64List = {123456789012345ll, -678901234567890ll, -0x7fffffffffffffffll-1, 0x7fffffffffffffffll};
    subBuilder.uInt8List = {12u, 34u, 0u, 0xffu};
    subBuilder.uInt16List = {1234u, 5678u, 0u, 0xffffu};
    subBuilder.uInt32List = {12345678u, 90123456u, 0u, 0xffffffffu};
    subBuilder.uInt64List = {123456789012345ull, 678901234567890ull, 0ull, 0xffffffffffffffffull};
    subBuilder.float32List = {0, 1234567, 1e37, -1e37, 1e-37, -1e-37};
    subBuilder.float64List = {0, 123456789012345, 1e306, -1e306, 1e-306, -1e-306};
    subBuilder.textList = {"quux", "corge", "grault"};
    subBuilder.dataList = {data("garply"), data("waldo"), data("fred")};
    {
      auto listBuilder = subBuilder.structList.init(3);
      listBuilder[0].textField = "x structlist 1";
      listBuilder[1].textField = "x structlist 2";
      listBuilder[2].textField = "x structlist 3";
    }
    subBuilder.enumList = {TestEnum::QUX, TestEnum::BAR, TestEnum::GRAULT};
  }
  builder.enumField = TestEnum::CORGE;

  builder.voidList.init(6);
  builder.boolList = {true, false, false, true};
  builder.int8List = {111, -111};
  builder.int16List = {11111, -11111};
  builder.int32List = {111111111, -111111111};
  builder.int64List = {1111111111111111111ll, -1111111111111111111ll};
  builder.uInt8List = {111u, 222u};
  builder.uInt16List = {33333u, 44444u};
  builder.uInt32List = {3333333333u};
  builder.uInt64List = {11111111111111111111ull};
  builder.float32List = {5555.5, kj::inf(), -kj::inf(), kj::nan()};
  builder.float64List = {7777.75, kj::inf(), -kj::inf(), kj::nan()};
  builder.textList = {"plugh", "xyzzy", "thud"};
  builder.dataList = {data("oops"), data("exhausted"), data("rfc3092")};
  {
    auto listBuilder = builder.structList.init(3);
    listBuilder[0].textField = "structlist 1";
    listBuilder[1].textField = "structlist 2";
    listBuilder[2].textField = "structlist 3";
  }
  builder.enumList = {TestEnum::FOO, TestEnum::GARPLY};
}

inline bool isNaN(float f) { return f != f; }
inline bool isNaN(double f) { return f != f; }

template <typename Reader>
void genericCheckTestMessage(Reader reader) {
  EXPECT_EQ_CAST(VOID, reader.voidField);
  EXPECT_EQ_CAST(true, reader.boolField);
  EXPECT_EQ_CAST(-123, reader.int8Field);
  EXPECT_EQ_CAST(-12345, reader.int16Field);
  EXPECT_EQ_CAST(-12345678, reader.int32Field);
  EXPECT_EQ_CAST(-123456789012345ll, reader.int64Field);
  EXPECT_EQ_CAST(234u, reader.uInt8Field);
  EXPECT_EQ_CAST(45678u, reader.uInt16Field);
  EXPECT_EQ_CAST(3456789012u, reader.uInt32Field);
  EXPECT_EQ_CAST(12345678901234567890ull, reader.uInt64Field);
  EXPECT_FLOAT_EQ(1234.5f, reader.float32Field);
  EXPECT_DOUBLE_EQ(-123e45, reader.float64Field);
  EXPECT_EQ("foo", *reader.textField);
  EXPECT_EQ(data("bar"), *reader.dataField);
  {
    auto subReader = *reader.structField;
    EXPECT_EQ_CAST(VOID, subReader.voidField);
    EXPECT_EQ_CAST(true, subReader.boolField);
    EXPECT_EQ_CAST(-12, subReader.int8Field);
    EXPECT_EQ_CAST(3456, subReader.int16Field);
    EXPECT_EQ_CAST(-78901234, subReader.int32Field);
    EXPECT_EQ_CAST(56789012345678ll, subReader.int64Field);
    EXPECT_EQ_CAST(90u, subReader.uInt8Field);
    EXPECT_EQ_CAST(1234u, subReader.uInt16Field);
    EXPECT_EQ_CAST(56789012u, subReader.uInt32Field);
    EXPECT_EQ_CAST(345678901234567890ull, subReader.uInt64Field);
    EXPECT_FLOAT_EQ(-1.25e-10f, subReader.float32Field);
    EXPECT_DOUBLE_EQ(345, subReader.float64Field);
    EXPECT_EQ("baz", *subReader.textField);
    EXPECT_EQ(data("qux"), *subReader.dataField);
    {
      auto subSubReader = *subReader.structField;
      EXPECT_EQ("nested", *subSubReader.textField);
      EXPECT_EQ("really nested", *subSubReader.structField->textField);
    }
    EXPECT_EQ_CAST(TestEnum::BAZ, subReader.enumField);

    checkList(*subReader.voidList, {VOID, VOID, VOID});
    checkList(*subReader.boolList, {false, true, false, true, true});
    checkList(*subReader.int8List, {12, -34, -0x80, 0x7f});
    checkList(*subReader.int16List, {1234, -5678, -0x8000, 0x7fff});
    // gcc warns on -0x800... and the only work-around I could find was to do -0x7ff...-1.
    checkList(*subReader.int32List, {12345678, -90123456, -0x7fffffff - 1, 0x7fffffff});
    checkList(*subReader.int64List, {123456789012345ll, -678901234567890ll, -0x7fffffffffffffffll-1, 0x7fffffffffffffffll});
    checkList(*subReader.uInt8List, {12u, 34u, 0u, 0xffu});
    checkList(*subReader.uInt16List, {1234u, 5678u, 0u, 0xffffu});
    checkList(*subReader.uInt32List, {12345678u, 90123456u, 0u, 0xffffffffu});
    checkList(*subReader.uInt64List, {123456789012345ull, 678901234567890ull, 0ull, 0xffffffffffffffffull});
    checkList(*subReader.float32List, {0.0f, 1234567.0f, 1e37f, -1e37f, 1e-37f, -1e-37f});
    checkList(*subReader.float64List, {0.0, 123456789012345.0, 1e306, -1e306, 1e-306, -1e-306});
    checkList(*subReader.textList, {"quux", "corge", "grault"});
    checkList(*subReader.dataList, {data("garply"), data("waldo"), data("fred")});
    {
      auto listReader = *subReader.structList;
      ASSERT_EQ(3u, listReader.size());
      EXPECT_EQ("x structlist 1", *listReader[0].textField);
      EXPECT_EQ("x structlist 2", *listReader[1].textField);
      EXPECT_EQ("x structlist 3", *listReader[2].textField);
    }
    checkList(*subReader.enumList, {TestEnum::QUX, TestEnum::BAR, TestEnum::GRAULT});
  }
  EXPECT_EQ_CAST(TestEnum::CORGE, reader.enumField);

  EXPECT_EQ_CAST(6u, reader.voidList->size());
  checkList(*reader.boolList, {true, false, false, true});
  checkList(*reader.int8List, {111, -111});
  checkList(*reader.int16List, {11111, -11111});
  checkList(*reader.int32List, {111111111, -111111111});
  checkList(*reader.int64List, {1111111111111111111ll, -1111111111111111111ll});
  checkList(*reader.uInt8List, {111u, 222u});
  checkList(*reader.uInt16List, {33333u, 44444u});
  checkList(*reader.uInt32List, {3333333333u});
  checkList(*reader.uInt64List, {11111111111111111111ull});
  {
    auto listReader = *reader.float32List;
    ASSERT_EQ(4u, listReader.size());
    EXPECT_EQ_CAST(5555.5f, listReader[0]);
    EXPECT_EQ_CAST(kj::inf(), listReader[1]);
    EXPECT_EQ_CAST(-kj::inf(), listReader[2]);
    EXPECT_TRUE(isNaN(listReader[3]));
  }
  {
    auto listReader = *reader.float64List;
    ASSERT_EQ(4u, listReader.size());
    EXPECT_EQ_CAST(7777.75, listReader[0]);
    EXPECT_EQ_CAST(kj::inf(), listReader[1]);
    EXPECT_EQ_CAST(-kj::inf(), listReader[2]);
    EXPECT_TRUE(isNaN(listReader[3]));
  }
  checkList(*reader.textList, {"plugh", "xyzzy", "thud"});
  checkList(*reader.dataList, {data("oops"), data("exhausted"), data("rfc3092")});
  {
    auto listReader = *reader.structList;
    ASSERT_EQ(3u, listReader.size());
    EXPECT_EQ("structlist 1", *listReader[0].textField);
    EXPECT_EQ("structlist 2", *listReader[1].textField);
    EXPECT_EQ("structlist 3", *listReader[2].textField);
  }
  checkList(*reader.enumList, {TestEnum::FOO, TestEnum::GARPLY});
}

template <typename Reader>
void genericCheckTestMessageAllZero(Reader reader) {
  EXPECT_EQ_CAST(VOID, reader.voidField);
  EXPECT_EQ_CAST(false, reader.boolField);
  EXPECT_EQ_CAST(0, reader.int8Field);
  EXPECT_EQ_CAST(0, reader.int16Field);
  EXPECT_EQ_CAST(0, reader.int32Field);
  EXPECT_EQ_CAST(0, reader.int64Field);
  EXPECT_EQ_CAST(0u, reader.uInt8Field);
  EXPECT_EQ_CAST(0u, reader.uInt16Field);
  EXPECT_EQ_CAST(0u, reader.uInt32Field);
  EXPECT_EQ_CAST(0u, reader.uInt64Field);
  EXPECT_FLOAT_EQ(0, reader.float32Field);
  EXPECT_DOUBLE_EQ(0, reader.float64Field);
  EXPECT_EQ("", *reader.textField);
  EXPECT_EQ(data(""), *reader.dataField);
  {
    auto subReader = *reader.structField;
    EXPECT_EQ_CAST(VOID, subReader.voidField);
    EXPECT_EQ_CAST(false, subReader.boolField);
    EXPECT_EQ_CAST(0, subReader.int8Field);
    EXPECT_EQ_CAST(0, subReader.int16Field);
    EXPECT_EQ_CAST(0, subReader.int32Field);
    EXPECT_EQ_CAST(0, subReader.int64Field);
    EXPECT_EQ_CAST(0u, subReader.uInt8Field);
    EXPECT_EQ_CAST(0u, subReader.uInt16Field);
    EXPECT_EQ_CAST(0u, subReader.uInt32Field);
    EXPECT_EQ_CAST(0u, subReader.uInt64Field);
    EXPECT_FLOAT_EQ(0, subReader.float32Field);
    EXPECT_DOUBLE_EQ(0, subReader.float64Field);
    EXPECT_EQ("", *subReader.textField);
    EXPECT_EQ(data(""), *subReader.dataField);
    {
      auto subSubReader = *subReader.structField;
      EXPECT_EQ("", *subSubReader.textField);
      EXPECT_EQ("", *subSubReader.structField->textField);
    }

    EXPECT_EQ_CAST(0u, subReader.voidList->size());
    EXPECT_EQ_CAST(0u, subReader.boolList->size());
    EXPECT_EQ_CAST(0u, subReader.int8List->size());
    EXPECT_EQ_CAST(0u, subReader.int16List->size());
    EXPECT_EQ_CAST(0u, subReader.int32List->size());
    EXPECT_EQ_CAST(0u, subReader.int64List->size());
    EXPECT_EQ_CAST(0u, subReader.uInt8List->size());
    EXPECT_EQ_CAST(0u, subReader.uInt16List->size());
    EXPECT_EQ_CAST(0u, subReader.uInt32List->size());
    EXPECT_EQ_CAST(0u, subReader.uInt64List->size());
    EXPECT_EQ_CAST(0u, subReader.float32List->size());
    EXPECT_EQ_CAST(0u, subReader.float64List->size());
    EXPECT_EQ_CAST(0u, subReader.textList->size());
    EXPECT_EQ_CAST(0u, subReader.dataList->size());
    EXPECT_EQ_CAST(0u, subReader.structList->size());
  }

  EXPECT_EQ_CAST(0u, reader.voidList->size());
  EXPECT_EQ_CAST(0u, reader.boolList->size());
  EXPECT_EQ_CAST(0u, reader.int8List->size());
  EXPECT_EQ_CAST(0u, reader.int16List->size());
  EXPECT_EQ_CAST(0u, reader.int32List->size());
  EXPECT_EQ_CAST(0u, reader.int64List->size());
  EXPECT_EQ_CAST(0u, reader.uInt8List->size());
  EXPECT_EQ_CAST(0u, reader.uInt16List->size());
  EXPECT_EQ_CAST(0u, reader.uInt32List->size());
  EXPECT_EQ_CAST(0u, reader.uInt64List->size());
  EXPECT_EQ_CAST(0u, reader.float32List->size());
  EXPECT_EQ_CAST(0u, reader.float64List->size());
  EXPECT_EQ_CAST(0u, reader.textList->size());
  EXPECT_EQ_CAST(0u, reader.dataList->size());
  EXPECT_EQ_CAST(0u, reader.structList->size());
}

template <typename Builder>
void genericInitListDefaults(Builder builder) {
  auto lists = builder.lists.init();

  lists.list0.init(2);
  lists.list1.init(4);
  lists.list8.init(2);
  lists.list16.init(2);
  lists.list32.init(2);
  lists.list64.init(2);
  lists.listP.init(2);

  lists.list0[0].f = VOID;
  lists.list0[1].f = VOID;
  lists.list1[0].f = true;
  lists.list1[1].f = false;
  lists.list1[2].f = true;
  lists.list1[3].f = true;
  lists.list8[0].f = 123u;
  lists.list8[1].f = 45u;
  lists.list16[0].f = 12345u;
  lists.list16[1].f = 6789u;
  lists.list32[0].f = 123456789u;
  lists.list32[1].f = 234567890u;
  lists.list64[0].f = 1234567890123456u;
  lists.list64[1].f = 2345678901234567u;
  lists.listP[0].f = "foo";
  lists.listP[1].f = "bar";

  {
    auto l = lists.int32ListList.init(3);
    l.set(0, {1, 2, 3});
    l.set(1, {4, 5});
    l.set(2, {12341234});
  }

  {
    auto l = lists.textListList.init(3);
    l.set(0, {"foo", "bar"});
    l.set(1, {"baz"});
    l.set(2, {"qux", "corge"});
  }

  {
    auto l = lists.structListList.init(2);
    auto e = l.init(0, 2);
    e[0].int32Field = 123;
    e[1].int32Field = 456;
    e = l.init(1, 1);
    e[0].int32Field = 789;
  }
}

template <typename Reader>
void genericCheckListDefaults(Reader reader) {
  auto lists = *reader.lists;

  ASSERT_EQ(2u, lists.list0->size());
  ASSERT_EQ(4u, lists.list1->size());
  ASSERT_EQ(2u, lists.list8->size());
  ASSERT_EQ(2u, lists.list16->size());
  ASSERT_EQ(2u, lists.list32->size());
  ASSERT_EQ(2u, lists.list64->size());
  ASSERT_EQ(2u, lists.listP->size());

  EXPECT_EQ_CAST(VOID, lists.list0[0].f);
  EXPECT_EQ_CAST(VOID, lists.list0[1].f);
  EXPECT_TRUE(lists.list1[0].f);
  EXPECT_FALSE(lists.list1[1].f);
  EXPECT_TRUE(lists.list1[2].f);
  EXPECT_TRUE(lists.list1[3].f);
  EXPECT_EQ_CAST(123u, lists.list8[0].f);
  EXPECT_EQ_CAST(45u, lists.list8[1].f);
  EXPECT_EQ_CAST(12345u, lists.list16[0].f);
  EXPECT_EQ_CAST(6789u, lists.list16[1].f);
  EXPECT_EQ_CAST(123456789u, lists.list32[0].f);
  EXPECT_EQ_CAST(234567890u, lists.list32[1].f);
  EXPECT_EQ_CAST(1234567890123456u, lists.list64[0].f);
  EXPECT_EQ_CAST(2345678901234567u, lists.list64[1].f);
  EXPECT_EQ("foo", *lists.listP[0].f);
  EXPECT_EQ("bar", *lists.listP[1].f);

  {
    auto l = *lists.int32ListList;
    ASSERT_EQ(3u, l.size());
    checkList(l[0], {1, 2, 3});
    checkList(l[1], {4, 5});
    checkList(l[2], {12341234});
  }

  {
    auto l = *lists.textListList;
    ASSERT_EQ(3u, l.size());
    checkList(l[0], {"foo", "bar"});
    checkList(l[1], {"baz"});
    checkList(l[2], {"qux", "corge"});
  }

  {
    auto l = *lists.structListList;
    ASSERT_EQ(2u, l.size());
    auto e = l[0];
    ASSERT_EQ(2u, e.size());
    EXPECT_EQ_CAST(123, e[0].int32Field);
    EXPECT_EQ_CAST(456, e[1].int32Field);
    e = l[1];
    ASSERT_EQ(1u, e.size());
    EXPECT_EQ_CAST(789, e[0].int32Field);
  }
}

}  // namespace

void initTestMessage(TestAllTypes::Builder builder) { genericInitTestMessage(builder); }
void initTestMessage(TestDefaults::Builder builder) { genericInitTestMessage(builder); }
void initTestMessage(TestListDefaults::Builder builder) { genericInitListDefaults(builder); }

void checkTestMessage(TestAllTypes::Builder builder) { genericCheckTestMessage(builder); }
void checkTestMessage(TestDefaults::Builder builder) { genericCheckTestMessage(builder); }
void checkTestMessage(TestListDefaults::Builder builder) { genericCheckListDefaults(builder); }

void checkTestMessage(TestAllTypes::Reader reader) { genericCheckTestMessage(reader); }
void checkTestMessage(TestDefaults::Reader reader) { genericCheckTestMessage(reader); }
void checkTestMessage(TestListDefaults::Reader reader) { genericCheckListDefaults(reader); }

void checkTestMessageAllZero(TestAllTypes::Builder builder) {
  genericCheckTestMessageAllZero(builder);
}
void checkTestMessageAllZero(TestAllTypes::Reader reader) {
  genericCheckTestMessageAllZero(reader);
}

// =======================================================================================
// Interface implementations.

TestInterfaceImpl::TestInterfaceImpl(int& callCount): callCount(callCount) {}

kj::Promise<void> TestInterfaceImpl::foo(FooContext context) {
  ++callCount;
  auto params = context.getParams();
  auto result = context.getResults();
  EXPECT_EQ_CAST(123, params.i);
  EXPECT_TRUE(params.j);
  result.x = "foo";
  return kj::READY_NOW;
}

kj::Promise<void> TestInterfaceImpl::baz(BazContext context) {
  ++callCount;
  auto params = context.getParams();
  checkTestMessage(*params.s);
  context.releaseParams();
  EXPECT_ANY_THROW(context.getParams());

  return kj::READY_NOW;
}

TestExtendsImpl::TestExtendsImpl(int& callCount): callCount(callCount) {}

kj::Promise<void> TestExtendsImpl::foo(FooContext context) {
  ++callCount;
  auto params = context.getParams();
  auto result = context.getResults();
  EXPECT_EQ_CAST(321, params.i);
  EXPECT_FALSE(params.j);
  result.x = "bar";
  return kj::READY_NOW;
}

kj::Promise<void> TestExtendsImpl::grault(GraultContext context) {
  ++callCount;
  context.releaseParams();

  initTestMessage(context.getResults());

  return kj::READY_NOW;
}

TestPipelineImpl::TestPipelineImpl(int& callCount): callCount(callCount) {}

kj::Promise<void> TestPipelineImpl::getCap(GetCapContext context) {
  ++callCount;

  auto params = context.getParams();
  EXPECT_EQ_CAST(234, params.n);

  auto cap = *params.inCap;
  context.releaseParams();

  auto request = cap.fooRequest();
  request.i = 123;
  request.j = true;

  return request.send().then(
      [this,context](Response<test::TestInterface::FooResults>&& response) mutable {
        EXPECT_EQ("foo", *response.x);

        auto result = context.getResults();
        result.s = "bar";
        result.outBox.init().cap = kj::heap<TestExtendsImpl>(callCount);
      });
}

}  // namespace _ (private)
}  // namespace capnp
