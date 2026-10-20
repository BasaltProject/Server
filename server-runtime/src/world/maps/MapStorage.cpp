#include <basalt/metamorph/datafix/LevelData.hpp>
#include <basalt/server/runtime/world/maps/MapStorage.hpp>
#include <basalt/server/world/region/ChunkNbt.hpp>
#include <basalt/wire/io/Compression.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <string>
#include <utility>

namespace basalt::server::runtime::world::maps {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

/// parity: `SavedDataStorage`, which wraps the data with the version that wrote it.
[[nodiscard]] std::vector<std::uint8_t> wrapped(NbtCompound data) {
    NbtCompound root;
    root.set("data", NbtTag::ofCompound(std::move(data)));
    root.putInt("DataVersion", server::world::region::chunkNbt::kDataVersion);
    return wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root)));
}

[[nodiscard]] std::optional<NbtCompound> unwrapped(const std::string& path) {
    std::optional<std::vector<std::uint8_t>> bytes = wire::io::fileIO::readBytes(path);
    if (!bytes.has_value()) {
        return std::nullopt;
    }
    // A vanilla save's own `maps/<id>.dat` is gzipped, and one copied in reads the same.
    if (bytes->size() > 2 && (*bytes)[0] == 0x1F && (*bytes)[1] == 0x8B) {
        auto plain = wire::io::gzip::decompress(*bytes);
        if (!plain.has_value()) {
            return std::nullopt;
        }
        bytes = std::move(*plain);
    }
    const auto read = wire::nbt::nbtIO::read(*bytes);
    const NbtCompound* root = read.has_value() ? read->tag.get<NbtCompound>() : nullptr;
    const NbtCompound* held = root == nullptr ? nullptr : root->getCompound("data");
    if (held == nullptr) {
        return std::nullopt;
    }
    NbtCompound data = *held;
    metamorph::datafix::upgradeSavedData("map", data, root->getInt("DataVersion"));
    return data;
}

} // namespace

std::string MapStorage::pathOf(std::string_view name) const { return directory_ + "/" + std::string{name} + ".dat"; }

std::optional<NbtCompound> MapStorage::read(std::string_view name) const {
    if (std::optional<NbtCompound> found = unwrapped(pathOf(name)); found.has_value()) {
        return found;
    }
    if (!fallback_.empty()) {
        if (std::optional<NbtCompound> found = unwrapped(fallback_ + "/" + std::string{name} + ".dat");
            found.has_value()) {
            return found;
        }
    }
    return legacy_.empty() ? std::nullopt : unwrapped(legacy_ + "/map_" + std::string{name} + ".dat");
}

MapData* MapStorage::get(std::int32_t id) {
    auto found = maps_.find(id);
    if (found == maps_.end()) {
        const std::optional<NbtCompound> data = read(std::to_string(id));
        found = maps_.emplace(id, data.has_value() ? MapData::load(*data) : std::nullopt).first;
    }
    return found->second.has_value() ? &*found->second : nullptr;
}

void MapStorage::set(std::int32_t id, MapData data) { maps_.insert_or_assign(id, std::move(data)); }

std::int32_t MapStorage::nextId() {
    if (!lastId_.has_value()) {
        const std::optional<NbtCompound> index = read("last_id");
        lastId_ = index.has_value() ? index->getInt("map", -1) : -1;
    }
    indexDirty_ = true;
    return ++*lastId_;
}

bool MapStorage::claimed(const std::string& key) {
    if (!claims_.has_value()) {
        claims_.emplace();
        if (const std::optional<NbtCompound> stored = read("structure_claims"); stored.has_value()) {
            if (const wire::nbt::NbtList* list = stored->getList("claims"); list != nullptr) {
                for (const NbtTag& element : list->elements()) {
                    if (const std::string* text = element.get<std::string>(); text != nullptr) {
                        claims_->insert(*text);
                    }
                }
            }
        }
    }
    return claims_->contains(key);
}

void MapStorage::claim(const std::string& key) {
    static_cast<void>(claimed(key));
    claimsDirty_ |= claims_->insert(key).second;
}

void MapStorage::save() {
    bool made = false;
    const auto write = [&](std::string_view name, NbtCompound data) {
        if (!made) {
            made = true;
            static_cast<void>(wire::io::fileIO::createDirectories(directory_));
        }
        static_cast<void>(wire::io::fileIO::writeBytes(pathOf(name), wrapped(std::move(data))));
    };
    for (auto& [id, data] : maps_) {
        if (data.has_value() && data->isDirty()) {
            write(std::to_string(id), data->save());
            data->clearDirty();
        }
    }
    if (claimsDirty_ && claims_.has_value()) {
        wire::nbt::NbtList list{wire::nbt::NbtType::String};
        for (const std::string& key : *claims_) {
            list.push(NbtTag::ofString(key));
        }
        NbtCompound stored;
        stored.set("claims", NbtTag::ofList(std::move(list)));
        write("structure_claims", std::move(stored));
        claimsDirty_ = false;
    }
    if (indexDirty_ && lastId_.has_value()) {
        NbtCompound index;
        index.putInt("map", *lastId_);
        write("last_id", std::move(index));
        indexDirty_ = false;
    }
}

} // namespace basalt::server::runtime::world::maps
