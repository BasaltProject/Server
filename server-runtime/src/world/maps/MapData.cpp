#include <basalt/server/runtime/world/maps/MapData.hpp>
#include <basalt/server/runtime/world/maps/MapDecorationTypes.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace basalt::server::runtime::world::maps {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

/// parity: the half size less one that `isInsideMap` and `clampMapCoordinate` measure against.
constexpr float kHalfSize = 63.0F;
/// parity: `decorationTypeForPlayerOutsideMap`'s range for the small off-map dot.
constexpr float kOffMapRange = 320.0F;
constexpr std::string_view kNether = "minecraft:the_nether";
constexpr std::string_view kFramePrefix = "frame-";

[[nodiscard]] bool insideMap(float x, float y) {
    return x >= -kHalfSize && y >= -kHalfSize && x <= kHalfSize && y <= kHalfSize;
}

/// parity: `MapItemSavedData.clampMapCoordinate`.
[[nodiscard]] std::int8_t clampCoordinate(float delta) {
    if (delta <= -kHalfSize) {
        return -128;
    }
    if (delta >= kHalfSize) {
        return 127;
    }
    return static_cast<std::int8_t>(static_cast<std::int32_t>(static_cast<double>(delta * 2.0F) + 0.5));
}

[[nodiscard]] std::string frameKey(std::int32_t entityId) {
    return std::string{kFramePrefix} + std::to_string(entityId);
}

[[nodiscard]] std::string posKey(std::string_view prefix, const BlockPos& pos) {
    return std::string{prefix} + std::to_string(pos[0]) + "," + std::to_string(pos[1]) + "," + std::to_string(pos[2]);
}

[[nodiscard]] NbtTag posTag(const BlockPos& pos) { return NbtTag::ofIntArray({pos[0], pos[1], pos[2]}); }

[[nodiscard]] std::optional<BlockPos> posOf(const NbtCompound& tag) {
    const std::span<const std::int32_t> pos = tag.getIntArray("pos");
    return pos.size() == 3 ? std::optional{BlockPos{pos[0], pos[1], pos[2]}} : std::nullopt;
}

[[nodiscard]] bool tracksCount(std::int32_t type) {
    const MapDecorationType* found = decorationTypes::byId(type);
    return found != nullptr && found->trackCount;
}

} // namespace

std::string MapBanner::id() const { return posKey("banner-", pos); }

std::string MapFrame::idOf(const BlockPos& pos) { return posKey(kFramePrefix, pos); }

MapData::MapData(std::int32_t centerX, std::int32_t centerZ, std::int8_t scale, bool trackingPosition,
                 bool unlimitedTracking, bool locked, std::string dimension)
    : centerX_(centerX), centerZ_(centerZ), dimension_(std::move(dimension)), trackingPosition_(trackingPosition),
      unlimitedTracking_(unlimitedTracking), scale_(scale), locked_(locked),
      colors_(static_cast<std::size_t>(kSize * kSize), 0) {}

MapData MapData::createFresh(double originX, double originZ, std::int8_t scale, bool trackingPosition,
                             bool unlimitedTracking, std::string dimension) {
    const std::int32_t size = kSize * (1 << scale);
    const std::int32_t areaX = wire::common::mth::floor((originX + 64.0) / static_cast<double>(size));
    const std::int32_t areaZ = wire::common::mth::floor((originZ + 64.0) / static_cast<double>(size));
    const std::int32_t x = (areaX * size) + (size / 2) - 64;
    const std::int32_t z = (areaZ * size) + (size / 2) - 64;
    MapData data{x, z, scale, trackingPosition, unlimitedTracking, false, std::move(dimension)};
    data.dirty_ = true;
    return data;
}

MapData MapData::locked() const {
    MapData result{centerX_, centerZ_, scale_, trackingPosition_, unlimitedTracking_, true, dimension_};
    result.banners_ = banners_;
    result.decorations_ = decorations_;
    result.trackedDecorationCount_ = trackedDecorationCount_;
    result.colors_ = colors_;
    result.dirty_ = true;
    return result;
}

MapData MapData::scaled() const {
    return createFresh(centerX_, centerZ_, static_cast<std::int8_t>(std::clamp(scale_ + 1, 0, kMaxScale)),
                       trackingPosition_, unlimitedTracking_, dimension_);
}

bool MapData::updateColor(std::int32_t x, std::int32_t y, std::uint8_t color) {
    if (colors_[static_cast<std::size_t>(x + (y * kSize))] == color) {
        return false;
    }
    setColor(x, y, color);
    return true;
}

void MapData::setColor(std::int32_t x, std::int32_t y, std::uint8_t color) {
    colors_[static_cast<std::size_t>(x + (y * kSize))] = color;
    dirty_ = true;
    markColorsDirty(x, y);
}

std::int8_t MapData::rotationOf(std::optional<std::int64_t> gameTime, double yRot) const {
    if (dimension_ == kNether && gameTime.has_value()) {
        // parity: `calculateRotation` spins a Nether marker off the clock.
        const auto s = static_cast<std::uint32_t>(static_cast<std::int32_t>(*gameTime / 10));
        const auto mixed = static_cast<std::int32_t>((s * s * 34187121U) + (s * 121U));
        return static_cast<std::int8_t>((mixed >> 15) & 15);
    }
    const double adjusted = yRot < 0.0 ? yRot - 8.0 : yRot + 8.0;
    const double sixteenths = std::clamp(adjusted * 16.0 / 360.0, -2147483648.0, 2147483647.0);
    return static_cast<std::int8_t>(static_cast<std::int32_t>(sixteenths) & 0xFF);
}

void MapData::addDecoration(std::int32_t type, std::optional<std::int64_t> gameTime, const std::string& key, double x,
                            double z, double yRot, std::optional<wire::text::Component> name) {
    const auto scaling = static_cast<float>(1 << scale_);
    const float dx = static_cast<float>(x - centerX_) / scaling;
    const float dy = static_cast<float>(z - centerZ_) / scaling;
    std::int32_t placed = type;
    if (type == decorationTypes::kPlayer) {
        if (!insideMap(dx, dy)) {
            // parity: `decorationTypeForPlayerOutsideMap`.
            if (std::abs(dx) < kOffMapRange && std::abs(dy) < kOffMapRange) {
                placed = decorationTypes::kPlayerOffMap;
            } else if (unlimitedTracking_) {
                placed = decorationTypes::kPlayerOffLimits;
            } else {
                removeDecoration(key);
                return;
            }
        }
    } else if (!insideMap(dx, dy) && !unlimitedTracking_) {
        removeDecoration(key);
        return;
    }
    MapDecoration decoration{.type = placed,
                             .x = clampCoordinate(dx),
                             .y = clampCoordinate(dy),
                             .rotation = static_cast<std::int8_t>(rotationOf(gameTime, yRot) & 15),
                             .name = std::move(name)};
    const auto found = std::ranges::find(decorations_, key, &std::pair<std::string, MapDecoration>::first);
    if (found == decorations_.end()) {
        if (tracksCount(placed)) {
            ++trackedDecorationCount_;
        }
        decorations_.emplace_back(key, std::move(decoration));
        markDecorationsDirty();
        return;
    }
    if (found->second == decoration) {
        return;
    }
    if (tracksCount(found->second.type)) {
        --trackedDecorationCount_;
    }
    if (tracksCount(placed)) {
        ++trackedDecorationCount_;
    }
    found->second = std::move(decoration);
    markDecorationsDirty();
}

void MapData::removeDecoration(std::string_view key) {
    const auto found = std::ranges::find(decorations_, key, &std::pair<std::string, MapDecoration>::first);
    if (found != decorations_.end()) {
        if (tracksCount(found->second.type)) {
            --trackedDecorationCount_;
        }
        decorations_.erase(found);
    }
    markDecorationsDirty();
}

bool MapData::hasDecoration(std::string_view key) const {
    return std::ranges::find(decorations_, key, &std::pair<std::string, MapDecoration>::first) != decorations_.end();
}

bool MapData::toggleBanner(const std::optional<MapBanner>& banner, const BlockPos& pos,
                           std::optional<std::int64_t> gameTime) {
    const double x = pos[0] + 0.5;
    const double z = pos[2] + 0.5;
    const auto scale = static_cast<double>(1 << scale_);
    const double dx = (x - centerX_) / scale;
    const double dy = (z - centerZ_) / scale;
    if (dx < static_cast<double>(-kHalfSize) || dy < static_cast<double>(-kHalfSize) ||
        dx > static_cast<double>(kHalfSize) || dy > static_cast<double>(kHalfSize) || !banner.has_value()) {
        return false;
    }
    const std::string id = banner->id();
    if (const auto found = banners_.find(id); found != banners_.end() && found->second == *banner) {
        banners_.erase(found);
        removeDecoration(id);
        dirty_ = true;
        return true;
    }
    if (trackedCountOverLimit(kTrackedDecorationLimit)) {
        return false;
    }
    banners_.insert_or_assign(id, *banner);
    if (const std::optional<std::int32_t> type = decorationTypes::bannerOf(banner->color); type.has_value()) {
        addDecoration(*type, gameTime, id, x, z, 180.0, banner->name);
    }
    dirty_ = true;
    return true;
}

void MapData::checkBanners(std::int32_t x, std::int32_t z,
                           const std::function<std::optional<MapBanner>(const BlockPos&)>& bannerAt) {
    for (auto it = banners_.begin(); it != banners_.end();) {
        const MapBanner& expected = it->second;
        if (expected.pos[0] != x || expected.pos[2] != z || bannerAt(expected.pos) == expected) {
            ++it;
            continue;
        }
        const std::string id = it->first;
        it = banners_.erase(it);
        removeDecoration(id);
        dirty_ = true;
    }
}

void MapData::trackFrame(const MapFrame& frame, std::optional<std::int64_t> gameTime) {
    const std::string id = MapFrame::idOf(frame.pos);
    if (const auto existing = frames_.find(id);
        existing != frames_.end() && existing->second.entityId != frame.entityId) {
        removeDecoration(frameKey(existing->second.entityId));
    }
    addDecoration(decorationTypes::kFrame, gameTime, frameKey(frame.entityId), frame.pos[0], frame.pos[2],
                  frame.rotation, std::nullopt);
    const auto [slot, inserted] = frames_.try_emplace(id, frame);
    if (inserted || slot->second != frame) {
        slot->second = frame;
        dirty_ = true;
    }
}

void MapData::removedFromFrame(const BlockPos& pos, std::int32_t entityId) {
    removeDecoration(frameKey(entityId));
    frames_.erase(MapFrame::idOf(pos));
    dirty_ = true;
}

void MapData::addStaticDecorations(const NbtCompound& stackDecorations, std::optional<std::int64_t> gameTime) {
    for (std::size_t index = 0; index < stackDecorations.names().size(); ++index) {
        const std::string& key = stackDecorations.names()[index];
        const NbtCompound* entry = stackDecorations.values()[index].get<NbtCompound>();
        if (entry == nullptr || hasDecoration(key)) {
            continue;
        }
        const std::optional<std::int32_t> type = decorationTypes::idOf(entry->getString("type"));
        if (type.has_value()) {
            addDecoration(*type, gameTime, key, entry->getDouble("x"), entry->getDouble("z"),
                          static_cast<double>(entry->getFloat("rotation")), std::nullopt);
        }
    }
}

MapData::Holder& MapData::holder(const wire::common::Uuid& player, std::int32_t entityId, std::string_view name) {
    if (Holder* found = findHolder(player); found != nullptr && found->entityId == entityId) {
        return *found;
    }
    dropHolder(player);
    holders_.push_back(Holder{.player = player, .entityId = entityId, .name = std::string{name}});
    return holders_.back();
}

MapData::Holder* MapData::findHolder(const wire::common::Uuid& player) {
    const auto found = std::ranges::find(holders_, player, &Holder::player);
    return found == holders_.end() ? nullptr : &*found;
}

void MapData::dropHolder(const wire::common::Uuid& player) {
    std::erase_if(holders_, [&](const Holder& holder) { return holder.player == player; });
}

void MapData::markColorsDirty(std::int32_t x, std::int32_t y) {
    for (Holder& holder : holders_) {
        if (holder.dirtyData) {
            holder.minDirtyX = std::min(holder.minDirtyX, x);
            holder.minDirtyY = std::min(holder.minDirtyY, y);
            holder.maxDirtyX = std::max(holder.maxDirtyX, x);
            holder.maxDirtyY = std::max(holder.maxDirtyY, y);
        } else {
            holder.dirtyData = true;
            holder.minDirtyX = x;
            holder.minDirtyY = y;
            holder.maxDirtyX = x;
            holder.maxDirtyY = y;
        }
    }
}

void MapData::markDecorationsDirty() {
    for (Holder& holder : holders_) {
        holder.dirtyDecorations = true;
    }
}

std::optional<wire::protocol::play::MapItemDataPacket> MapData::nextUpdate(std::int32_t mapId, Holder& holder) const {
    wire::protocol::play::MapItemDataPacket packet{.mapId = mapId, .scale = scale_, .locked = locked_};
    if (holder.dirtyData) {
        holder.dirtyData = false;
        wire::protocol::play::MapColorPatch patch;
        const std::int32_t width = holder.maxDirtyX + 1 - holder.minDirtyX;
        const std::int32_t height = holder.maxDirtyY + 1 - holder.minDirtyY;
        patch.startX = static_cast<std::uint8_t>(holder.minDirtyX);
        patch.startY = static_cast<std::uint8_t>(holder.minDirtyY);
        patch.width = static_cast<std::uint8_t>(width);
        patch.height = static_cast<std::uint8_t>(height);
        patch.colors.resize(static_cast<std::size_t>(width * height));
        for (std::int32_t x = 0; x < width; ++x) {
            for (std::int32_t y = 0; y < height; ++y) {
                patch.colors[static_cast<std::size_t>(x + (y * width))] =
                    colors_[static_cast<std::size_t>(holder.minDirtyX + x + ((holder.minDirtyY + y) * kSize))];
            }
        }
        packet.patch = std::move(patch);
    }
    // parity: decorations go out at most every fifth tick they are dirty on.
    if (holder.dirtyDecorations && holder.tick++ % 5 == 0) {
        holder.dirtyDecorations = false;
        std::vector<wire::protocol::play::MapDecorationEntry> entries;
        entries.reserve(decorations_.size());
        for (const auto& [key, decoration] : decorations_) {
            entries.push_back({.type = decoration.type,
                               .x = decoration.x,
                               .y = decoration.y,
                               .rotation = decoration.rotation,
                               .name = decoration.name});
        }
        packet.decorations = std::move(entries);
    }
    if (!packet.patch.has_value() && !packet.decorations.has_value()) {
        return std::nullopt;
    }
    return packet;
}

NbtCompound MapData::save() const {
    NbtCompound tag;
    tag.putString("dimension", dimension_);
    tag.putInt("xCenter", centerX_);
    tag.putInt("zCenter", centerZ_);
    tag.putByte("scale", scale_);
    tag.putByteArray("colors", std::vector<std::int8_t>(colors_.begin(), colors_.end()));
    tag.putBoolean("trackingPosition", trackingPosition_);
    tag.putBoolean("unlimitedTracking", unlimitedTracking_);
    tag.putBoolean("locked", locked_);
    NbtList banners{NbtType::Compound};
    for (const auto& [id, banner] : banners_) {
        NbtCompound entry;
        entry.set("pos", posTag(banner.pos));
        entry.putString("color", banner.color);
        if (banner.name.has_value()) {
            entry.set("name", wire::text::componentNbt::serialize(*banner.name));
        }
        banners.push(NbtTag::ofCompound(std::move(entry)));
    }
    tag.set("banners", NbtTag::ofList(std::move(banners)));
    NbtList frames{NbtType::Compound};
    for (const auto& [id, frame] : frames_) {
        NbtCompound entry;
        entry.set("pos", posTag(frame.pos));
        entry.putInt("rotation", frame.rotation);
        entry.putInt("entity_id", frame.entityId);
        frames.push(NbtTag::ofCompound(std::move(entry)));
    }
    tag.set("frames", NbtTag::ofList(std::move(frames)));
    return tag;
}

std::optional<MapData> MapData::load(const NbtCompound& tag) {
    if (!tag.contains("dimension") || !tag.contains("xCenter") || !tag.contains("zCenter")) {
        return std::nullopt;
    }
    MapData data{tag.getInt("xCenter"),
                 tag.getInt("zCenter"),
                 static_cast<std::int8_t>(std::clamp<std::int32_t>(tag.getByte("scale"), 0, kMaxScale)),
                 tag.getBoolean("trackingPosition", true),
                 tag.getBoolean("unlimitedTracking", false),
                 tag.getBoolean("locked", false),
                 std::string{tag.getString("dimension")}};
    if (const std::span<const std::int8_t> colors = tag.getByteArray("colors"); colors.size() == data.colors_.size()) {
        std::ranges::transform(colors, data.colors_.begin(),
                               [](std::int8_t c) { return static_cast<std::uint8_t>(c); });
    }
    if (const wire::nbt::NbtList* banners = tag.getList("banners"); banners != nullptr) {
        for (const NbtTag& element : banners->elements()) {
            const NbtCompound* entry = element.get<NbtCompound>();
            const std::optional<BlockPos> pos = entry == nullptr ? std::nullopt : posOf(*entry);
            if (!pos.has_value()) {
                continue;
            }
            MapBanner banner{.pos = *pos, .color = std::string{entry->getString("color", "white")}};
            if (const NbtTag* name = entry->find("name"); name != nullptr) {
                banner.name = wire::text::componentNbt::deserialize(*name);
            }
            const std::string id = banner.id();
            if (const std::optional<std::int32_t> type = decorationTypes::bannerOf(banner.color); type.has_value()) {
                data.addDecoration(*type, std::nullopt, id, pos->at(0), pos->at(2), 180.0, banner.name);
            }
            data.banners_.insert_or_assign(id, std::move(banner));
        }
    }
    if (const wire::nbt::NbtList* frames = tag.getList("frames"); frames != nullptr) {
        for (const NbtTag& element : frames->elements()) {
            const NbtCompound* entry = element.get<NbtCompound>();
            const std::optional<BlockPos> pos = entry == nullptr ? std::nullopt : posOf(*entry);
            if (!pos.has_value()) {
                continue;
            }
            const MapFrame frame{
                .pos = *pos, .rotation = entry->getInt("rotation"), .entityId = entry->getInt("entity_id")};
            data.frames_.insert_or_assign(MapFrame::idOf(frame.pos), frame);
            data.addDecoration(decorationTypes::kFrame, std::nullopt, frameKey(frame.entityId), pos->at(0), pos->at(2),
                               frame.rotation, std::nullopt);
        }
    }
    return data;
}

} // namespace basalt::server::runtime::world::maps
