#pragma once

#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/protocol/play/MapPackets.hpp>
#include <basalt/wire/text/Component.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::world::maps {

using BlockPos = std::array<std::int32_t, 3>;

/// A banner someone marked on a map. parity: `MapBanner`.
struct MapBanner {
    BlockPos pos{};
    std::string color = "white";
    std::optional<wire::text::Component> name;

    [[nodiscard]] std::string id() const;

    friend bool operator==(const MapBanner&, const MapBanner&) = default;
};

/// The frame a map hangs in. parity: `MapFrame`.
struct MapFrame {
    BlockPos pos{};
    std::int32_t rotation = 0;
    std::int32_t entityId = 0;

    [[nodiscard]] static std::string idOf(const BlockPos& pos);

    friend bool operator==(const MapFrame&, const MapFrame&) = default;
};

/// parity: `MapDecoration`, already placed on the map's own byte grid.
struct MapDecoration {
    std::int32_t type = 0;
    std::int8_t x = 0;
    std::int8_t y = 0;
    std::int8_t rotation = 0;
    std::optional<wire::text::Component> name;

    friend bool operator==(const MapDecoration&, const MapDecoration&) = default;
};

/// One map's saved state and who is looking at it. parity: `MapItemSavedData`.
class MapData {
public:
    static constexpr std::int32_t kSize = 128;
    static constexpr std::int32_t kMaxScale = 4;
    static constexpr std::int32_t kTrackedDecorationLimit = 256;

    /// parity: `MapItemSavedData.HoldingPlayer`.
    struct Holder {
        wire::common::Uuid player;
        std::int32_t entityId = 0;
        /// parity: `getPlainTextName`, the key of the player's own marker.
        std::string name;
        bool dirtyData = true;
        std::int32_t minDirtyX = 0;
        std::int32_t minDirtyY = 0;
        std::int32_t maxDirtyX = kSize - 1;
        std::int32_t maxDirtyY = kSize - 1;
        bool dirtyDecorations = true;
        std::int32_t tick = 0;
        std::int32_t step = 0;
    };

    /// parity: `MapItemSavedData.createFresh`, snapped to the grid of its scale.
    [[nodiscard]] static MapData createFresh(double originX, double originZ, std::int8_t scale, bool trackingPosition,
                                             bool unlimitedTracking, std::string dimension);

    /// parity: `MapItemSavedData.locked`, a copy that no longer draws.
    [[nodiscard]] MapData locked() const;

    /// parity: `MapItemSavedData.scaled`.
    [[nodiscard]] MapData scaled() const;

    [[nodiscard]] std::int32_t centerX() const noexcept { return centerX_; }

    [[nodiscard]] std::int32_t centerZ() const noexcept { return centerZ_; }

    [[nodiscard]] std::int8_t scale() const noexcept { return scale_; }

    [[nodiscard]] bool isLocked() const noexcept { return locked_; }

    [[nodiscard]] bool trackingPosition() const noexcept { return trackingPosition_; }

    [[nodiscard]] bool unlimitedTracking() const noexcept { return unlimitedTracking_; }

    [[nodiscard]] const std::string& dimension() const noexcept { return dimension_; }

    [[nodiscard]] std::span<const std::uint8_t> colors() const noexcept { return colors_; }

    /// parity: `MapItemSavedData.updateColor`: whether the pixel changed.
    bool updateColor(std::int32_t x, std::int32_t y, std::uint8_t color);

    void setColor(std::int32_t x, std::int32_t y, std::uint8_t color);

    /// parity: `addDecoration`.
    void addDecoration(std::int32_t type, std::optional<std::int64_t> gameTime, const std::string& key, double x,
                       double z, double yRot, std::optional<wire::text::Component> name);

    void removeDecoration(std::string_view key);

    [[nodiscard]] bool hasDecoration(std::string_view key) const;

    [[nodiscard]] const std::map<std::string, MapFrame>& frames() const noexcept { return frames_; }

    [[nodiscard]] const std::vector<std::pair<std::string, MapDecoration>>& decorations() const noexcept {
        return decorations_;
    }

    /// parity: `toggleBanner`.
    [[nodiscard]] bool toggleBanner(const std::optional<MapBanner>& banner, const BlockPos& pos,
                                    std::optional<std::int64_t> gameTime);

    /// parity: `checkBanners`.
    void checkBanners(std::int32_t x, std::int32_t z,
                      const std::function<std::optional<MapBanner>(const BlockPos&)>& bannerAt);

    void trackFrame(const MapFrame& frame, std::optional<std::int64_t> gameTime);

    void removedFromFrame(const BlockPos& pos, std::int32_t entityId);

    void addStaticDecorations(const wire::nbt::NbtCompound& stackDecorations, std::optional<std::int64_t> gameTime);

    [[nodiscard]] bool trackedCountOverLimit(std::int32_t limit) const noexcept {
        return trackedDecorationCount_ > limit;
    }

    /// parity: `getHoldingPlayer`, adding one on first sight.
    Holder& holder(const wire::common::Uuid& player, std::int32_t entityId, std::string_view name);

    [[nodiscard]] Holder* findHolder(const wire::common::Uuid& player);

    [[nodiscard]] std::vector<Holder>& holders() noexcept { return holders_; }

    void dropHolder(const wire::common::Uuid& player);

    /// parity: `HoldingPlayer.nextUpdatePacket`, or nothing when that player is up to date.
    [[nodiscard]] std::optional<wire::protocol::play::MapItemDataPacket> nextUpdate(std::int32_t mapId,
                                                                                    Holder& holder) const;

    [[nodiscard]] bool isDirty() const noexcept { return dirty_; }

    void clearDirty() noexcept { dirty_ = false; }

    /// parity: `MapItemSavedData.CODEC`, the compound under a saved file's `data`.
    [[nodiscard]] wire::nbt::NbtCompound save() const;

    [[nodiscard]] static std::optional<MapData> load(const wire::nbt::NbtCompound& tag);

private:
    MapData(std::int32_t centerX, std::int32_t centerZ, std::int8_t scale, bool trackingPosition,
            bool unlimitedTracking, bool locked, std::string dimension);

    [[nodiscard]] std::int8_t rotationOf(std::optional<std::int64_t> gameTime, double yRot) const;

    void markColorsDirty(std::int32_t x, std::int32_t y);

    void markDecorationsDirty();

    std::int32_t centerX_ = 0;
    std::int32_t centerZ_ = 0;
    std::string dimension_;
    bool trackingPosition_ = true;
    bool unlimitedTracking_ = false;
    std::int8_t scale_ = 0;
    bool locked_ = false;
    std::vector<std::uint8_t> colors_;
    std::map<std::string, MapBanner> banners_;
    /// Insertion ordered, like vanilla's linked map: the order the client draws them in.
    std::vector<std::pair<std::string, MapDecoration>> decorations_;
    std::map<std::string, MapFrame> frames_;
    std::int32_t trackedDecorationCount_ = 0;
    std::vector<Holder> holders_;
    bool dirty_ = false;
};

} // namespace basalt::server::runtime::world::maps
