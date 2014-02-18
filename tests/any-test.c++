// Copyright (c) 2013, Kenton Varda <temporal@gmail.com>
// Copyright (c) 2014, Jakub Spiewak <j.m.spiewak@gmil.com>
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

#include <capnp/any.h>
#include <capnp/message.h>
#include <gtest/gtest.h>
#include "test-util.h"

namespace capnp {
namespace _ {  // private
namespace {

TEST(Any, AnyPointer) {
  MallocMessageBuilder builder;
  auto root = builder.getRoot<test::TestAnyPointer>();

  initTestMessage(root.anyPointerField.initAs<TestAllTypes>());
  checkTestMessage(root.anyPointerField.getAs<TestAllTypes>());
  checkTestMessage(root.asReader().anyPointerField.getAs<TestAllTypes>());

  root.anyPointerField.setAs<Text>("foo");
  EXPECT_EQ("foo", root.anyPointerField.getAs<Text>());
  EXPECT_EQ("foo", root.asReader().anyPointerField.getAs<Text>());

  root.anyPointerField.setAs<Data>(data("foo"));
  EXPECT_EQ(data("foo"), root.anyPointerField.getAs<Data>());
  EXPECT_EQ(data("foo"), root.asReader().anyPointerField.getAs<Data>());

  {
    root.anyPointerField.setAs<List<uint32_t>>({123, 456, 789});

    {
      List<uint32_t>::Builder list = root.anyPointerField.getAs<List<uint32_t>>();
      ASSERT_EQ(3u, list.size());
      EXPECT_EQ(123u, list[0]);
      EXPECT_EQ(456u, list[1]);
      EXPECT_EQ(789u, list[2]);
    }

    {
      List<uint32_t>::Reader list = root.asReader().anyPointerField.getAs<List<uint32_t>>();
      ASSERT_EQ(3u, list.size());
      EXPECT_EQ(123u, list[0]);
      EXPECT_EQ(456u, list[1]);
      EXPECT_EQ(789u, list[2]);
    }
  }

  {
    root.anyPointerField.setAs<List<Text>>({"foo", "bar"});

    {
      List<Text>::Builder list = root.anyPointerField.getAs<List<Text>>();
      ASSERT_EQ(2u, list.size());
      EXPECT_EQ("foo", list[0]);
      EXPECT_EQ("bar", list[1]);
    }

    {
      List<Text>::Reader list = root.asReader().anyPointerField.getAs<List<Text>>();
      ASSERT_EQ(2u, list.size());
      EXPECT_EQ("foo", list[0]);
      EXPECT_EQ("bar", list[1]);
    }
  }

  {
    {
      List<TestAllTypes>::Builder list = root.anyPointerField.initAs<List<TestAllTypes>>(2);
      ASSERT_EQ(2u, list.size());
      initTestMessage(list[0]);
    }

    {
      List<TestAllTypes>::Builder list = root.anyPointerField.getAs<List<TestAllTypes>>();
      ASSERT_EQ(2u, list.size());
      checkTestMessage(list[0]);
      checkTestMessageAllZero(list[1]);
    }

    {
      List<TestAllTypes>::Reader list =
          root.asReader().anyPointerField.getAs<List<TestAllTypes>>();
      ASSERT_EQ(2u, list.size());
      checkTestMessage(list[0]);
      checkTestMessageAllZero(list[1]);
    }
  }
}

}  // namespace
}  // namespace _ (private)
}  // namespace capnp
