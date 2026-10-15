#include <basalt/server/runtime/world/poi/PoiManager.hpp>
#include <basalt/server/world/region/AnvilRegion.hpp>
#include <basalt/server/world/region/LinearRegion.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <chrono>
#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::poi {
namespace {

using server::world::region::LinearRegion;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

/// parity: `SharedConstants.WORLD_VERSION` of 26.3.
constexpr std::int32_t kDataVersion = 5023;

[[nodiscard]] std::string regionName(std::int32_t regionX, std::int32_t regionZ, std::string_view extension) {
    return "r." + std::to_string(regionX) + "." + std::to_string(regionZ) + "." + std::string{extension};
}

} // namespace

NbtCompound PoiManager::encodeChunk(const std::vector<PoiRecord>& records, std::int32_t minSection,
                                    std::int32_t sectionCount) {
    std::map<std::int32_t, NbtList> bySection;
    for (const PoiRecord& record : records) {
        const std::int32_t sectionY = record.pos.y >> 4;
        if (sectionY < minSection || sectionY >= minSection + sectionCount) {
            continue;
        }
        NbtCompound entry;
        entry.putIntArray("pos", {record.pos.x, record.pos.y, record.pos.z});
        entry.putString("type", "minecraft:" + std::string{poiTypes::name(record.type)});
        entry.putInt("free_tickets", record.freeTickets);
        auto [list, inserted] = bySection.try_emplace(sectionY, NbtType::Compound);
        static_cast<void>(inserted);
        static_cast<void>(list->second.push(NbtTag::ofCompound(std::move(entry))));
    }
    NbtCompound sections;
    for (auto& [sectionY, list] : bySection) {
        NbtCompound section;
        section.putBoolean("Valid", true);
        section.set("Records", NbtTag::ofList(std::move(list)));
        sections.set(std::to_string(sectionY), NbtTag::ofCompound(std::move(section)));
    }
    NbtCompound root;
    root.set("Sections", NbtTag::ofCompound(std::move(sections)));
    root.putInt("DataVersion", kDataVersion);
    return root;
}

void PoiManager::readChunkLocked(std::int32_t chunkX, std::int32_t chunkZ, const NbtCompound& tag) {
    const NbtCompound* sections = tag.getCompound("Sections");
    if (sections == nullptr) {
        return;
    }
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    if (chunks_.contains(key)) {
        // Whatever this run already knows about the chunk is newer than the file.
        return;
    }
    ChunkPoi& chunk = chunks_[key];
    chunk.sections.resize(static_cast<std::size_t>(bounds_.sectionCount()));
    for (std::size_t index = 0; index < sections->names().size(); ++index) {
        const std::string& name = sections->names()[index];
        const NbtCompound* section = sections->values()[index].get<NbtCompound>();
        std::int32_t sectionY = 0;
        try {
            sectionY = std::stoi(name);
        } catch (...) {
            continue;
        }
        const std::int32_t slot = sectionY - bounds_.minSection();
        if (section == nullptr || slot < 0 || slot >= bounds_.sectionCount()) {
            continue;
        }
        Section& target = chunk.sections[static_cast<std::size_t>(slot)].emplace();
        // parity: `Valid` defaults to false.
        target.valid = section->getBoolean("Valid", false);
        const NbtList* records = section->getList("Records");
        if (records == nullptr) {
            continue;
        }
        for (const NbtTag& entry : records->elements()) {
            const NbtCompound* record = entry.get<NbtCompound>();
            if (record == nullptr) {
                continue;
            }
            const std::span<const std::int32_t> pos = record->getIntArray("pos");
            const std::optional<PoiType> type = poiTypes::byName(record->getString("type"));
            if (pos.size() < 3 || !type.has_value()) {
                continue;
            }
            addLocked(target, BlockPos{pos[0], pos[1], pos[2]}, *type, record->getInt("free_tickets", 0));
        }
    }
    for (std::int32_t slot = 0; slot < bounds_.sectionCount(); ++slot) {
        refreshCenterLocked(chunkX, bounds_.minSection() + slot, chunkZ);
    }
}

void PoiManager::ensureRegionLocked(std::int32_t regionX, std::int32_t regionZ) {
    const std::int64_t key = chunkKey(regionX, regionZ);
    if (!loadedRegions_.insert(key).second || directory_.empty()) {
        return;
    }
    std::optional<LinearRegion> region;
    if (const std::optional<std::vector<std::uint8_t>> bytes =
            wire::io::fileIO::readBytes(directory_ + "/" + regionName(regionX, regionZ, "linear"));
        bytes.has_value()) {
        if (auto read = LinearRegion::read(*bytes); read.has_value()) {
            region = std::move(*read);
        }
    } else if (const std::optional<std::vector<std::uint8_t>> anvil =
                   wire::io::fileIO::readBytes(directory_ + "/" + regionName(regionX, regionZ, "mca"));
               anvil.has_value()) {
        if (auto read = server::world::region::anvilRegion::read(*anvil); read.has_value()) {
            region = std::move(*read);
        }
    }
    if (!region.has_value()) {
        return;
    }
    for (std::int32_t localZ = 0; localZ < LinearRegion::kDimension; ++localZ) {
        for (std::int32_t localX = 0; localX < LinearRegion::kDimension; ++localX) {
            if (!region->hasChunkAt(localX, localZ)) {
                continue;
            }
            const auto parsed = wire::nbt::nbtIO::read(region->chunkAt(localX, localZ));
            const NbtCompound* root = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
            if (root != nullptr) {
                readChunkLocked((regionX << 5) + localX, (regionZ << 5) + localZ, *root);
            }
        }
    }
}

void PoiManager::save() {
    const std::lock_guard lock{mutex_};
    if (directory_.empty() || dirtyRegions_.empty()) {
        dirtyRegions_.clear();
        return;
    }
    static_cast<void>(wire::io::fileIO::createDirectories(directory_));
    const auto timestamp = static_cast<std::int32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    for (const std::int64_t regionKey : dirtyRegions_) {
        const auto regionX = static_cast<std::int32_t>(regionKey);
        const auto regionZ = static_cast<std::int32_t>(regionKey >> 32);
        LinearRegion region;
        for (std::int32_t localZ = 0; localZ < LinearRegion::kDimension; ++localZ) {
            for (std::int32_t localX = 0; localX < LinearRegion::kDimension; ++localX) {
                const auto found = chunks_.find(chunkKey((regionX << 5) + localX, (regionZ << 5) + localZ));
                if (found == chunks_.end()) {
                    continue;
                }
                std::vector<PoiRecord> records;
                bool any = false;
                for (const std::optional<Section>& section : found->second.sections) {
                    if (section.has_value()) {
                        any = true;
                        records.insert(records.end(), section->records.begin(), section->records.end());
                    }
                }
                if (!any) {
                    continue;
                }
                NbtCompound root = encodeChunk(records, bounds_.minSection(), bounds_.sectionCount());
                // Empty sections are written too, so a reload does not rebuild them from blocks.
                NbtCompound* sections = nullptr;
                if (NbtTag* tag = root.find("Sections"); tag != nullptr) {
                    sections = tag->get<NbtCompound>();
                }
                for (std::size_t slot = 0; slot < found->second.sections.size() && sections != nullptr; ++slot) {
                    const std::string name = std::to_string(bounds_.minSection() + static_cast<std::int32_t>(slot));
                    if (found->second.sections[slot].has_value() && !sections->contains(name)) {
                        NbtCompound empty;
                        empty.putBoolean("Valid", true);
                        empty.set("Records", NbtTag::ofList(NbtList{NbtType::Compound}));
                        sections->set(name, NbtTag::ofCompound(std::move(empty)));
                    }
                }
                region.setChunk(localX, localZ, wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(root))),
                                timestamp);
            }
        }
        static_cast<void>(region.writeTo(directory_ + "/" + regionName(regionX, regionZ, "linear")));
    }
    dirtyRegions_.clear();
}

} // namespace basalt::server::runtime::world::poi
