#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

class RealBlockTransformers {
public:
    using ReadText = std::function<std::optional<std::string>(std::string_view name)>;

    using Tagged = std::function<bool(std::string_view tag, const wire::common::Identifier& block)>;

    enum class Particle : std::uint8_t { None, Scrape, WaxOn, WaxOff };

    enum class Drop : std::uint8_t { FromMiddle, ClickedFace };

    struct Provider;

    struct Transform {
        std::shared_ptr<const Provider> provider;
        std::string sound;
        Particle particle = Particle::None;
        std::vector<wire::protocol::play::Direction> disallowedFaces;
        std::optional<wire::common::Identifier> loot;
        Drop drop = Drop::FromMiddle;
        bool updateFromNeighbors = true;
        bool copperChest = false;
        bool consumeOnUse = true;
        std::int32_t itemDamagePerUse = 0;
    };

    explicit RealBlockTransformers(ReadText readText) : readText_(std::move(readText)) {}

    [[nodiscard]] const std::vector<Transform>& of(const wire::common::Identifier& id);

    [[nodiscard]] static std::vector<Transform> parse(std::string_view json);

    [[nodiscard]] static std::optional<std::int32_t> stateFor(const Transform& transform, const WorldState& world,
                                                              std::int32_t x, std::int32_t y, std::int32_t z,
                                                              const Tagged& tagged);

    [[nodiscard]] static std::optional<wire::common::Identifier> defaultFor(const wire::common::Identifier& item);

private:
    ReadText readText_;
    // Several connection threads use tools at once; the cache has one owner and it is this lock.
    std::mutex mutex_;
    std::map<wire::common::Identifier, std::vector<Transform>> cache_;
};

} // namespace basalt::server::runtime::world
