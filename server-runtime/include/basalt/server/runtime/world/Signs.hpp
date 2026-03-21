#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// What a sign holds and how it is written down.
///
/// parity: `SignBlockEntity` over `SignText`: two sides, four lines each, a dye colour and whether
/// the text glows. A line is a text component, and a plain one is written as a bare NBT string,
/// which is the short form the client reads.
namespace signs {

inline constexpr std::int32_t kLines = 4;

/// parity: `SignBlockEntity.MAX_TEXT_LINE_LENGTH`, which is what the server enforces.
inline constexpr std::size_t kMaxLineLength = 384;

[[nodiscard]] bool isSign(std::int32_t stateId);

/// The block entity type a sign of this block is: the hanging ones are their own.
[[nodiscard]] std::optional<wire::common::Identifier> typeOf(std::int32_t stateId);

/// An empty sign, which is what a freshly placed one holds until someone writes on it.
[[nodiscard]] wire::nbt::NbtCompound emptyData();

/// The same with one side replaced, each line trimmed to what a sign accepts.
[[nodiscard]] wire::nbt::NbtCompound withText(const wire::nbt::NbtCompound& data, bool isFront,
                                              const std::vector<std::string>& lines);

/// One face as plain lines run together, newline between them. What is stored is four components
/// in json; what anything outside the protocol wants is the words.
[[nodiscard]] std::string plainText(const wire::nbt::NbtCompound& data, bool front);

/// Whether this sign refuses to be edited. parity: `SignBlockEntity.isWaxed`.
[[nodiscard]] bool isWaxed(const wire::nbt::NbtCompound& data);

/// parity: `SignBlockEntity.applyImplicitComponents`: 26.3's `sign_text_front`, `sign_text_back`
/// and `waxed` components of the item that placed it.
[[nodiscard]] wire::nbt::NbtCompound withComponents(const wire::nbt::NbtCompound& data,
                                                    const wire::nbt::NbtCompound& components);

/// parity: `SignText.hasEditableText`: every line empty or plain text, nothing a player could not type.
[[nodiscard]] bool hasEditableText(const wire::nbt::NbtCompound& data, bool front);

/// parity: `SignBlockEntity.getSlotPlayerIsFacing`: whether the player looks at the front, from
/// where they stand against the sign's hitbox centre.
[[nodiscard]] bool facesFront(std::int32_t stateId, std::int32_t x, std::int32_t z, double playerX, double playerZ);

[[nodiscard]] server::world::ChunkBlockEntity blockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ,
                                                          const wire::common::Identifier& type,
                                                          wire::nbt::NbtCompound data = emptyData());

} // namespace signs

} // namespace basalt::server::runtime::world
