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

#include <capnp/message.h>
#include <gtest/gtest.h>
#include "test-util.h"

namespace capnp {
namespace _ {  // private
namespace {

TEST(Basic, AllTypes) {
  MallocMessageBuilder builder;

  initTestMessage(builder.initRoot<TestAllTypes>());
  checkTestMessage(builder.getRoot<TestAllTypes>());
  checkTestMessage(builder.getRoot<TestAllTypes>().asReader());

  SegmentArrayMessageReader reader(builder.getSegmentsForOutput());

  checkTestMessage(reader.getRoot<TestAllTypes>());

  ASSERT_EQ(1u, builder.getSegmentsForOutput().size());

  checkTestMessage(readMessageUnchecked<TestAllTypes>(builder.getSegmentsForOutput()[0].begin()));

  EXPECT_EQ(builder.getSegmentsForOutput()[0].size() - 1,  // -1 for root pointer
            reader.getRoot<TestAllTypes>().totalSize().wordCount);
}

TEST(Basic, Defaults) {
  AlignedData<1> nullRoot = {{0, 0, 0, 0, 0, 0, 0, 0}};
  kj::ArrayPtr<const word> segments[1] = {kj::arrayPtr(nullRoot.words, 1)};
  SegmentArrayMessageReader reader(kj::arrayPtr(segments, 1));

  checkTestMessage(reader.getRoot<TestDefaults>());
  checkTestMessage(readMessageUnchecked<TestDefaults>(nullRoot.words));

  checkTestMessage(TestDefaults::Reader());
}

TEST(Basic, DefaultInitialization) {
  MallocMessageBuilder builder;

  checkTestMessage(builder.getRoot<TestDefaults>());  // first pass initializes to defaults
  checkTestMessage(builder.getRoot<TestDefaults>().asReader());

  checkTestMessage(builder.getRoot<TestDefaults>());  // second pass just reads the initialized structure
  checkTestMessage(builder.getRoot<TestDefaults>().asReader());

  SegmentArrayMessageReader reader(builder.getSegmentsForOutput());

  checkTestMessage(reader.getRoot<TestDefaults>());
}

TEST(Basic, ListDefaults) {
  MallocMessageBuilder builder;
  TestListDefaults::Builder root = builder.getRoot<TestListDefaults>();

  checkTestMessage(root.asReader());
  checkTestMessage(root);
  checkTestMessage(root.asReader());
}

TEST(Basic, BuildListDefaults) {
  MallocMessageBuilder builder;
  TestListDefaults::Builder root = builder.getRoot<TestListDefaults>();

  initTestMessage(root);
  checkTestMessage(root.asReader());
  checkTestMessage(root);
  checkTestMessage(root.asReader());
}

TEST(Basic, Unions) {
  MallocMessageBuilder builder;
  TestUnion::Builder root = builder.getRoot<TestUnion>();

  EXPECT_EQ(TestUnion::Union0::U0F0S0, root.union0.which());
  EXPECT_EQ_CAST(VOID, root.union0.u0f0s0);
  EXPECT_DEBUG_ANY_THROW((bool)root.union0.u0f0s1);

  root.union0.u0f0s1 = true;
  EXPECT_EQ(TestUnion::Union0::U0F0S1, root.union0.which());
  EXPECT_TRUE(root.union0.u0f0s1);
  EXPECT_DEBUG_ANY_THROW((Void)root.union0.u0f0s0);

  root.union0.u0f0s8 = 123;
  EXPECT_EQ(TestUnion::Union0::U0F0S8, root.union0.which());
  EXPECT_EQ_CAST(123, root.union0.u0f0s8);
  EXPECT_DEBUG_ANY_THROW((bool)root.union0.u0f0s1);
}

TEST(Basic, UnnamedUnion) {
  MallocMessageBuilder builder;
  auto root = builder.initRoot<test::TestUnnamedUnion>();
  EXPECT_EQ(test::TestUnnamedUnion::FOO, root.which());

  root.bar = 321;
  EXPECT_EQ(test::TestUnnamedUnion::BAR, root.which());
  EXPECT_EQ(test::TestUnnamedUnion::BAR, root.asReader().which());
  EXPECT_EQ_CAST(321u, root.bar);
  EXPECT_EQ_CAST(321u, root.asReader().bar);
  EXPECT_DEBUG_ANY_THROW((int)root.foo);
  EXPECT_DEBUG_ANY_THROW((int)root.asReader().foo);

  root.foo = 123;
  EXPECT_EQ(test::TestUnnamedUnion::FOO, root.which());
  EXPECT_EQ(test::TestUnnamedUnion::FOO, root.asReader().which());
  EXPECT_EQ_CAST(123u, root.foo);
  EXPECT_EQ_CAST(123u, root.asReader().foo);
  EXPECT_DEBUG_ANY_THROW((int)root.bar);
  EXPECT_DEBUG_ANY_THROW((int)root.asReader().bar);
}

TEST(Basic, Groups) {
  MallocMessageBuilder builder;
  auto root = builder.initRoot<test::TestGroups>();

  {
    auto foo = root.groups.foo.init();
    foo.corge = 12345678;
    foo.grault = 123456789012345ll;
    foo.garply = "foobar";

    EXPECT_EQ_CAST(12345678, foo.corge);
    EXPECT_EQ_CAST(123456789012345ll, foo.grault);
    EXPECT_EQ("foobar", foo.garply.get());
  }

  {
    auto bar = root.groups.bar.init();
    bar.corge = 23456789;
    bar.grault = "barbaz";
    bar.garply = 234567890123456ll;

    EXPECT_EQ_CAST(23456789, bar.corge);
    EXPECT_EQ("barbaz", bar.grault.get());
    EXPECT_EQ_CAST(234567890123456ll, bar.garply);
  }

  {
    auto baz = root.groups.baz.init();
    baz.corge = 34567890;
    baz.grault = "bazqux";
    baz.garply = "quxquux";

    EXPECT_EQ_CAST(34567890, baz.corge);
    EXPECT_EQ("bazqux", baz.grault.get());
    EXPECT_EQ("quxquux", baz.garply.get());
  }
}

TEST(Basic, Orphan) {
  MallocMessageBuilder builder;
  auto root = builder.initRoot<TestAllTypes>();

  initTestMessage(root.structField.init());
  EXPECT_TRUE(root.structField != nullptr);

  Orphan<TestAllTypes> orphan = root.structField.disown();
  EXPECT_FALSE(orphan == nullptr);

  checkTestMessage(orphan.getReader());
  checkTestMessage(orphan.get());
  EXPECT_TRUE(root.structField == nullptr);

  root.structField = kj::mv(orphan);
  EXPECT_TRUE(orphan == nullptr);
  EXPECT_TRUE(root.structField != nullptr);
  checkTestMessage(root.asReader().structField);
}

}  // namespace
}  // namespace _ (private)
}  // namespace capnp
