#include <basalt/server/registry/BlockStateDefinitions.hpp>
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

[[nodiscard]] bool portable(std::string_view text, bool allowSlash) {
    const auto valid = [allowSlash](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' ||
               (allowSlash && c == '/');
    };
    return !text.empty() && std::ranges::all_of(text, valid) && text.find("..") == std::string_view::npos &&
           text.front() != '/' && text.find("//") == std::string_view::npos;
}

[[nodiscard]] std::optional<Identifier> parsed(std::string_view id) {
    std::expected<Identifier, wire::common::IdentifierError> result = Identifier::parse(id);
    if (!result.has_value() || !portable(result->namespaceName(), false) || !portable(result->path(), true)) {
        return std::nullopt;
    }
    return std::optional{std::move(*result)};
}

[[nodiscard]] std::optional<NbtCompound> compoundOf(std::span<const std::uint8_t> bytes) {
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(bytes);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    return root == nullptr ? std::nullopt : std::optional{*root};
}

[[nodiscard]] std::vector<std::uint8_t> bytesOf(NbtCompound compound) {
    return wire::nbt::nbtIO::writeAnonymous(wire::nbt::NbtTag::ofCompound(std::move(compound)));
}

[[nodiscard]] std::string_view blockPath(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] bool isBlock(std::int32_t stateId, std::string_view wanted) {
    return wanted.empty() || blockPath(stateId) == (wanted.starts_with("minecraft:") ? wanted.substr(10) : wanted);
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

std::optional<server::plugins::PluginTemplateScan>
ServerStructures::scan(std::span<const std::uint8_t> structure,
                       const server::plugins::PluginTemplateScanRequest& request) {
    namespace tmpl = server::world::structure;
    const std::optional<NbtCompound> compound = compoundOf(structure);
    if (!compound.has_value()) {
        return std::nullopt;
    }
    tmpl::StructureTemplate loaded;
    loaded.load(*compound);
    const auto rotation = static_cast<tmpl::StructureRotation>(request.rotation & 3);
    const auto mirror = static_cast<tmpl::StructureMirror>(std::clamp(request.mirror, 0, 2));
    const tmpl::StructurePos pivot{request.pivotX, 0, request.pivotZ};
    const tmpl::StructurePos origin{request.x, request.y, request.z};
    server::plugins::PluginTemplateScan out;
    out.sizeX = loaded.size().x;
    out.sizeY = loaded.size().y;
    out.sizeZ = loaded.size().z;
    for (const tmpl::StructureBlockInfo& info : loaded.blocks()) {
        if (!isBlock(info.stateId, request.block)) {
            continue;
        }
        tmpl::StructurePos at = info.pos;
        if (request.absolute) {
            at = tmpl::transform(info.pos, mirror, rotation, pivot);
            at = {at.x + origin.x, at.y + origin.y, at.z + origin.z};
        }
        server::plugins::PluginTemplateBlock block{.x = at.x, .y = at.y, .z = at.z, .stateId = info.stateId};
        if (info.blockEntityNbt.has_value()) {
            block.nbt = bytesOf(*info.blockEntityNbt);
        }
        out.blocks.push_back(std::move(block));
    }
    for (const tmpl::PlacedJigsaw& jigsaw : loaded.jigsawBlocks(origin, rotation, mirror, pivot)) {
        out.jigsaws.push_back({.x = jigsaw.pos.x,
                               .y = jigsaw.pos.y,
                               .z = jigsaw.pos.z,
                               .front = std::string{tmpl::serializedName(jigsaw.front)},
                               .top = std::string{tmpl::serializedName(jigsaw.top)},
                               .rollable = jigsaw.rollable,
                               .name = jigsaw.name,
                               .pool = jigsaw.pool,
                               .target = jigsaw.target,
                               .finalState = jigsaw.finalStateName,
                               .placementPriority = jigsaw.placementPriority,
                               .selectionPriority = jigsaw.selectionPriority});
    }
    return out;
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
