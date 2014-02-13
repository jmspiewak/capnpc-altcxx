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

// This program is a code generator plugin for `capnp compile` which generates C++ code.

#include <capnp/schema.capnp.h>
#include <capnp/serialize.h>
#include <kj/debug.h>
#include <kj/io.h>
#include <kj/string-tree.h>
#include <kj/vector.h>
#include <capnp/schema-loader.h>
#include <capnp/dynamic.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <kj/main.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef VERSION
#define VERSION "(unknown)"
#endif

namespace capnp {
namespace altcxx {
namespace {

static constexpr uint64_t NAMESPACE_ANNOTATION_ID = 0xb9c6f99ebf805f2cull;
static constexpr uint64_t RENAME_ANNOTATION_ID    = 0xa700d7fb1907fdd8ull;

static constexpr const char* FIELD_SIZE_NAMES[] = {
  "VOID", "BIT", "BYTE", "TWO_BYTES", "FOUR_BYTES", "EIGHT_BYTES", "POINTER", "INLINE_COMPOSITE"
};

bool hasDiscriminantValue(const schema::Field::Reader& reader) {
  return reader.getDiscriminantValue() != schema::Field::NO_DISCRIMINANT;
}

void enumerateDeps(schema::Type::Reader type, std::set<uint64_t>& deps) {
  switch (type.which()) {
    case schema::Type::STRUCT:
      deps.insert(type.getStruct().getTypeId());
      break;
    case schema::Type::ENUM:
      deps.insert(type.getEnum().getTypeId());
      break;
    case schema::Type::INTERFACE:
      deps.insert(type.getInterface().getTypeId());
      break;
    case schema::Type::LIST:
      enumerateDeps(type.getList().getElementType(), deps);
      break;
    default:
      break;
  }
}

void enumerateDeps(schema::Node::Reader node, std::set<uint64_t>& deps) {
  switch (node.which()) {
    case schema::Node::STRUCT: {
      auto structNode = node.getStruct();
      for (auto field: structNode.getFields()) {
        switch (field.which()) {
          case schema::Field::SLOT:
            enumerateDeps(field.getSlot().getType(), deps);
            break;
          case schema::Field::GROUP:
            deps.insert(field.getGroup().getTypeId());
            break;
        }
      }
      if (structNode.getIsGroup()) {
        deps.insert(node.getScopeId());
      }
      break;
    }
    case schema::Node::INTERFACE: {
      auto interfaceNode = node.getInterface();
      for (auto extend: interfaceNode.getExtends()) {
        deps.insert(extend);
      }
      for (auto method: interfaceNode.getMethods()) {
        deps.insert(method.getParamStructType());
        deps.insert(method.getResultStructType());
      }
      break;
    }
    default:
      break;
  }
}

struct OrderByName {
  template <typename T>
  inline bool operator()(const T& a, const T& b) const {
    return a.getProto().getName() < b.getProto().getName();
  }
};

template <typename MemberList>
kj::Array<uint> makeMembersByName(MemberList&& members) {
  auto sorted = KJ_MAP(member, members) { return member; };
  std::sort(sorted.begin(), sorted.end(), OrderByName());
  return KJ_MAP(member, sorted) { return member.getIndex(); };
}

kj::StringPtr baseName(kj::StringPtr path) {
  KJ_IF_MAYBE(slashPos, path.findLast('/')) {
    return path.slice(*slashPos + 1);
  } else {
    return path;
  }
}

// =======================================================================================

class CapnpcAltCxxMain {
public:
  CapnpcAltCxxMain(kj::ProcessContext& context): context(context) {}

  kj::MainFunc getMain() {
    return kj::MainBuilder(context, VERSION, "Cap'n Proto alternative c++ plugin",
        "This Cap'n Proto compiler plugin generates C++ classes with property-like fields")
        .callAfterParsing(KJ_BIND_METHOD(*this, run))
        .build();
  }

private:
  kj::ProcessContext& context;
  SchemaLoader schemaLoader;
  std::unordered_set<uint64_t> usedImports;
  std::map<uint64_t, bool> needsPipelineCache;
  bool hasInterfaces = false;

  kj::StringTree cppFullName(Schema schema) {
    auto node = schema.getProto();
    if (node.getScopeId() == 0) {
      usedImports.insert(node.getId());
      for (auto annotation: node.getAnnotations()) {
        if (annotation.getId() == NAMESPACE_ANNOTATION_ID) {
          return kj::strTree(" ::", annotation.getValue().getText());
        }
      }
      return kj::strTree(" ");
    } else {
      Schema parent = schemaLoader.get(node.getScopeId());
      for (auto nested: parent.getProto().getNestedNodes()) {
        if (nested.getId() == node.getId()) {
          return kj::strTree(cppFullName(parent), "::", nested.getName());
        }
      }
      KJ_FAIL_REQUIRE("A schema Node's supposed scope did not contain the node as a NestedNode.");
    }
  }

  kj::String toUpperCase(kj::StringPtr name) {
    kj::Vector<char> result(name.size() + 4);

    for (char c: name) {
      if ('a' <= c && c <= 'z') {
        result.add(c - 'a' + 'A');
      } else if (result.size() > 0 && 'A' <= c && c <= 'Z') {
        result.add('_');
        result.add(c);
      } else {
        result.add(c);
      }
    }

    result.add('\0');

    return kj::String(result.releaseAsArray());
  }

  kj::String toTitleCase(kj::StringPtr name) {
    kj::String result = kj::heapString(name);
    if ('a' <= result[0] && result[0] <= 'z') {
      result[0] = result[0] - 'a' + 'A';
    }
    return kj::mv(result);
  }

  kj::StringTree typeName(schema::Type::Reader type) {
    switch (type.which()) {
      case schema::Type::VOID: return kj::strTree(" ::capnp::Void");

      case schema::Type::BOOL: return kj::strTree("bool");
      case schema::Type::INT8: return kj::strTree(" ::int8_t");
      case schema::Type::INT16: return kj::strTree(" ::int16_t");
      case schema::Type::INT32: return kj::strTree(" ::int32_t");
      case schema::Type::INT64: return kj::strTree(" ::int64_t");
      case schema::Type::UINT8: return kj::strTree(" ::uint8_t");
      case schema::Type::UINT16: return kj::strTree(" ::uint16_t");
      case schema::Type::UINT32: return kj::strTree(" ::uint32_t");
      case schema::Type::UINT64: return kj::strTree(" ::uint64_t");
      case schema::Type::FLOAT32: return kj::strTree("float");
      case schema::Type::FLOAT64: return kj::strTree("double");

      case schema::Type::TEXT: return kj::strTree(" ::capnp::Text");
      case schema::Type::DATA: return kj::strTree(" ::capnp::Data");

      case schema::Type::ENUM:
        return cppFullName(schemaLoader.get(type.getEnum().getTypeId()));
      case schema::Type::STRUCT:
        return cppFullName(schemaLoader.get(type.getStruct().getTypeId()));
      case schema::Type::INTERFACE:
        return cppFullName(schemaLoader.get(type.getInterface().getTypeId()));

      case schema::Type::LIST:
        return kj::strTree(" ::capnp::List<", typeName(type.getList().getElementType()), ">");

      case schema::Type::ANY_POINTER:
        // Not used.
        return kj::strTree();
    }
    KJ_UNREACHABLE;
  }

  kj::StringTree literalValue(schema::Type::Reader type, schema::Value::Reader value) {
    switch (value.which()) {
      case schema::Value::VOID: return kj::strTree(" ::capnp::VOID");
      case schema::Value::BOOL: return kj::strTree(value.getBool() ? "true" : "false");
      case schema::Value::INT8: return kj::strTree(value.getInt8());
      case schema::Value::INT16: return kj::strTree(value.getInt16());
      case schema::Value::INT32: return kj::strTree(value.getInt32());
      case schema::Value::INT64: return kj::strTree(value.getInt64(), "ll");
      case schema::Value::UINT8: return kj::strTree(value.getUint8(), "u");
      case schema::Value::UINT16: return kj::strTree(value.getUint16(), "u");
      case schema::Value::UINT32: return kj::strTree(value.getUint32(), "u");
      case schema::Value::UINT64: return kj::strTree(value.getUint64(), "llu");
      case schema::Value::FLOAT32: return kj::strTree(value.getFloat32(), "f");
      case schema::Value::FLOAT64: return kj::strTree(value.getFloat64());
      case schema::Value::ENUM: {
        EnumSchema schema = schemaLoader.get(type.getEnum().getTypeId()).asEnum();
        if (value.getEnum() < schema.getEnumerants().size()) {
          return kj::strTree(
              cppFullName(schema), "::",
              toUpperCase(schema.getEnumerants()[value.getEnum()].getProto().getName()));
        } else {
          return kj::strTree("static_cast<", cppFullName(schema), ">(", value.getEnum(), ")");
        }
      }

      case schema::Value::TEXT:
      case schema::Value::DATA:
      case schema::Value::STRUCT:
      case schema::Value::INTERFACE:
      case schema::Value::LIST:
      case schema::Value::ANY_POINTER:
        KJ_FAIL_REQUIRE("literalValue() can only be used on primitive types.");
    }
    KJ_UNREACHABLE;
  }

  // -----------------------------------------------------------------
  // Code to deal with "slots" -- determines what to zero out when we clear a group.

  static uint typeSizeBits(schema::Type::Which whichType) {
    switch (whichType) {
      case schema::Type::BOOL: return 1;
      case schema::Type::INT8: return 8;
      case schema::Type::INT16: return 16;
      case schema::Type::INT32: return 32;
      case schema::Type::INT64: return 64;
      case schema::Type::UINT8: return 8;
      case schema::Type::UINT16: return 16;
      case schema::Type::UINT32: return 32;
      case schema::Type::UINT64: return 64;
      case schema::Type::FLOAT32: return 32;
      case schema::Type::FLOAT64: return 64;
      case schema::Type::ENUM: return 16;

      case schema::Type::VOID:
      case schema::Type::TEXT:
      case schema::Type::DATA:
      case schema::Type::LIST:
      case schema::Type::STRUCT:
      case schema::Type::INTERFACE:
      case schema::Type::ANY_POINTER:
        KJ_FAIL_REQUIRE("Should only be called for data types.");
    }
    KJ_UNREACHABLE;
  }

  enum class Section {
    NONE,
    DATA,
    POINTERS
  };

  static Section sectionFor(schema::Type::Which whichType) {
    switch (whichType) {
      case schema::Type::VOID:
        return Section::NONE;
      case schema::Type::BOOL:
      case schema::Type::INT8:
      case schema::Type::INT16:
      case schema::Type::INT32:
      case schema::Type::INT64:
      case schema::Type::UINT8:
      case schema::Type::UINT16:
      case schema::Type::UINT32:
      case schema::Type::UINT64:
      case schema::Type::FLOAT32:
      case schema::Type::FLOAT64:
      case schema::Type::ENUM:
        return Section::DATA;
      case schema::Type::TEXT:
      case schema::Type::DATA:
      case schema::Type::LIST:
      case schema::Type::STRUCT:
      case schema::Type::INTERFACE:
      case schema::Type::ANY_POINTER:
        return Section::POINTERS;
    }
    KJ_UNREACHABLE;
  }

  static kj::StringPtr maskType(schema::Type::Which whichType) {
    switch (whichType) {
      case schema::Type::BOOL: return "bool";
      case schema::Type::INT8: return " ::uint8_t";
      case schema::Type::INT16: return " ::uint16_t";
      case schema::Type::INT32: return " ::uint32_t";
      case schema::Type::INT64: return " ::uint64_t";
      case schema::Type::UINT8: return " ::uint8_t";
      case schema::Type::UINT16: return " ::uint16_t";
      case schema::Type::UINT32: return " ::uint32_t";
      case schema::Type::UINT64: return " ::uint64_t";
      case schema::Type::FLOAT32: return " ::uint32_t";
      case schema::Type::FLOAT64: return " ::uint64_t";
      case schema::Type::ENUM: return " ::uint16_t";

      case schema::Type::VOID:
      case schema::Type::TEXT:
      case schema::Type::DATA:
      case schema::Type::LIST:
      case schema::Type::STRUCT:
      case schema::Type::INTERFACE:
      case schema::Type::ANY_POINTER:
        KJ_FAIL_REQUIRE("Should only be called for data types.");
    }
    KJ_UNREACHABLE;
  }

  struct Slot {
    schema::Type::Which whichType;
    uint offset;

    bool isSupersetOf(Slot other) const {
      auto section = sectionFor(whichType);
      if (section != sectionFor(other.whichType)) return false;
      switch (section) {
        case Section::NONE:
          return true;  // all voids overlap
        case Section::DATA: {
          auto bits = typeSizeBits(whichType);
          auto start = offset * bits;
          auto otherBits = typeSizeBits(other.whichType);
          auto otherStart = other.offset * otherBits;
          return start <= otherStart && otherStart + otherBits <= start + bits;
        }
        case Section::POINTERS:
          return offset == other.offset;
      }
      KJ_UNREACHABLE;
    }

    bool operator<(Slot other) const {
      // Sort by section, then start position, and finally size.

      auto section = sectionFor(whichType);
      auto otherSection = sectionFor(other.whichType);
      if (section < otherSection) {
        return true;
      } else if (section > otherSection) {
        return false;
      }

      switch (section) {
        case Section::NONE:
          return false;
        case Section::DATA: {
          auto bits = typeSizeBits(whichType);
          auto start = offset * bits;
          auto otherBits = typeSizeBits(other.whichType);
          auto otherStart = other.offset * otherBits;
          if (start < otherStart) {
            return true;
          } else if (start > otherStart) {
            return false;
          }

          // Sort larger sizes before smaller.
          return bits > otherBits;
        }
        case Section::POINTERS:
          return offset < other.offset;
      }
      KJ_UNREACHABLE;
    }
  };

  void getSlots(StructSchema schema, kj::Vector<Slot>& slots) {
    auto structProto = schema.getProto().getStruct();
    if (structProto.getDiscriminantCount() > 0) {
      slots.add(Slot { schema::Type::UINT16, structProto.getDiscriminantOffset() });
    }

    for (auto field: schema.getFields()) {
      auto proto = field.getProto();
      switch (proto.which()) {
        case schema::Field::SLOT: {
          auto slot = proto.getSlot();
          slots.add(Slot { slot.getType().which(), slot.getOffset() });
          break;
        }
        case schema::Field::GROUP:
          getSlots(schema.getDependency(proto.getGroup().getTypeId()).asStruct(), slots);
          break;
      }
    }
  }

  kj::Array<Slot> getSortedSlots(StructSchema schema) {
    // Get a representation of all of the field locations owned by this schema, e.g. so that they
    // can be zero'd out.

    kj::Vector<Slot> slots(schema.getFields().size());
    getSlots(schema, slots);
    std::sort(slots.begin(), slots.end());

    kj::Vector<Slot> result(slots.size());

    // All void slots are redundant, and they sort towards the front of the list.  By starting out
    // with `prevSlot` = void, we will end up skipping them all, which is what we want.
    Slot prevSlot = { schema::Type::VOID, 0 };
    for (auto slot: slots) {
      if (prevSlot.isSupersetOf(slot)) {
        // This slot is redundant as prevSlot is a superset of it.
        continue;
      }

      // Since all sizes are power-of-two, if two slots overlap at all, one must be a superset of
      // the other.  Since we sort slots by starting position, we know that the only way `slot`
      // could be a superset of `prevSlot` is if they have the same starting position.  However,
      // since we sort slots with the same starting position by descending size, this is not
      // possible.
      KJ_DASSERT(!slot.isSupersetOf(prevSlot));

      result.add(slot);

      prevSlot = slot;
    }

    return result.releaseAsArray();
  }

  // -----------------------------------------------------------------

  bool needsPipeline(StructSchema schema) {
    auto pair = needsPipelineCache.insert({schema.getProto().getId(), false});

    if (!pair.second) {
      return pair.first->second;
    } else {
      for (StructSchema::Field field: schema.getFields()) {
        uint64_t typeId;

        if (field.getProto().isGroup()) {
          typeId = field.getProto().getGroup().getTypeId();
        } else {
          auto type = field.getProto().getSlot().getType();

          switch (type.which()) {
            case schema::Type::STRUCT:
              typeId = type.getStruct().getTypeId();
              break;

            case schema::Type::INTERFACE:
              pair.first->second = true;
              return true;

            default:
              continue;
          }
        }

        if (needsPipeline(schema.getDependency(typeId).asStruct())) {
          pair.first->second = true;
          return true;
        }
      }
    }

    return false;
  }

  // -----------------------------------------------------------------

  struct FieldText {
    kj::StringTree unionCheck;
    kj::StringTree property;
    kj::StringTree pipelineProperty;
  };

  enum class FieldKind {
    PRIMITIVE,
    BLOB,
    STRUCT,
    LIST,
    INTERFACE,
    ANY_POINTER
  };

  FieldText makeFieldText(StructSchema::Field field) {
    auto proto = field.getProto();

    kj::StringPtr name = proto.getName();
    kj::StringPtr propertyName = name;
    kj::String titleCase = toTitleCase(name);

    bool inUnion = hasDiscriminantValue(proto);
    kj::StringTree maybeInUnion;
    kj::StringTree unionCheck;

    for (auto annotation: proto.getAnnotations()) {
      if (annotation.getId() == RENAME_ANNOTATION_ID) {
        propertyName = annotation.getValue().getText();
        break;
      }
    }

    if (inUnion) {
      auto containingStruct = field.getContainingStruct();
      auto discrimOff = containingStruct.getProto().getStruct().getDiscriminantOffset();
      kj::String upperCase = toUpperCase(name);

      unionCheck = kj::strTree("  bool is", titleCase, "() { return which() == ",
                               upperCase, "; }\n");

      maybeInUnion = kj::strTree(", ::capnp::altcxx::InUnion<", discrimOff, ", ",
                                 upperCase, '>');
    }

    kj::StringPtr prefix = "    ::capnp::altcxx::";
    kj::StringTree propertyTail = kj::strTree(kj::mv(maybeInUnion), "> ", propertyName, ";\n");

    if (proto.isGroup()) {
      constexpr int initOpIndent = 35;
      kj::String initOpPrefix = kj::str(kj::repeat(' ', initOpIndent), "::capnp::altcxx::");

      auto slots = getSortedSlots(schemaLoader.get(field.getProto().getGroup().getTypeId())
                                  .asStruct());

      return FieldText {
        kj::mv(unionCheck),

        kj::strTree(prefix, "GroupProperty<Impl, ", titleCase,
          ", ::capnp::altcxx::GroupInitializer<\n",
          KJ_MAP(slot, slots) {
            switch (sectionFor(slot.whichType)) {
              case Section::NONE:
                return kj::strTree();

              case Section::DATA:
                return kj::strTree(initOpPrefix, "ZeroElement<", slot.offset, ", ",
                  maskType(slot.whichType), ">,\n");

              case Section::POINTERS:
                return kj::strTree(initOpPrefix, "ClearPointer<", slot.offset, ">,\n");
            }
            KJ_UNREACHABLE;
          },
          kj::repeat(' ', initOpIndent), "::capnp::altcxx::NoOp>", kj::mv(propertyTail)),

        hasDiscriminantValue(proto) ? kj::strTree() :
          kj::strTree(prefix, "PipelineProperty<", titleCase, "> ", propertyName, ";\n"),
      };
    }

    auto slot = proto.getSlot();

    FieldKind kind = FieldKind::PRIMITIVE;
    kj::String type = typeName(slot.getType()).flatten();
    kj::String defaultMask;    // primitives only
    size_t defaultOffset = 0;    // pointers only: offset of the default value within the schema.
    size_t defaultSize = 0;      // blobs only: byte size of the default value.

    auto typeBody = slot.getType();
    auto defaultBody = slot.getDefaultValue();
    switch (typeBody.which()) {
      case schema::Type::VOID:
        kind = FieldKind::PRIMITIVE;
        break;

#define HANDLE_PRIMITIVE(discrim, typeName, defaultName, suffix) \
      case schema::Type::discrim: \
        kind = FieldKind::PRIMITIVE; \
        if (defaultBody.get##defaultName() != 0) { \
          defaultMask = kj::str(defaultBody.get##defaultName(), #suffix); \
        } \
        break;

      HANDLE_PRIMITIVE(BOOL, bool, Bool, );
      HANDLE_PRIMITIVE(INT8 , ::int8_t , Int8 , );
      HANDLE_PRIMITIVE(INT16, ::int16_t, Int16, );
      HANDLE_PRIMITIVE(INT32, ::int32_t, Int32, );
      HANDLE_PRIMITIVE(INT64, ::int64_t, Int64, ll);
      HANDLE_PRIMITIVE(UINT8 , ::uint8_t , Uint8 , u);
      HANDLE_PRIMITIVE(UINT16, ::uint16_t, Uint16, u);
      HANDLE_PRIMITIVE(UINT32, ::uint32_t, Uint32, u);
      HANDLE_PRIMITIVE(UINT64, ::uint64_t, Uint64, ull);
#undef HANDLE_PRIMITIVE

      case schema::Type::FLOAT32:
        kind = FieldKind::PRIMITIVE;
        if (defaultBody.getFloat32() != 0) {
          uint32_t mask;
          float value = defaultBody.getFloat32();
          static_assert(sizeof(mask) == sizeof(value), "bug");
          memcpy(&mask, &value, sizeof(mask));
          defaultMask = kj::str(mask, "u");
        }
        break;

      case schema::Type::FLOAT64:
        kind = FieldKind::PRIMITIVE;
        if (defaultBody.getFloat64() != 0) {
          uint64_t mask;
          double value = defaultBody.getFloat64();
          static_assert(sizeof(mask) == sizeof(value), "bug");
          memcpy(&mask, &value, sizeof(mask));
          defaultMask = kj::str(mask, "ull");
        }
        break;

      case schema::Type::TEXT:
        kind = FieldKind::BLOB;
        if (defaultBody.hasText()) {
          defaultOffset = field.getDefaultValueSchemaOffset();
          defaultSize = defaultBody.getText().size();
        }
        break;
      case schema::Type::DATA:
        kind = FieldKind::BLOB;
        if (defaultBody.hasData()) {
          defaultOffset = field.getDefaultValueSchemaOffset();
          defaultSize = defaultBody.getData().size();
        }
        break;

      case schema::Type::ENUM:
        kind = FieldKind::PRIMITIVE;
        if (defaultBody.getEnum() != 0) {
          defaultMask = kj::str(defaultBody.getEnum(), "u");
        }
        break;

      case schema::Type::STRUCT:
        kind = FieldKind::STRUCT;
        if (defaultBody.hasStruct()) {
          defaultOffset = field.getDefaultValueSchemaOffset();
        }
        break;
      case schema::Type::LIST:
        kind = FieldKind::LIST;
        if (defaultBody.hasList()) {
          defaultOffset = field.getDefaultValueSchemaOffset();
        }
        break;
      case schema::Type::INTERFACE:
        kind = FieldKind::INTERFACE;
        break;
      case schema::Type::ANY_POINTER:
        kind = FieldKind::ANY_POINTER;
        if (defaultBody.hasAnyPointer()) {
          defaultOffset = field.getDefaultValueSchemaOffset();
        }
        break;
    }

    uint offset = slot.getOffset();
    kj::String propertyMaskParam;

    if (defaultMask.size() > 0) {
      propertyMaskParam = kj::str(", ", defaultMask);
    } else if (inUnion) {
      propertyMaskParam = kj::str(", 0u");
    }

    if (kind == FieldKind::PRIMITIVE) {
      return FieldText {
        kj::mv(unionCheck),
        kj::strTree(prefix, "PrimitiveProperty<Impl, ", offset, ", ", type,
                    kj::mv(propertyMaskParam), kj::mv(propertyTail))
      };

    } else if (kind == FieldKind::INTERFACE) {
      return FieldText {
        kj::mv(unionCheck),
        kj::strTree(prefix, "InterfaceProperty<Impl, ", offset, ", ",
                    type, kj::mv(propertyTail)),

        kj::strTree(hasDiscriminantValue(proto) ? kj::strTree() : kj::strTree(
          prefix, "PipelineProperty<", type,
          ", ::capnp::altcxx::PipelineGetPointerAsCapOp<", offset, ">> ", propertyName, ";\n"))
      };

    } else if (kind == FieldKind::ANY_POINTER) {
      return FieldText {
        kj::mv(unionCheck),
        kj::strTree(prefix, "AnyPointerProperty<Impl, ", offset, kj::mv(propertyTail))
      };

    } else {
      uint64_t typeId = field.getContainingStruct().getProto().getId();
      kj::String propertyDefault;

      if (defaultOffset == 0) {
        if (inUnion) propertyDefault = kj::str(", ::capnp::altcxx::NoDefault");
      } else {
        propertyDefault = kj::str(", ::capnp::altcxx::PointerDefault<&::capnp::schemas::s_",
            kj::hex(typeId), ", ", defaultOffset, defaultSize == 0 ? kj::strTree() :
            kj::strTree(", ", defaultSize), '>');
      }

      kj::StringPtr propertyType;

      switch (kind) {
        case FieldKind::STRUCT:
          propertyType = "StructProperty";
          break;

        case FieldKind::BLOB:
          propertyType = typeBody.isText() ? "TextProperty" : "BlobProperty";
          break;

        case FieldKind::LIST:
          propertyType = "ListProperty";
          type = kj::str(typeName(typeBody.getList().getElementType()));
          break;

        default:
          KJ_UNREACHABLE
          break;
      }

      return FieldText {
        kj::mv(unionCheck),

        kj::strTree(prefix, propertyType, "<Impl, ", offset,
                    typeBody.isText() ? kj::strTree() : kj::strTree(", ", type),
                    kj::mv(propertyDefault), kj::mv(propertyTail)),

        kj::strTree(kind == FieldKind::STRUCT && !hasDiscriminantValue(proto)
                    ? kj::strTree(prefix, "PipelineProperty<", type,
                                  ", ::capnp::altcxx::PipelineGetPointerOp<", offset, ">> ",
                                  propertyName, ";\n")
                    : kj::strTree())
      };
    }
  }

  // -----------------------------------------------------------------

  struct StructText {
    kj::StringTree outerTypeDecl;
    kj::StringTree outerTypeDef;
    kj::StringTree readerBuilderDefs;
  };

  kj::StringTree makeBaseDef(kj::StringPtr fullName, bool isUnion, uint discrimOffset,
                             kj::Array<kj::StringTree>&& methods,
                             kj::Array<kj::StringTree>&& properties) {
    return kj::strTree(
        "template <typename Impl>\n"
        "class ", fullName, "::Base {\n"
        "  typedef typename Impl::template UnionMember<Base> UnionMember;\n"
        "\n"
        "public:\n"
        "  ALTCXX_BASE_CTORS_DTOR_ASSIGNS\n"
        "\n"
        "  ::capnp::MessageSize totalSize() const {\n"
        "    return _reader().totalSize().asPublic();\n"
        "  }\n"
        "\n",
        isUnion ? kj::strTree("  Which which() { return _impl.template getDataField<Which>(",
                              discrimOffset, " * ::capnp::ELEMENTS); }\n") : kj::strTree(),
        kj::mv(methods),
        "  union {\n",
        kj::mv(properties),
        "    UnionMember _impl;\n"
        "  };\n"
        "\n"
        "protected:\n"
        "  ::capnp::_::StructReader _reader() const { return _impl.asReader(); }\n"
        "\n"
        "  friend ::kj::StringTree KJ_STRINGIFY(Base base) {\n"
        "    return ::capnp::_::structString<", fullName, ">(base._reader());\n"
        "  }\n"
        "};\n"
        "\n");
  }

  kj::StringTree makePipelineDef(kj::StringPtr fullName, kj::StringPtr unqualifiedParentType,
                                 kj::Array<kj::StringTree>&& properties) {
    return kj::strTree(
        "class ", fullName, "::Pipeline {\n"
        "public:\n"
        "  typedef ", unqualifiedParentType, " Pipelines;\n"
        "\n"
        "  inline Pipeline(decltype(nullptr)): _typeless(nullptr) {}\n"
        "  inline Pipeline(Pipeline&& other): _typeless(kj::mv(other._typeless)) {}\n"
        "  inline explicit Pipeline(::capnp::AnyPointer::Pipeline&& typeless)\n"
        "      : _typeless(kj::mv(typeless)) {}\n"
        "  inline ~Pipeline() { kj::dtor(_typeless); }\n"
        "  inline Pipeline& operator = (Pipeline&& other) { _typeless = kj::mv(other._typeless); return *this; }"
        "\n",
        "  union {\n",
        kj::mv(properties),
        "    ::capnp::altcxx::PrivatePipeline<Pipeline> _typeless;\n"
        "  };\n"
        "};\n"
        "\n");
  }

  StructText makeStructText(kj::StringPtr scope, kj::StringPtr name, StructSchema schema,
                            kj::Array<kj::StringTree> nestedTypeDecls) {
    auto proto = schema.getProto();
    auto fullName = kj::str(scope, name);
    bool noPipeline = !needsPipeline(schema);
    auto fieldTexts = KJ_MAP(f, schema.getFields()) { return makeFieldText(f); };

    auto structNode = proto.getStruct();
    uint discrimOffset = structNode.getDiscriminantOffset();

    return StructText {
      kj::strTree(
          "  struct ", name, ";\n"),

      kj::strTree(
          "struct ", fullName, " {\n",
          "  ", name, "() = delete;\n"
          "\n"
          "  template <typename Impl>\n"
          "  class Base;\n"
          "\n"
          "  typedef ::capnp::altcxx::Reader<", name, "> Reader;\n"
          "  typedef ::capnp::altcxx::Builder<", name, "> Builder;\n"
          "  ", noPipeline ? kj::strTree("typedef ::capnp::altcxx::DummyPipeline<", name, '>') :
                             kj::strTree("class"), " Pipeline;\n"
          "\n",
          structNode.getDiscriminantCount() == 0 ? kj::strTree() : kj::strTree(
              "  enum Which: uint16_t {\n",
              KJ_MAP(f, structNode.getFields()) {
                if (hasDiscriminantValue(f)) {
                  return kj::strTree("    ", toUpperCase(f.getName()), ",\n");
                } else {
                  return kj::strTree();
                }
              },
              "  };\n"),
          KJ_MAP(n, nestedTypeDecls) { return kj::mv(n); },
          "};\n"
          "\n"),

      kj::strTree(
          makeBaseDef(fullName, structNode.getDiscriminantCount() != 0, discrimOffset,
                      KJ_MAP(f, fieldTexts) { return kj::mv(f.unionCheck); },
                      KJ_MAP(f, fieldTexts) { return kj::mv(f.property); }),
          noPipeline ? kj::strTree() : makePipelineDef(fullName, name,
              KJ_MAP(f, fieldTexts) { return kj::mv(f.pipelineProperty); }))
    };
  }

  // -----------------------------------------------------------------

  struct MethodText {
    kj::StringTree clientDecls;
    kj::StringTree serverDecls;
    kj::StringTree inlineDefs;
    kj::StringTree sourceDefs;
    kj::StringTree dispatchCase;
  };

  MethodText makeMethodText(kj::StringPtr interfaceName, InterfaceSchema::Method method) {
    auto proto = method.getProto();
    auto name = proto.getName();
    auto titleCase = toTitleCase(name);
    auto paramSchema = schemaLoader.get(proto.getParamStructType()).asStruct();
    auto resultSchema = schemaLoader.get(proto.getResultStructType()).asStruct();

    auto paramProto = paramSchema.getProto();
    auto resultProto = resultSchema.getProto();

    kj::String paramType = paramProto.getScopeId() == 0 ?
        kj::str(interfaceName, "::", titleCase, "Params") : cppFullName(paramSchema).flatten();
    kj::String resultType = resultProto.getScopeId() == 0 ?
        kj::str(interfaceName, "::", titleCase, "Results") : cppFullName(resultSchema).flatten();

    kj::String shortParamType = paramProto.getScopeId() == 0 ?
        kj::str(titleCase, "Params") : cppFullName(paramSchema).flatten();
    kj::String shortResultType = resultProto.getScopeId() == 0 ?
        kj::str(titleCase, "Results") : cppFullName(resultSchema).flatten();

    auto interfaceProto = method.getContainingInterface().getProto();
    uint64_t interfaceId = interfaceProto.getId();
    auto interfaceIdHex = kj::hex(interfaceId);
    uint16_t methodId = method.getIndex();

    return MethodText {
      kj::strTree(
          "  ::capnp::Request<", paramType, ", ", resultType, "> ", name, "Request(\n"
          "      ::kj::Maybe< ::capnp::MessageSize> sizeHint = nullptr);\n"),

      kj::strTree(
          paramProto.getScopeId() != 0 ? kj::strTree() : kj::strTree(
              "  typedef ", paramType, " ", titleCase, "Params;\n"),
          resultProto.getScopeId() != 0 ? kj::strTree() : kj::strTree(
              "  typedef ", resultType, " ", titleCase, "Results;\n"),
          "  typedef ::capnp::CallContext<", shortParamType, ", ", shortResultType, "> ",
                titleCase, "Context;\n"
          "  virtual ::kj::Promise<void> ", name, "(", titleCase, "Context context);\n"),

      kj::strTree(),

      kj::strTree(
          "::capnp::Request<", paramType, ", ", resultType, ">\n",
          interfaceName, "::Client::", name, "Request(::kj::Maybe< ::capnp::MessageSize> sizeHint) {\n"
          "  return newCall<", paramType, ", ", resultType, ">(\n"
          "      0x", interfaceIdHex, "ull, ", methodId, ", sizeHint);\n"
          "}\n"
          "::kj::Promise<void> ", interfaceName, "::Server::", name, "(", titleCase, "Context) {\n"
          "  return ::capnp::Capability::Server::internalUnimplemented(\n"
          "      \"", interfaceProto.getDisplayName(), "\", \"", name, "\",\n"
          "      0x", interfaceIdHex, "ull, ", methodId, ");\n"
          "}\n"),

      kj::strTree(
          "    case ", methodId, ":\n"
          "      return ", name, "(::capnp::Capability::Server::internalGetTypedContext<\n"
          "          ", paramType, ", ", resultType, ">(context));\n")
    };
  }

  struct InterfaceText {
    kj::StringTree outerTypeDecl;
    kj::StringTree outerTypeDef;
    kj::StringTree clientServerDefs;
    kj::StringTree inlineMethodDefs;
    kj::StringTree sourceDefs;
  };

  struct ExtendInfo {
    kj::String typeName;
    uint64_t id;
  };

  InterfaceText makeInterfaceText(kj::StringPtr scope, kj::StringPtr name, InterfaceSchema schema,
                                  kj::Array<kj::StringTree> nestedTypeDecls) {
    auto fullName = kj::str(scope, name);
    auto methods = KJ_MAP(m, schema.getMethods()) { return makeMethodText(fullName, m); };

    auto proto = schema.getProto();

    auto extends = KJ_MAP(id, proto.getInterface().getExtends()) {
      Schema schema = schemaLoader.get(id);
      return ExtendInfo { cppFullName(schema).flatten(), schema.getProto().getId() };
    };

    return InterfaceText {
      kj::strTree(
          "  struct ", name, ";\n"),

      kj::strTree(
          "struct ", fullName, " {\n",
          "  ", name, "() = delete;\n"
          "\n"
          "  class Client;\n"
          "  class Server;\n"
          "\n",
          KJ_MAP(n, nestedTypeDecls) { return kj::mv(n); },
          "};\n"
          "\n"),

      kj::strTree(
          "class ", fullName, "::Client\n"
          "    : public virtual ::capnp::Capability::Client",
          KJ_MAP(e, extends) {
            return kj::strTree(",\n      public virtual ", e.typeName, "::Client");
          }, " {\n"
          "public:\n"
          "  typedef ", fullName, " Calls;\n"
          "  typedef ", fullName, " Reads;\n"
          "\n"
          "  Client(decltype(nullptr));\n"
          "  explicit Client(::kj::Own< ::capnp::ClientHook>&& hook);\n"
          "  template <typename T, typename = ::kj::EnableIf< ::kj::canConvert<T*, Server*>()>>\n"
          "  Client(::kj::Own<T>&& server);\n"
          "  template <typename T, typename = ::kj::EnableIf< ::kj::canConvert<T*, Client*>()>>\n"
          "  Client(::kj::Promise<T>&& promise);\n"
          "  Client(::kj::Exception&& exception);\n"
          "  Client(Client&) = default;\n"
          "  Client(Client&&) = default;\n"
          "  Client& operator=(Client& other);\n"
          "  Client& operator=(Client&& other);\n"
          "\n",
          KJ_MAP(m, methods) { return kj::mv(m.clientDecls); },
          "\n"
          "protected:\n"
          "  Client() = default;\n"
          "};\n"
          "\n"
          "class ", fullName, "::Server\n"
          "    : public virtual ::capnp::Capability::Server",
          KJ_MAP(e, extends) {
            return kj::strTree(",\n      public virtual ", e.typeName, "::Server");
          }, " {\n"
          "public:\n",
          "  typedef ", fullName, " Serves;\n"
          "\n"
          "  ::kj::Promise<void> dispatchCall(uint64_t interfaceId, uint16_t methodId,\n"
          "      ::capnp::CallContext< ::capnp::AnyPointer, ::capnp::AnyPointer> context)\n"
          "      override;\n"
          "\n"
          "protected:\n",
          KJ_MAP(m, methods) { return kj::mv(m.serverDecls); },
          "\n"
          "  ::kj::Promise<void> dispatchCallInternal(uint16_t methodId,\n"
          "      ::capnp::CallContext< ::capnp::AnyPointer, ::capnp::AnyPointer> context);\n"
          "};\n"
          "\n"),

      kj::strTree(
          "inline ", fullName, "::Client::Client(decltype(nullptr))\n"
          "    : ::capnp::Capability::Client(nullptr) {}\n"
          "inline ", fullName, "::Client::Client(\n"
          "    ::kj::Own< ::capnp::ClientHook>&& hook)\n"
          "    : ::capnp::Capability::Client(::kj::mv(hook)) {}\n"
          "template <typename T, typename>\n"
          "inline ", fullName, "::Client::Client(::kj::Own<T>&& server)\n"
          "    : ::capnp::Capability::Client(::kj::mv(server)) {}\n"
          "template <typename T, typename>\n"
          "inline ", fullName, "::Client::Client(::kj::Promise<T>&& promise)\n"
          "    : ::capnp::Capability::Client(::kj::mv(promise)) {}\n"
          "inline ", fullName, "::Client::Client(::kj::Exception&& exception)\n"
          "    : ::capnp::Capability::Client(::kj::mv(exception)) {}\n"
          "inline ", fullName, "::Client& ", fullName, "::Client::operator=(Client& other) {\n"
          "  ::capnp::Capability::Client::operator=(other);\n"
          "  return *this;\n"
          "}\n"
          "inline ", fullName, "::Client& ", fullName, "::Client::operator=(Client&& other) {\n"
          "  ::capnp::Capability::Client::operator=(kj::mv(other));\n"
          "  return *this;\n"
          "}\n"
          "\n"),

      kj::strTree(
          KJ_MAP(m, methods) { return kj::mv(m.sourceDefs); },
          "::kj::Promise<void> ", fullName, "::Server::dispatchCall(\n"
          "    uint64_t interfaceId, uint16_t methodId,\n"
          "    ::capnp::CallContext< ::capnp::AnyPointer, ::capnp::AnyPointer> context) {\n"
          "  switch (interfaceId) {\n"
          "    case 0x", kj::hex(proto.getId()), "ull:\n"
          "      return dispatchCallInternal(methodId, context);\n",
          KJ_MAP(e, extends) {
            return kj::strTree(
              "    case 0x", kj::hex(e.id), "ull:\n"
              "      return ", e.typeName, "::Server::dispatchCallInternal(methodId, context);\n");
          },
          "    default:\n"
          "      return internalUnimplemented(\"", proto.getDisplayName(), "\", interfaceId);\n"
          "  }\n"
          "}\n"
          "::kj::Promise<void> ", fullName, "::Server::dispatchCallInternal(\n"
          "    uint16_t methodId,\n"
          "    ::capnp::CallContext< ::capnp::AnyPointer, ::capnp::AnyPointer> context) {\n"
          "  switch (methodId) {\n",
          KJ_MAP(m, methods) { return kj::mv(m.dispatchCase); },
          "    default:\n"
          "      return ::capnp::Capability::Server::internalUnimplemented(\n"
          "          \"", proto.getDisplayName(), "\",\n"
          "          0x", kj::hex(proto.getId()), "ull, methodId);\n"
          "  }\n"
          "}\n")
    };
  }

  // -----------------------------------------------------------------

  struct ConstText {
    bool needsSchema;
    kj::StringTree decl;
    kj::StringTree def;
  };

  ConstText makeConstText(kj::StringPtr scope, kj::StringPtr name, ConstSchema schema) {
    auto proto = schema.getProto();
    auto constProto = proto.getConst();
    auto type = constProto.getType();
    auto typeName_ = typeName(type).flatten();
    auto upperCase = toUpperCase(name);

    // Linkage qualifier for non-primitive types.
    const char* linkage = scope.size() == 0 ? "extern " : "static ";

    switch (type.which()) {
      case schema::Value::VOID:
      case schema::Value::BOOL:
      case schema::Value::INT8:
      case schema::Value::INT16:
      case schema::Value::INT32:
      case schema::Value::INT64:
      case schema::Value::UINT8:
      case schema::Value::UINT16:
      case schema::Value::UINT32:
      case schema::Value::UINT64:
      case schema::Value::FLOAT32:
      case schema::Value::FLOAT64:
      case schema::Value::ENUM:
        return ConstText {
          false,
          kj::strTree("static constexpr ", typeName_, ' ', upperCase, " = ",
              literalValue(constProto.getType(), constProto.getValue()), ";\n"),
          scope.size() == 0 ? kj::strTree() : kj::strTree(
              "constexpr ", typeName_, ' ', scope, upperCase, ";\n")
        };

      case schema::Value::TEXT: {
        kj::String constType = kj::strTree(
            "::capnp::_::ConstText<", schema.as<Text>().size(), ">").flatten();
        return ConstText {
          true,
          kj::strTree(linkage, "const ", constType, ' ', upperCase, ";\n"),
          kj::strTree("const ", constType, ' ', scope, upperCase, "(::capnp::schemas::b_",
                      kj::hex(proto.getId()), ".words + ", schema.getValueSchemaOffset(), ");\n")
        };
      }

      case schema::Value::DATA: {
        kj::String constType = kj::strTree(
            "::capnp::_::ConstData<", schema.as<Data>().size(), ">").flatten();
        return ConstText {
          true,
          kj::strTree(linkage, "const ", constType, ' ', upperCase, ";\n"),
          kj::strTree("const ", constType, ' ', scope, upperCase, "(::capnp::schemas::b_",
                      kj::hex(proto.getId()), ".words + ", schema.getValueSchemaOffset(), ");\n")
        };
      }

      case schema::Value::STRUCT: {
        kj::String constType = kj::strTree(
            "::capnp::_::ConstStruct<", typeName_, ">").flatten();
        return ConstText {
          true,
          kj::strTree(linkage, "const ", constType, ' ', upperCase, ";\n"),
          kj::strTree("const ", constType, ' ', scope, upperCase, "(::capnp::schemas::b_",
                      kj::hex(proto.getId()), ".words + ", schema.getValueSchemaOffset(), ");\n")
        };
      }

      case schema::Value::LIST: {
        kj::String constType = kj::strTree(
            "::capnp::_::ConstList<", typeName(type.getList().getElementType()), ">").flatten();
        return ConstText {
          true,
          kj::strTree(linkage, "const ", constType, ' ', upperCase, ";\n"),
          kj::strTree("const ", constType, ' ', scope, upperCase, "(::capnp::schemas::b_",
                      kj::hex(proto.getId()), ".words + ", schema.getValueSchemaOffset(), ");\n")
        };
      }

      case schema::Value::ANY_POINTER:
      case schema::Value::INTERFACE:
        return ConstText { false, kj::strTree(), kj::strTree() };
    }

    KJ_UNREACHABLE;
  }

  // -----------------------------------------------------------------

  struct NodeText {
    kj::StringTree outerTypeDecl;
    kj::StringTree outerTypeDef;
    kj::StringTree readerBuilderDefs;
    kj::StringTree inlineMethodDefs;
    kj::StringTree capnpSchemaDecls;
    kj::StringTree capnpSchemaDefs;
    kj::StringTree capnpPrivateDecls;
    kj::StringTree capnpPrivateDefs;
    kj::StringTree sourceFileDefs;
  };

  struct NodeTextNoSchema {
    kj::StringTree outerTypeDecl;
    kj::StringTree outerTypeDef;
    kj::StringTree readerBuilderDefs;
    kj::StringTree inlineMethodDefs;
    kj::StringTree capnpPrivateDecls;
    kj::StringTree capnpPrivateDefs;
    kj::StringTree sourceFileDefs;
  };

  NodeText makeNodeText(kj::StringPtr namespace_, kj::StringPtr scope,
                        kj::StringPtr name, Schema schema) {
    auto proto = schema.getProto();
    auto fullName = kj::str(scope, name);
    auto subScope = kj::str(fullName, "::");
    auto hexId = kj::hex(proto.getId());

    // Compute nested nodes, including groups.
    kj::Vector<NodeText> nestedTexts(proto.getNestedNodes().size());
    for (auto nested: proto.getNestedNodes()) {
      nestedTexts.add(makeNodeText(
          namespace_, subScope, nested.getName(), schemaLoader.get(nested.getId())));
    };

    if (proto.isStruct()) {
      for (auto field: proto.getStruct().getFields()) {
        if (field.isGroup()) {
          nestedTexts.add(makeNodeText(
              namespace_, subScope, toTitleCase(field.getName()),
              schemaLoader.get(field.getGroup().getTypeId())));
        }
      }
    } else if (proto.isInterface()) {
      for (auto method: proto.getInterface().getMethods()) {
        {
          Schema params = schemaLoader.get(method.getParamStructType());
          auto paramsProto = schemaLoader.get(method.getParamStructType()).getProto();
          if (paramsProto.getScopeId() == 0) {
            nestedTexts.add(makeNodeText(namespace_, subScope,
                toTitleCase(kj::str(method.getName(), "Params")), params));
          }
        }
        {
          Schema results = schemaLoader.get(method.getResultStructType());
          auto resultsProto = schemaLoader.get(method.getResultStructType()).getProto();
          if (resultsProto.getScopeId() == 0) {
            nestedTexts.add(makeNodeText(namespace_, subScope,
                toTitleCase(kj::str(method.getName(), "Results")), results));
          }
        }
      }
    }

    // Convert the encoded schema to a literal byte array.
    kj::ArrayPtr<const word> rawSchema = schema.asUncheckedMessage();
    auto schemaLiteral = kj::StringTree(KJ_MAP(w, rawSchema) {
      const byte* bytes = reinterpret_cast<const byte*>(&w);

      return kj::strTree(KJ_MAP(i, kj::range<uint>(0, sizeof(word))) {
        auto text = kj::toCharSequence(kj::implicitCast<uint>(bytes[i]));
        return kj::strTree(kj::repeat(' ', 4 - text.size()), text, ",");
      });
    }, "\n   ");

    auto schemaDecl = kj::strTree(
        "extern const ::capnp::_::RawSchema s_", hexId, ";\n");

    std::set<uint64_t> deps;
    enumerateDeps(proto, deps);

    kj::Array<uint> membersByName;
    kj::Array<uint> membersByDiscrim;
    switch (proto.which()) {
      case schema::Node::STRUCT: {
        auto structSchema = schema.asStruct();
        membersByName = makeMembersByName(structSchema.getFields());
        auto builder = kj::heapArrayBuilder<uint>(structSchema.getFields().size());
        for (auto field: structSchema.getUnionFields()) {
          builder.add(field.getIndex());
        }
        for (auto field: structSchema.getNonUnionFields()) {
          builder.add(field.getIndex());
        }
        membersByDiscrim = builder.finish();
        break;
      }
      case schema::Node::ENUM:
        membersByName = makeMembersByName(schema.asEnum().getEnumerants());
        break;
      case schema::Node::INTERFACE:
        membersByName = makeMembersByName(schema.asInterface().getMethods());
        break;
      default:
        break;
    }

    auto schemaDef = kj::strTree(
        "static const ::capnp::_::AlignedData<", rawSchema.size(), "> b_", hexId, " = {\n"
        "  {", kj::mv(schemaLiteral), " }\n"
        "};\n",
        deps.size() == 0 ? kj::strTree() : kj::strTree(
            "static const ::capnp::_::RawSchema* const d_", hexId, "[] = {\n",
            KJ_MAP(depId, deps) {
              return kj::strTree("  &s_", kj::hex(depId), ",\n");
            },
            "};\n"),
        membersByName.size() == 0 ? kj::strTree() : kj::strTree(
            "static const uint16_t m_", hexId, "[] = {",
            kj::StringTree(KJ_MAP(index, membersByName) { return kj::strTree(index); }, ", "),
            "};\n"),
        membersByDiscrim.size() == 0 ? kj::strTree() : kj::strTree(
            "static const uint16_t i_", hexId, "[] = {",
            kj::StringTree(KJ_MAP(index, membersByDiscrim) { return kj::strTree(index); }, ", "),
            "};\n"),
        "const ::capnp::_::RawSchema s_", hexId, " = {\n"
        "  0x", hexId, ", b_", hexId, ".words, ", rawSchema.size(), ", ",
        deps.size() == 0 ? kj::strTree("nullptr") : kj::strTree("d_", hexId), ", ",
        membersByName.size() == 0 ? kj::strTree("nullptr") : kj::strTree("m_", hexId), ",\n",
        "  ", deps.size(), ", ", membersByName.size(), ", ",
        membersByDiscrim.size() == 0 ? kj::strTree("nullptr") : kj::strTree("i_", hexId),
        ", nullptr, nullptr\n"
        "};\n");

    NodeTextNoSchema top = makeNodeTextWithoutNested(
        namespace_, scope, name, schema,
        KJ_MAP(n, nestedTexts) { return kj::mv(n.outerTypeDecl); });

    return NodeText {
      kj::mv(top.outerTypeDecl),

      kj::strTree(
          kj::mv(top.outerTypeDef),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.outerTypeDef); }),

      kj::strTree(
          kj::mv(top.readerBuilderDefs),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.readerBuilderDefs); }),

      kj::strTree(
          kj::mv(top.inlineMethodDefs),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.inlineMethodDefs); }),

      kj::strTree(
          kj::mv(schemaDecl),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.capnpSchemaDecls); }),

      kj::strTree(
          kj::mv(schemaDef),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.capnpSchemaDefs); }),

      kj::strTree(
          kj::mv(top.capnpPrivateDecls),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.capnpPrivateDecls); }),

      kj::strTree(
          kj::mv(top.capnpPrivateDefs),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.capnpPrivateDefs); }),

      kj::strTree(
          kj::mv(top.sourceFileDefs),
          KJ_MAP(n, nestedTexts) { return kj::mv(n.sourceFileDefs); }),
    };
  }

  NodeTextNoSchema makeNodeTextWithoutNested(kj::StringPtr namespace_, kj::StringPtr scope,
                                             kj::StringPtr name, Schema schema,
                                             kj::Array<kj::StringTree> nestedTypeDecls) {
    auto proto = schema.getProto();
    auto fullName = kj::str(scope, name);
    auto hexId = kj::hex(proto.getId());

    switch (proto.which()) {
      case schema::Node::FILE:
        KJ_FAIL_REQUIRE("This method shouldn't be called on file nodes.");

      case schema::Node::STRUCT: {
        StructText structText =
            makeStructText(scope, name, schema.asStruct(), kj::mv(nestedTypeDecls));
        auto structNode = proto.getStruct();

        return NodeTextNoSchema {
          kj::mv(structText.outerTypeDecl),
          kj::mv(structText.outerTypeDef),
          kj::mv(structText.readerBuilderDefs),
          kj::strTree(),

          kj::strTree(
              "CAPNP_DECLARE_STRUCT(\n"
              "    ", namespace_, "::", fullName, ", ", hexId, ",\n"
              "    ", structNode.getDataWordCount(), ", ",
                      structNode.getPointerCount(), ", ",
                      FIELD_SIZE_NAMES[static_cast<uint>(structNode.getPreferredListEncoding())],
                      ");\n"),
          kj::strTree(
              "CAPNP_DEFINE_STRUCT(\n"
              "    ", namespace_, "::", fullName, ");\n"),

          kj::strTree(),
        };
      }

      case schema::Node::ENUM: {
        auto enumerants = schema.asEnum().getEnumerants();

        return NodeTextNoSchema {
          scope.size() == 0 ? kj::strTree() : kj::strTree(
              "  enum class ", name, ": uint16_t {\n",
              KJ_MAP(e, enumerants) {
                return kj::strTree("    ", toUpperCase(e.getProto().getName()), ",\n");
              },
              "  };\n"
              "\n"),

          scope.size() > 0 ? kj::strTree() : kj::strTree(
              "enum class ", name, ": uint16_t {\n",
              KJ_MAP(e, enumerants) {
                return kj::strTree("  ", toUpperCase(e.getProto().getName()), ",\n");
              },
              "};\n"
              "\n"),

          kj::strTree(),
          kj::strTree(),

          kj::strTree(
              "CAPNP_DECLARE_ENUM(\n"
              "    ", namespace_, "::", fullName, ", ", hexId, ");\n"),
          kj::strTree(
              "CAPNP_DEFINE_ENUM(\n"
              "    ", namespace_, "::", fullName, ");\n"),

          kj::strTree(),
        };
      }

      case schema::Node::INTERFACE: {
        hasInterfaces = true;

        InterfaceText interfaceText =
            makeInterfaceText(scope, name, schema.asInterface(), kj::mv(nestedTypeDecls));

        return NodeTextNoSchema {
          kj::mv(interfaceText.outerTypeDecl),
          kj::mv(interfaceText.outerTypeDef),
          kj::mv(interfaceText.clientServerDefs),
          kj::mv(interfaceText.inlineMethodDefs),

          kj::strTree(
              "CAPNP_DECLARE_INTERFACE(\n"
              "    ", namespace_, "::", fullName, ", ", hexId, ");\n"),
          kj::strTree(
              "CAPNP_DEFINE_INTERFACE(\n"
              "    ", namespace_, "::", fullName, ");\n"),

          kj::mv(interfaceText.sourceDefs),
        };
      }

      case schema::Node::CONST: {
        auto constText = makeConstText(scope, name, schema.asConst());

        return NodeTextNoSchema {
          scope.size() == 0 ? kj::strTree() : kj::strTree("  ", kj::mv(constText.decl)),
          scope.size() > 0 ? kj::strTree() : kj::mv(constText.decl),
          kj::strTree(),
          kj::strTree(),

          kj::strTree(),
          kj::strTree(),

          kj::mv(constText.def),
        };
      }

      case schema::Node::ANNOTATION: {
        return NodeTextNoSchema {
          kj::strTree(),
          kj::strTree(),
          kj::strTree(),
          kj::strTree(),

          kj::strTree(),
          kj::strTree(),

          kj::strTree(),
        };
      }
    }

    KJ_UNREACHABLE;
  }

  // -----------------------------------------------------------------

  struct FileText {
    kj::StringTree header;
    kj::StringTree source;
  };

  FileText makeFileText(Schema schema,
                        schema::CodeGeneratorRequest::RequestedFile::Reader request) {
    usedImports.clear();

    auto node = schema.getProto();
    auto displayName = node.getDisplayName();

    kj::Vector<kj::ArrayPtr<const char>> namespaceParts;
    kj::String namespacePrefix;

    for (auto annotation: node.getAnnotations()) {
      if (annotation.getId() == NAMESPACE_ANNOTATION_ID) {
        kj::StringPtr ns = annotation.getValue().getText();
        kj::StringPtr ns2 = ns;
        namespacePrefix = kj::str("::", ns);

        for (;;) {
          KJ_IF_MAYBE(colonPos, ns.findFirst(':')) {
            namespaceParts.add(ns.slice(0, *colonPos));
            ns = ns.slice(*colonPos);
            if (!ns.startsWith("::")) {
              context.exitError(kj::str(displayName, ": invalid namespace spec: ", ns2));
            }
            ns = ns.slice(2);
          } else {
            namespaceParts.add(ns);
            break;
          }
        }

        break;
      }
    }

    auto nodeTexts = KJ_MAP(nested, node.getNestedNodes()) {
      return makeNodeText(namespacePrefix, "", nested.getName(), schemaLoader.get(nested.getId()));
    };

    kj::String separator = kj::str("// ", kj::repeat('=', 87), "\n");

    kj::Vector<kj::StringPtr> includes;
    for (auto import: request.getImports()) {
      if (usedImports.count(import.getId()) > 0) {
        includes.add(import.getName());
      }
    }

    kj::StringTree sourceDefs = kj::strTree(
        KJ_MAP(n, nodeTexts) { return kj::mv(n.sourceFileDefs); });

    return FileText {
      kj::strTree(
          "// Generated by Cap'n Proto compiler, DO NOT EDIT\n"
          "// source: ", baseName(displayName), "\n"
          "\n"
          "#ifndef CAPNP_INCLUDED_", kj::hex(node.getId()), "_\n",
          "#define CAPNP_INCLUDED_", kj::hex(node.getId()), "_\n"
          "\n"
          "#include <capnp/altc++/property.h>\n",
          hasInterfaces ? kj::strTree("#include <capnp/capability.h>\n") : kj::strTree(),
          "\n"
          "#if CAPNP_VERSION != ", CAPNP_VERSION, "\n"
          "#error \"Version mismatch between generated code and library headers.  You must "
              "use the same version of the Cap'n Proto compiler and library.\"\n"
          "#endif\n"
          "\n",
          KJ_MAP(path, includes) {
            if (path.startsWith("/")) {
              return kj::strTree("#include <", path.slice(1), ".h>\n");
            } else {
              return kj::strTree("#include \"", path, ".h\"\n");
            }
          },
          "\n",

          KJ_MAP(n, namespaceParts) { return kj::strTree("namespace ", n, " {\n"); }, "\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.outerTypeDef); },
          KJ_MAP(n, namespaceParts) { return kj::strTree("}  // namespace\n"); }, "\n",

          separator, "\n"
          "namespace capnp {\n"
          "namespace schemas {\n"
          "\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.capnpSchemaDecls); },
          "\n"
          "}  // namespace schemas\n"
          "namespace _ {  // private\n"
          "\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.capnpPrivateDecls); },
          "\n"
          "}  // namespace _ (private)\n"
          "}  // namespace capnp\n"

          "\n", separator, "\n",
          KJ_MAP(n, namespaceParts) { return kj::strTree("namespace ", n, " {\n"); }, "\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.readerBuilderDefs); },
          separator, "\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.inlineMethodDefs); },
          KJ_MAP(n, namespaceParts) { return kj::strTree("}  // namespace\n"); }, "\n",
          "#endif  // CAPNP_INCLUDED_", kj::hex(node.getId()), "_\n"),

      kj::strTree(
          "// Generated by Cap'n Proto compiler, DO NOT EDIT\n"
          "// source: ", baseName(displayName), "\n"
          "\n"
          "#include \"", baseName(displayName), ".h\"\n"
          "\n"
          "namespace capnp {\n"
          "namespace schemas {\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.capnpSchemaDefs); },
          "}  // namespace schemas\n"
          "namespace _ {  // private\n",
          KJ_MAP(n, nodeTexts) { return kj::mv(n.capnpPrivateDefs); },
          "}  // namespace _ (private)\n"
          "}  // namespace capnp\n",
          sourceDefs.size() == 0 ? kj::strTree() : kj::strTree(
              "\n", separator, "\n",
              KJ_MAP(n, namespaceParts) { return kj::strTree("namespace ", n, " {\n"); }, "\n",
              kj::mv(sourceDefs), "\n",
              KJ_MAP(n, namespaceParts) { return kj::strTree("}  // namespace\n"); }, "\n"))
    };
  }

  // -----------------------------------------------------------------

  void makeDirectory(kj::StringPtr path) {
    KJ_IF_MAYBE(slashpos, path.findLast('/')) {
      // Make the parent dir.
      makeDirectory(kj::str(path.slice(0, *slashpos)));
    }

    if (mkdir(path.cStr(), 0777) < 0) {
      int error = errno;
      if (error != EEXIST) {
        KJ_FAIL_SYSCALL("mkdir(path)", error, path);
      }
    }
  }

  void writeFile(kj::StringPtr filename, const kj::StringTree& text) {
    if (!filename.startsWith("/")) {
      KJ_IF_MAYBE(slashpos, filename.findLast('/')) {
        // Make the parent dir.
        makeDirectory(kj::str(filename.slice(0, *slashpos)));
      }
    }

    int fd;
    KJ_SYSCALL(fd = open(filename.cStr(), O_CREAT | O_WRONLY | O_TRUNC, 0666), filename);
    kj::FdOutputStream out((kj::AutoCloseFd(fd)));

    text.visit(
        [&](kj::ArrayPtr<const char> text) {
          out.write(text.begin(), text.size());
        });
  }

  kj::MainBuilder::Validity run() {
    ReaderOptions options;
    options.traversalLimitInWords = 1 << 30;  // Don't limit.
    StreamFdMessageReader reader(STDIN_FILENO, options);
    auto request = reader.getRoot<schema::CodeGeneratorRequest>();

    for (auto node: request.getNodes()) {
      schemaLoader.load(node);
    }

    kj::FdOutputStream rawOut(STDOUT_FILENO);
    kj::BufferedOutputStreamWrapper out(rawOut);

    for (auto requestedFile: request.getRequestedFiles()) {
      auto schema = schemaLoader.get(requestedFile.getId());
      auto fileText = makeFileText(schema, requestedFile);

      writeFile(kj::str(schema.getProto().getDisplayName(), ".h"), fileText.header);
      writeFile(kj::str(schema.getProto().getDisplayName(), ".c++"), fileText.source);
    }

    return true;
  }
};

}  // namespace
} // namespace altcxx
}  // namespace capnp

KJ_MAIN(capnp::altcxx::CapnpcAltCxxMain)
