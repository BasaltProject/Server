#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `AbstractBoat` and its four kinds.
namespace boats {

/// parity: `AbstractBoat.DATA_ID_PADDLE_LEFT`.
inline constexpr std::uint8_t kPaddleLeftDataIndex = 11;
inline constexpr std::uint8_t kPaddleRightDataIndex = 12;

/// parity: the `outOfControlTicks` past.
inline constexpr float kOutOfControlTicks = 60.0F;

[[nodiscard]] bool isBoat(const wire::common::Identifier& type);

/// parity: `AbstractChestBoat`.
[[nodiscard]] bool hasChest(const wire::common::Identifier& type);

/// parity: `getMaxPassengers`: two, or one beside a chest.
[[nodiscard]] std::int32_t maxPassengers(const wire::common::Identifier& type);

/// parity: `AbstractBoat.Status`, in its declared order.
enum class Status : std::uint8_t { InWater, UnderWater, UnderFlowingWater, OnLand, InAir };

/// parity: `AbstractBoat`'s own fields.
struct BoatState {
    Status status = Status::InAir;
    Status oldStatus = Status::InAir;
    double waterLevel = 0.0;
    float landFriction = 0.0F;
    double lastYd = 0.0;
    /// parity: `paddlePositions`, how far round each oar has turned.
    std::array<float, 2> paddlePositions{};
    /// parity: `isAboveBubbleColumn`.
    bool aboveBubbleColumn = false;
    bool bubbleColumnDown = false;
    std::int32_t bubbleTime = 0;
};

/// parity: `rideHeight`: a third of the boat, or eight ninths of a raft.
[[nodiscard]] double rideHeight(const ServerEntity& boat);

struct TickResult {
    /// parity: sunk for three seconds: everybody aboard goes.
    bool ejectAll = false;

    /// parity: the oar strokes that splash this turn, as the sound and where it is heard.
    struct Stroke {
        std::string_view sound;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    std::vector<Stroke> strokes;
};

/// parity: `AbstractBoat.tick`.
[[nodiscard]] TickResult tick(world::WorldState& world, ServerEntity& boat, bool drivenByPlayer);

} // namespace boats

} // namespace basalt::server::runtime::entity
