#include <basalt/server/runtime/command/CommandStorage.hpp>
#include <basalt/server/runtime/command/DataPaths.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <string>
#include <vector>

namespace basalt::server::runtime::command {
namespace {

using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

[[nodiscard]] NbtTag positionOf(double x, double y, double z) {
    return NbtTag::ofList(NbtList{NbtType::Double, {NbtTag::ofDouble(x), NbtTag::ofDouble(y), NbtTag::ofDouble(z)}});
}

[[nodiscard]] NbtTag rotationOf(float yaw, float pitch) {
    return NbtTag::ofList(NbtList{NbtType::Float, {NbtTag::ofFloat(yaw), NbtTag::ofFloat(pitch)}});
}

template <typename Range>
[[nodiscard]] NbtTag tagsOf(const Range& tags) {
    std::vector<NbtTag> elements;
    for (const std::string& tag : tags) {
        elements.push_back(NbtTag::ofString(tag));
    }
    return NbtTag::ofList(NbtList{NbtType::String, std::move(elements)});
}

/// The block entity at a position, or null when its chunk is not loaded or it holds none.
[[nodiscard]] server::world::ChunkBlockEntity* blockEntityAt(const CommandContext& context,
                                                             const std::array<std::int32_t, 3>& pos) {
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return nullptr;
    }
    server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(pos[0] >> 4, pos[2] >> 4);
    return chunk == nullptr ? nullptr : chunk->blockEntityAt(pos[0] & 15, pos[1], pos[2] & 15);
}

[[nodiscard]] NbtTag numberTag(double value, std::string_view type) {
    if (type == "byte") {
        return NbtTag::ofByte(static_cast<std::int8_t>(static_cast<std::int32_t>(value)));
    }
    if (type == "short") {
        return NbtTag::ofShort(static_cast<std::int16_t>(static_cast<std::int32_t>(value)));
    }
    if (type == "long") {
        return NbtTag::ofLong(static_cast<std::int64_t>(value));
    }
    if (type == "float") {
        return NbtTag::ofFloat(static_cast<float>(value));
    }
    if (type == "double") {
        return NbtTag::ofDouble(value);
    }
    return NbtTag::ofInt(static_cast<std::int32_t>(value));
}

} // namespace

NbtCompound entityNbtOf(const player::PlayerSession& player) {
    NbtCompound tag;
    tag.putString("id", "minecraft:player");
    tag.putString("Name", player.name());
    tag.putFloat("Health", player.health());
    tag.putInt("XpLevel", player.experience.level());
    tag.putFloat("XpP", player.experience.progress());
    tag.putInt("XpTotal", player.experience.total());
    tag.putString("Dimension", player.worldName);
    tag.set("Pos", positionOf(player.x, player.y, player.z));
    tag.set("Rotation", rotationOf(player.yaw, player.pitch));
    tag.set("Tags", tagsOf(player.tags));
    tag.putInt("foodLevel", player.food.foodLevel());
    return tag;
}

NbtCompound entityNbtOf(const entity::ServerEntity& entity) {
    NbtCompound tag;
    tag.putString("id", entity.type().toString());
    if (entity.customName.has_value()) {
        tag.putString("CustomName", *entity.customName);
    }
    tag.putFloat("Health", static_cast<float>(entity.health));
    tag.set("Pos", positionOf(entity.x, entity.y, entity.z));
    tag.set("Rotation", rotationOf(entity.yaw, entity.pitch));
    tag.set("Tags", tagsOf(entity.tags));
    return tag;
}

NbtCompound& CommandStorage::get(const Identifier& id) { return compounds_[id]; }

void CommandStorage::set(const Identifier& id, NbtCompound value) { compounds_.insert_or_assign(id, std::move(value)); }

std::set<Identifier> CommandStorage::ids() const {
    std::set<Identifier> named;
    for (const auto& [id, value] : compounds_) {
        named.insert(id);
    }
    return named;
}

void CommandStorage::load() {
    const std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path_);
    if (!bytes.has_value()) {
        return;
    }
    const std::expected<wire::nbt::NamedTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::read(*bytes);
    const auto* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    if (root == nullptr) {
        return;
    }
    compounds_.clear();
    for (const std::string& key : root->names()) {
        const NbtTag* value = root->find(key);
        const auto* compound = value == nullptr ? nullptr : value->get<NbtCompound>();
        const std::expected<Identifier, wire::common::IdentifierError> id = Identifier::parse(key);
        if (compound != nullptr && id.has_value()) {
            compounds_.insert_or_assign(*id, *compound);
        }
    }
}

void CommandStorage::save() const {
    if (compounds_.empty()) {
        return;
    }
    NbtCompound root;
    for (const auto& [id, value] : compounds_) {
        root.set(id.toString(), NbtTag::ofCompound(value));
    }
    // The directory may not exist yet: the server writes this beside `worlds/`, which is created on
    // demand, and a test points it inside `build/`.
    if (const std::size_t slash = path_.rfind('/'); slash != std::string::npos && slash > 0) {
        static_cast<void>(wire::io::fileIO::createDirectories(path_.substr(0, slash)));
    }
    static_cast<void>(wire::io::fileIO::writeBytes(path_, wire::nbt::nbtIO::write(NbtTag::ofCompound(root))));
}

std::optional<NbtCompound> DataAccess::read(const CommandContext& context, const DataHolderRef& holder) const {
    if (const auto* named = holder.as<DataHolderRef::Storage>(); named != nullptr) {
        return storage_->get(named->id);
    }
    if (const auto* block = holder.as<DataHolderRef::Block>(); block != nullptr) {
        const server::world::ChunkBlockEntity* entity = blockEntityAt(context, block->pos);
        return entity == nullptr ? std::nullopt : std::optional{entity->data()};
    }
    const auto* entity = holder.as<DataHolderRef::Entity>();
    return entity == nullptr || entity->player == nullptr ? std::nullopt : std::optional{entityNbtOf(*entity->player)};
}

bool DataAccess::pathExists(const CommandContext& context, const DataHolderRef& holder, std::string_view path) const {
    const std::optional<NbtCompound> root = read(context, holder);
    return root.has_value() && dataPaths::resolve(NbtTag::ofCompound(*root), path).has_value();
}

std::optional<NbtCompound> DataAccess::readCompound(const CommandContext& context, const DataHolderRef& holder,
                                                    const std::string* path) const {
    const std::optional<NbtCompound> root = read(context, holder);
    if (!root.has_value()) {
        return std::nullopt;
    }
    if (path == nullptr) {
        return root;
    }
    const std::optional<NbtTag> found = dataPaths::resolve(NbtTag::ofCompound(*root), *path);
    const auto* compound = found.has_value() ? found->get<NbtCompound>() : nullptr;
    return compound == nullptr ? std::nullopt : std::optional{*compound};
}

void DataAccess::store(const Identifier& id, std::string_view path, double value, std::string_view type) const {
    const std::optional<NbtCompound> written = dataPaths::set(storage_->get(id), path, numberTag(value, type));
    if (written.has_value()) {
        storage_->set(id, *written);
    }
}

void DataAccess::storeInBlock(const CommandContext& context, const std::array<std::int32_t, 3>& pos,
                              std::string_view path, double value, std::string_view type) const {
    server::world::ChunkBlockEntity* entity = blockEntityAt(context, pos);
    if (entity == nullptr) {
        return;
    }
    const std::optional<NbtCompound> written = dataPaths::set(entity->data(), path, numberTag(value, type));
    if (!written.has_value()) {
        return;
    }
    const Parsed<world::LoadedWorld*> world = context.world();
    if (!world.has_value()) {
        return;
    }
    server::world::Chunk* chunk = (*world)->state().chunkIfLoaded(pos[0] >> 4, pos[2] >> 4);
    if (chunk != nullptr) {
        chunk->setBlockEntity(
            server::world::ChunkBlockEntity{entity->localX(), entity->y(), entity->localZ(), entity->type(), *written});
    }
}

} // namespace basalt::server::runtime::command
