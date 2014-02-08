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

#include <kj/debug.h>
#include <kj/async-io.h>
#include <capnp/rpc-twoparty.h>
#include <capnp/message.h>
#include <gtest/gtest.h>
#include "test-util.h"

namespace capnp {
namespace _ {  // private
namespace {

TEST(Test, AllTypes) {
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

TEST(Test, Defaults) {
  AlignedData<1> nullRoot = {{0, 0, 0, 0, 0, 0, 0, 0}};
  kj::ArrayPtr<const word> segments[1] = {kj::arrayPtr(nullRoot.words, 1)};
  SegmentArrayMessageReader reader(kj::arrayPtr(segments, 1));

  checkTestMessage(reader.getRoot<TestDefaults>());
  checkTestMessage(readMessageUnchecked<TestDefaults>(nullRoot.words));

  checkTestMessage(TestDefaults::Reader());
}

TEST(Test, DefaultInitialization) {
  MallocMessageBuilder builder;

  checkTestMessage(builder.getRoot<TestDefaults>());  // first pass initializes to defaults
  checkTestMessage(builder.getRoot<TestDefaults>().asReader());

  checkTestMessage(builder.getRoot<TestDefaults>());  // second pass just reads the initialized structure
  checkTestMessage(builder.getRoot<TestDefaults>().asReader());

  SegmentArrayMessageReader reader(builder.getSegmentsForOutput());

  checkTestMessage(reader.getRoot<TestDefaults>());
}

TEST(Test, ListDefaults) {
  MallocMessageBuilder builder;
  TestListDefaults::Builder root = builder.getRoot<TestListDefaults>();

  checkTestMessage(root.asReader());
  checkTestMessage(root);
  checkTestMessage(root.asReader());
}

TEST(Test, BuildListDefaults) {
  MallocMessageBuilder builder;
  TestListDefaults::Builder root = builder.getRoot<TestListDefaults>();

  initTestMessage(root);
  checkTestMessage(root.asReader());
  checkTestMessage(root);
  checkTestMessage(root.asReader());
}

TEST(Test, Unions) {
  MallocMessageBuilder builder;
  TestUnion::Builder root = builder.getRoot<TestUnion>();

  EXPECT_EQ(TestUnion::Union0::U0F0S0, root.union0->which());
  EXPECT_EQ_CAST(VOID, root.union0->u0f0s0);
  EXPECT_DEBUG_ANY_THROW(root.union0->u0f0s1);

  root.union0->u0f0s1 = true;
  EXPECT_EQ(TestUnion::Union0::U0F0S1, root.union0->which());
  EXPECT_TRUE(root.union0->u0f0s1);
  EXPECT_DEBUG_ANY_THROW(root.union0->u0f0s0);

  root.union0->u0f0s8 = 123;
  EXPECT_EQ(TestUnion::Union0::U0F0S8, root.union0->which());
  EXPECT_EQ_CAST(123, root.union0->u0f0s8);
  EXPECT_DEBUG_ANY_THROW(root.union0->u0f0s1);
}

TEST(Test, UnnamedUnion) {
  MallocMessageBuilder builder;
  auto root = builder.initRoot<test::TestUnnamedUnion>();
  EXPECT_EQ(test::TestUnnamedUnion::FOO, root.which());

  root.bar = 321;
  EXPECT_EQ(test::TestUnnamedUnion::BAR, root.which());
  EXPECT_EQ(test::TestUnnamedUnion::BAR, root.asReader().which());
  EXPECT_EQ_CAST(321u, root.bar);
  EXPECT_EQ_CAST(321u, root.asReader().bar);
  EXPECT_DEBUG_ANY_THROW(root.foo);
  EXPECT_DEBUG_ANY_THROW(root.asReader().foo);

  root.foo = 123;
  EXPECT_EQ(test::TestUnnamedUnion::FOO, root.which());
  EXPECT_EQ(test::TestUnnamedUnion::FOO, root.asReader().which());
  EXPECT_EQ_CAST(123u, root.foo);
  EXPECT_EQ_CAST(123u, root.asReader().foo);
  EXPECT_DEBUG_ANY_THROW(root.bar);
  EXPECT_DEBUG_ANY_THROW(root.asReader().bar);
}

TEST(Test, Groups) {
  MallocMessageBuilder builder;
  auto root = builder.initRoot<test::TestGroups>();

  {
    auto foo = root.groups->foo.init();
    foo.corge = 12345678;
    foo.grault = 123456789012345ll;
    foo.garply = "foobar";

    EXPECT_EQ_CAST(12345678, foo.corge);
    EXPECT_EQ_CAST(123456789012345ll, foo.grault);
    EXPECT_EQ("foobar", *foo.garply);
  }

  {
    auto bar = root.groups->bar.init();
    bar.corge = 23456789;
    bar.grault = "barbaz";
    bar.garply = 234567890123456ll;

    EXPECT_EQ_CAST(23456789, bar.corge);
    EXPECT_EQ("barbaz", *bar.grault);
    EXPECT_EQ_CAST(234567890123456ll, bar.garply);
  }

  {
    auto baz = root.groups->baz.init();
    baz.corge = 34567890;
    baz.grault = "bazqux";
    baz.garply = "quxquux";

    EXPECT_EQ_CAST(34567890, baz.corge);
    EXPECT_EQ("bazqux", *baz.grault);
    EXPECT_EQ("quxquux", *baz.garply);
  }
}

TEST(Test, Orphan) {
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
  checkTestMessage(*root.asReader().structField);
}

class TestRestorer final: public SturdyRefRestorer<test::TestSturdyRefObjectId> {
public:
  TestRestorer(int& callCount): callCount(callCount) {}

  Capability::Client restore(test::TestSturdyRefObjectId::Reader objectId) override {
    switch (objectId.tag) {
      case test::TestSturdyRefObjectId::Tag::TEST_INTERFACE:
        return kj::heap<TestInterfaceImpl>(callCount);
      case test::TestSturdyRefObjectId::Tag::TEST_EXTENDS:
        return Capability::Client(newBrokenCap("No TestExtends implemented."));
      case test::TestSturdyRefObjectId::Tag::TEST_PIPELINE:
        return kj::heap<TestPipelineImpl>(callCount);
      default:
        return Capability::Client(newBrokenCap("Not implemented."));
    }
    KJ_UNREACHABLE;
  }

private:
  int& callCount;
};

kj::AsyncIoProvider::PipeThread runServer(kj::AsyncIoProvider& ioProvider, int& callCount) {
  return ioProvider.newPipeThread(
      [&callCount](kj::AsyncIoProvider& ioProvider, kj::AsyncIoStream& stream,
                   kj::WaitScope& waitScope) {
    TwoPartyVatNetwork network(stream, rpc::twoparty::Side::SERVER);
    TestRestorer restorer(callCount);
    auto server = makeRpcServer(network, restorer);
    network.onDisconnect().wait(waitScope);
  });
}

Capability::Client getPersistentCap(RpcSystem<rpc::twoparty::SturdyRefHostId>& client,
                                    rpc::twoparty::Side side,
                                    test::TestSturdyRefObjectId::Tag tag) {
  // Create the SturdyRefHostId.
  MallocMessageBuilder hostIdMessage(8);
  auto hostId = hostIdMessage.initRoot<rpc::twoparty::SturdyRefHostId>();
  hostId.setSide(side);

  // Create the SturdyRefObjectId.
  MallocMessageBuilder objectIdMessage(8);
  objectIdMessage.initRoot<test::TestSturdyRefObjectId>().tag = tag;

  // Connect to the remote capability.
  return client.restore(hostId, objectIdMessage.getRoot<AnyPointer>());
}

TEST(Test, Pipelining) {
  auto ioContext = kj::setupAsyncIo();
  int callCount = 0;
  int reverseCallCount = 0;  // Calls back from server to client.

  auto serverThread = runServer(*ioContext.provider, callCount);
  TwoPartyVatNetwork network(*serverThread.pipe, rpc::twoparty::Side::CLIENT);
  auto rpcClient = makeRpcClient(network);

  bool disconnected = false;
  bool drained = false;
  kj::Promise<void> disconnectPromise = network.onDisconnect().then([&]() { disconnected = true; });
  kj::Promise<void> drainedPromise = network.onDrained().then([&]() { drained = true; });

  {
    // Request the particular capability from the server.
    auto client = getPersistentCap(rpcClient, rpc::twoparty::Side::SERVER,
        test::TestSturdyRefObjectId::Tag::TEST_PIPELINE).castAs<test::TestPipeline>();

    {
      // Use the capability.
      auto request = client.getCapRequest();
      request.n = 234;
      request.inCap = kj::heap<TestInterfaceImpl>(reverseCallCount);

      auto promise = request.send();

      auto pipelineRequest = promise.outBox->cap->fooRequest();
      pipelineRequest.i = 321;
      auto pipelinePromise = pipelineRequest.send();

      auto pipelineRequest2 = promise.outBox->cap->castAs<test::TestExtends>().graultRequest();
      auto pipelinePromise2 = pipelineRequest2.send();

      promise = nullptr;  // Just to be annoying, drop the original promise.

      EXPECT_EQ(0, callCount);
      EXPECT_EQ(0, reverseCallCount);

      auto response = pipelinePromise.wait(ioContext.waitScope);
      EXPECT_EQ("bar", *response.x);

      auto response2 = pipelinePromise2.wait(ioContext.waitScope);
      checkTestMessage(response2);

      EXPECT_EQ(3, callCount);
      EXPECT_EQ(1, reverseCallCount);
    }

    EXPECT_FALSE(disconnected);
    EXPECT_FALSE(drained);

    // What if we disconnect?
    serverThread.pipe->shutdownWrite();

    // The other side should also disconnect.
    disconnectPromise.wait(ioContext.waitScope);
    EXPECT_FALSE(drained);

    {
      // Use the now-broken capability.
      auto request = client.getCapRequest();
      request.n = 234;
      request.inCap = kj::heap<TestInterfaceImpl>(reverseCallCount);

      auto promise = request.send();

      auto pipelineRequest = promise.outBox->cap->fooRequest();
      pipelineRequest.i = 321;
      auto pipelinePromise = pipelineRequest.send();

      auto pipelineRequest2 = promise.outBox->cap->castAs<test::TestExtends>().graultRequest();
      auto pipelinePromise2 = pipelineRequest2.send();

      EXPECT_ANY_THROW(pipelinePromise.wait(ioContext.waitScope));
      EXPECT_ANY_THROW(pipelinePromise2.wait(ioContext.waitScope));

      EXPECT_EQ(3, callCount);
      EXPECT_EQ(1, reverseCallCount);
    }

    EXPECT_FALSE(drained);
  }

  drainedPromise.wait(ioContext.waitScope);
}

}  // namespace
}  // namespace _ (private)
}  // namespace capnp
