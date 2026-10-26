#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `SignBlockEntity`.
namespace signs {

inline constexpr std::int32_t kLines = 4;

inline constexpr std::size_t kMaxLineLength = 384;

[[nodiscard]] bool isSign(std::int32_t stateId);

[[nodiscard]] std::optional<wire::common::Identifier> typeOf(std::int32_t stateId);

[[nodiscard]] wire::nbt::NbtCompound emptyData();

[[nodiscard]] wire::nbt::NbtCompound withText(const wire::nbt::NbtCompound& data, bool isFront,
                                              const std::vector<std::string>& lines);

/// The same with lines that may carry `&` colour codes, which turn them into styled text.
[[nodiscard]] wire::nbt::NbtCompound withColouredText(const wire::nbt::NbtCompound& data, bool isFront,
                                                      const std::vector<std::string>& lines);

/// What is stored is four json components; anything outside the protocol wants the words.
[[nodiscard]] std::string plainText(const wire::nbt::NbtCompound& data, bool front);

/// Whether this sign refuses to be edited. parity: `SignBlockEntity.isWaxed`.
[[nodiscard]] bool isWaxed(const wire::nbt::NbtCompound& data);

/// parity: `SignBlockEntity.applyImplicitComponents`.
[[nodiscard]] wire::nbt::NbtCompound withComponents(const wire::nbt::NbtCompound& data,
                                                    const wire::nbt::NbtCompound& components);

/// parity: `SignText.hasEditableText`.
[[nodiscard]] bool hasEditableText(const wire::nbt::NbtCompound& data, bool front);

/// parity: `SignBlockEntity.getSlotPlayerIsFacing`.
[[nodiscard]] bool facesFront(std::int32_t stateId, std::int32_t x, std::int32_t z, double playerX, double playerZ);

[[nodiscard]] server::world::ChunkBlockEntity blockEntity(std::int32_t localX, std::int32_t y, std::int32_t localZ,
                                                          const wire::common::Identifier& type,
                                                          wire::nbt::NbtCompound data = emptyData());

} // namespace signs

} // namespace basalt::server::runtime::world
