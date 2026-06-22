#include <basalt/server/runtime/StructureBlockSystem.hpp>
#include <basalt/server/runtime/plugins/ServerStructures.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/StructureBlocks.hpp>
#include <basalt/server/runtime/world/StructureTemplates.hpp>
#include <basalt/server/world/structure/StructureTemplate.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>

namespace basalt::server::runtime::plugins {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;

[[nodiscard]] std::optional<Identifier> parsed(std::string_view id) {
    std::expected<Identifier, wire::common::IdentifierError> result = Identifier::parse(id);
    return result.has_value() ? std::optional{std::move(*result)} : std::nullopt;
}

[[nodiscard]] std::optional<NbtCompound> compoundOf(std::span<const std::uint8_t> bytes) {
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(bytes);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional{*root};
}

[[nodiscard]] std::vector<std::uint8_t> bytesOf(NbtCompound compound) {
    return wire::nbt::nbtIO::writeAnonymous(wire::nbt::NbtTag::ofCompound(std::move(compound)));
}

} // namespace

std::vector<std::string> ServerStructures::ids() {
    std::vector<std::string> out;
    if (hooks_.templates != nullptr) {
        for (const Identifier& id : hooks_.templates->allIds()) {
            out.push_back(id.toString());
        }
    }
    return out;
}

std::optional<std::vector<std::uint8_t>> ServerStructures::read(std::string_view id) {
    const std::optional<Identifier> parsedId = parsed(id);
    if (!parsedId.has_value() || hooks_.templates == nullptr) {
        return std::nullopt;
    }
    std::optional<NbtCompound> found = hooks_.templates->read(*parsedId);
    return found.has_value() ? std::optional{bytesOf(std::move(*found))} : std::nullopt;
}

bool ServerStructures::store(std::string_view id, std::span<const std::uint8_t> structure, bool toDisk) {
    const std::optional<Identifier> parsedId = parsed(id);
    const std::optional<NbtCompound> compound = compoundOf(structure);
    return parsedId.has_value() && compound.has_value() && hooks_.templates != nullptr &&
           hooks_.templates->store(*parsedId, *compound, toDisk);
}

bool ServerStructures::remove(std::string_view id, bool fromDisk) {
    const std::optional<Identifier> parsedId = parsed(id);
    return parsedId.has_value() && hooks_.templates != nullptr && hooks_.templates->erase(*parsedId, fromDisk);
}

std::string ServerStructures::file(std::string_view id) {
    const std::optional<Identifier> parsedId = parsed(id);
    return parsedId.has_value() && hooks_.templates != nullptr ? hooks_.templates->generatedFile(*parsedId).string()
                                                               : std::string{};
}

std::optional<std::vector<std::uint8_t>> ServerStructures::capture(std::string_view world, std::int32_t x,
                                                                   std::int32_t y, std::int32_t z, std::int32_t sizeX,
                                                                   std::int32_t sizeY, std::int32_t sizeZ,
                                                                   bool entities) {
    world::LoadedWorld* found = hooks_.worldFor ? hooks_.worldFor(std::string{world}) : nullptr;
    if (found == nullptr || sizeX <= 0 || sizeY <= 0 || sizeZ <= 0) {
        return std::nullopt;
    }
    return bytesOf(world::structureBlocks::capture(found->state(), {x, y, z}, {sizeX, sizeY, sizeZ}, entities));
}

bool ServerStructures::place(std::span<const std::uint8_t> structure,
                             const server::plugins::PluginStructurePlacement& placement) {
    world::LoadedWorld* found = hooks_.worldFor ? hooks_.worldFor(placement.world) : nullptr;
    StructureBlockSystem* blocks = hooks_.blocks ? hooks_.blocks() : nullptr;
    const std::optional<NbtCompound> compound = compoundOf(structure);
    if (found == nullptr || blocks == nullptr || !compound.has_value()) {
        return false;
    }
    server::world::structure::StructureTemplate loaded;
    loaded.load(*compound);
    std::optional<std::size_t> palette;
    if (placement.palette >= 0) {
        palette = std::min<std::size_t>(static_cast<std::size_t>(placement.palette),
                                        std::max<std::size_t>(loaded.paletteCount(), 1) - 1);
    }
    blocks->placeTemplate(*found, loaded, {placement.x, placement.y, placement.z},
                          StructureBlockSystem::Placement{.rotation = placement.rotation & 3,
                                                          .mirror = placement.mirror,
                                                          .integrity = placement.integrity,
                                                          .seed = placement.seed,
                                                          .ignoreEntities = !placement.entities,
                                                          .palette = palette});
    return true;
}

} // namespace basalt::server::runtime::plugins
