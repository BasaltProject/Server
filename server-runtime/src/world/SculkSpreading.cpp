#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/SculkSpreading.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>

namespace basalt::server::runtime::world::sculkSpreading {
namespace {

using Pos = std::array<std::int32_t, 3>;
using registry::BlockFace;
using wire::common::Identifier;
using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;

/// parity: `createLevelSpreader`'s growth cost, no-growth radius and two decay rates.
constexpr std::int32_t kGrowthSpawnCost = 10;
constexpr std::int32_t kNoGrowthRadius = 4;
constexpr std::int32_t kChargeDecayRate = 10;
constexpr std::int32_t kAdditionalDecayRate = 5;
constexpr std::int32_t kMaxCursorDistance = 1024;
constexpr std::int32_t kMaxGrowthRadius = 24;
constexpr std::int32_t kShriekerRate = 11;
constexpr std::int32_t kSpreadEvent = 3006;

/// parity: `Direction.values()`, whose order every shuffle and bit here follows.
struct Direction {
    std::string_view name;
    Pos step;
    std::int32_t axis;
    std::size_t opposite;
};

constexpr std::array<Direction, 6> kDirections{{{"down", {0, -1, 0}, 1, 1},
                                                {"up", {0, 1, 0}, 1, 0},
                                                {"north", {0, 0, -1}, 2, 3},
                                                {"south", {0, 0, 1}, 2, 2},
                                                {"west", {-1, 0, 0}, 0, 5},
                                                {"east", {1, 0, 0}, 0, 4}}};

enum class Behaviour : std::uint8_t { Default, Sculk, Vein };
enum class SpreadType : std::uint8_t { SamePosition, SamePlane, WrapAround };

[[nodiscard]] Pos offset(const Pos& pos, const Pos& by) { return {pos[0] + by[0], pos[1] + by[1], pos[2] + by[2]}; }

[[nodiscard]] Pos offset(const Pos& pos, std::size_t direction) { return offset(pos, kDirections[direction].step); }

[[nodiscard]] BlockFace faceOf(std::size_t direction) { return static_cast<BlockFace>(direction); }

[[nodiscard]] std::string_view pathOf(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] Behaviour behaviourOf(std::int32_t stateId) {
    const std::string_view path = pathOf(stateId);
    return path == "sculk" ? Behaviour::Sculk : path == "sculk_vein" ? Behaviour::Vein : Behaviour::Default;
}

[[nodiscard]] bool hasFace(std::int32_t stateId, std::size_t direction) {
    if (pathOf(stateId) != "sculk_vein") {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(std::string{kDirections[direction].name});
    return found != properties.end() && found->second == "true";
}

[[nodiscard]] std::uint8_t facesOf(std::int32_t stateId) {
    std::uint8_t faces = 0;
    for (std::size_t direction = 0; direction < kDirections.size(); ++direction) {
        if (hasFace(stateId, direction)) {
            faces = static_cast<std::uint8_t>(faces | (1U << direction));
        }
    }
    return faces;
}

[[nodiscard]] std::int32_t withProperty(std::int32_t stateId, std::string_view property, bool value) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return stateId;
    }
    return registry::blockStateDefinitions::stateIdOf(*block, {{std::string{property}, value ? "true" : "false"}},
                                                      stateId)
        .value_or(stateId);
}

[[nodiscard]] std::int32_t defaultOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

/// parity: `FluidState.isSourceOfType(WATER)`, still water or anything waterlogged.
[[nodiscard]] bool isWaterSource(std::int32_t stateId) {
    return registry::blockFluids::hasWaterFluid(stateId) &&
           (pathOf(stateId) != "water" || stateId == registry::blocks::water().stateId);
}

class Spreader {
public:
    Spreader(WorldState& world, random::RandomSource& random, const BlockTagged& tagged, std::vector<Step>& steps)
        : world_(&world), random_(&random), tagged_(&tagged), steps_(&steps) {}

    [[nodiscard]] std::int32_t at(const Pos& pos) const {
        return world_->blockState(pos[0], pos[1], pos[2]).value_or(registry::blocks::air().stateId);
    }

    [[nodiscard]] bool is(std::string_view tag, std::int32_t stateId) const {
        return *tagged_ && (*tagged_)(tag, stateId);
    }

    void set(const Pos& pos, std::int32_t stateId, bool updatesNeighbours) {
        if (!world_->setBlockState(pos[0], pos[1], pos[2], stateId)) {
            return;
        }
        steps_->push_back(Step{.pos = pos, .stateId = stateId, .updatesNeighbours = updatesNeighbours});
    }

    void sound(const Pos& pos, std::string_view name) {
        steps_->push_back(Step{.pos = pos, .effect = blockEffect::Sound{.name = std::string{name}}});
    }

    void event(const Pos& pos, std::int32_t data) {
        steps_->push_back(Step{.pos = pos, .effect = blockEffect::Effect{.type = kSpreadEvent, .data = data}});
    }

    [[nodiscard]] random::RandomSource& random() { return *random_; }

    /// parity: `Util.shuffle` over a copy, which is `Direction.allShuffled` too.
    template <typename T>
    void shuffle(std::vector<T>& values) {
        for (auto size = static_cast<std::int32_t>(values.size()); size > 1; --size) {
            const std::int32_t swapTo = random_->nextInt(size);
            std::swap(values[static_cast<std::size_t>(size - 1)], values[static_cast<std::size_t>(swapTo)]);
        }
    }

    [[nodiscard]] std::vector<std::size_t> shuffledDirections() {
        std::vector<std::size_t> order{0, 1, 2, 3, 4, 5};
        shuffle(order);
        return order;
    }

    /// parity: `MultifaceBlock.isValidStateForPlacement` for a vein.
    [[nodiscard]] bool validPlacement(std::int32_t existing, const Pos& pos, std::size_t face) const {
        if (hasFace(existing, face)) {
            return false;
        }
        return registry::blockShapes::canAttachTo(at(offset(pos, face)), faceOf(kDirections[face].opposite));
    }

    /// parity: `SculkVeinSpreaderConfig.stateCanBeReplaced` over the default config's.
    [[nodiscard]] bool replaceable(const Pos& source, const Pos& pos, std::size_t face, std::int32_t existing) const {
        const std::string_view against = pathOf(at(offset(pos, face)));
        if (against == "sculk" || against == "sculk_catalyst" || against == "moving_piston") {
            return false;
        }
        if (std::abs(source[0] - pos[0]) + std::abs(source[1] - pos[1]) + std::abs(source[2] - pos[2]) == 2) {
            const Pos neighbour = offset(source, kDirections[face].opposite);
            if (registry::blockShapes::isFaceSturdy(at(neighbour), faceOf(face))) {
                return false;
            }
        }
        if (registry::blockFluids::hasFluid(existing) && !registry::blockFluids::hasWaterFluid(existing)) {
            return false;
        }
        if (is("minecraft:fire", existing)) {
            return false;
        }
        return registry::blockShapes::isReplaceable(existing) || registry::blocks::isAir(existing) ||
               pathOf(existing) == "sculk_vein" || existing == registry::blocks::water().stateId;
    }

    /// parity: `MultifaceBlock.getStateForPlacement` for a vein.
    [[nodiscard]] std::optional<std::int32_t> placement(std::int32_t existing, const Pos& pos, std::size_t face) const {
        if (!validPlacement(existing, pos, face)) {
            return std::nullopt;
        }
        std::int32_t state = existing;
        if (pathOf(existing) != "sculk_vein") {
            state = defaultOf("sculk_vein");
            if (isWaterSource(existing)) {
                state = withProperty(state, "waterlogged", true);
            }
        }
        return withProperty(state, kDirections[face].name, true);
    }

    /// parity: `MultifaceSpreader.spreadAll` with a vein's config, over `spreadTypes`.
    std::int32_t spreadAll(std::int32_t state, const Pos& pos, bool samePositionOnly) {
        const bool otherSource = pathOf(state) != "sculk_vein";
        std::int32_t placed = 0;
        for (std::size_t from = 0; from < kDirections.size(); ++from) {
            if (!otherSource && !hasFace(state, from)) {
                continue;
            }
            for (std::size_t toward = 0; toward < kDirections.size(); ++toward) {
                if (kDirections[toward].axis == kDirections[from].axis || (!otherSource && hasFace(state, toward))) {
                    continue;
                }
                for (const SpreadType type :
                     {SpreadType::SamePosition, SpreadType::SamePlane, SpreadType::WrapAround}) {
                    if (samePositionOnly && type != SpreadType::SamePosition) {
                        break;
                    }
                    Pos target = pos;
                    std::size_t face = toward;
                    if (type == SpreadType::SamePlane) {
                        target = offset(pos, toward);
                        face = from;
                    } else if (type == SpreadType::WrapAround) {
                        target = offset(offset(pos, toward), from);
                        face = kDirections[toward].opposite;
                    }
                    const std::int32_t existing = at(target);
                    if (!replaceable(pos, target, face, existing) || !validPlacement(existing, target, face)) {
                        continue;
                    }
                    // parity: `spreadToFace`, which writes with flag two.
                    if (const std::optional<std::int32_t> grown = placement(existing, target, face);
                        grown.has_value()) {
                        set(target, *grown, false);
                        ++placed;
                    }
                    break;
                }
            }
        }
        return placed;
    }

    /// parity: `SculkVeinBlock.regrow`.
    bool regrow(const Pos& pos, std::int32_t existing, std::uint8_t faces) {
        std::int32_t state = defaultOf("sculk_vein");
        bool any = false;
        for (std::size_t face = 0; face < kDirections.size(); ++face) {
            if ((faces & (1U << face)) != 0 &&
                registry::blockShapes::canAttachTo(at(offset(pos, face)), faceOf(kDirections[face].opposite))) {
                state = withProperty(state, kDirections[face].name, true);
                any = true;
            }
        }
        if (!any) {
            return false;
        }
        if (registry::blockFluids::hasFluid(existing)) {
            state = withProperty(state, "waterlogged", true);
        }
        set(pos, state, true);
        return true;
    }

    bool attemptSpreadVein(Behaviour behaviour, const Pos& pos, std::int32_t state,
                           const std::optional<std::uint8_t>& facings) {
        if (behaviour != Behaviour::Default) {
            return spreadAll(state, pos, false) > 0;
        }
        if (!facings.has_value()) {
            return spreadAll(at(pos), pos, true) > 0;
        }
        if (*facings != 0) {
            return (registry::blocks::isAir(state) || registry::blockFluids::hasWaterFluid(state)) &&
                   regrow(pos, state, *facings);
        }
        return spreadAll(state, pos, false) > 0;
    }

    /// parity: `SculkVeinBlock.onDischarged`.
    void dischargeVein(const Pos& pos, std::int32_t state) {
        if (pathOf(state) != "sculk_vein") {
            return;
        }
        for (std::size_t face = 0; face < kDirections.size(); ++face) {
            if (hasFace(state, face) && pathOf(at(offset(pos, face))) == "sculk") {
                state = withProperty(state, kDirections[face].name, false);
            }
        }
        if (facesOf(state) == 0) {
            state = registry::blockFluids::hasFluid(at(pos)) ? registry::blocks::water().stateId
                                                             : registry::blocks::air().stateId;
        }
        set(pos, state, true);
    }

    void discharge(Behaviour behaviour, const Pos& pos, std::int32_t state) {
        if (behaviour == Behaviour::Vein) {
            dischargeVein(pos, state);
        }
    }

    /// parity: `SculkVeinBlock.attemptPlaceSculk`.
    bool placeSculk(const Pos& pos) {
        const std::int32_t state = at(pos);
        for (const std::size_t support : shuffledDirections()) {
            if (!hasFace(state, support)) {
                continue;
            }
            const Pos supportPos = offset(pos, support);
            if (!is("minecraft:sculk_replaceable", at(supportPos))) {
                continue;
            }
            const std::int32_t sculk = defaultOf("sculk");
            set(supportPos, sculk, true);
            sound(supportPos, "minecraft:block.sculk.spread");
            spreadAll(sculk, supportPos, false);
            for (std::size_t direction = 0; direction < kDirections.size(); ++direction) {
                if (direction == kDirections[support].opposite) {
                    continue;
                }
                const Pos veinPos = offset(supportPos, direction);
                const std::int32_t vein = at(veinPos);
                if (pathOf(vein) == "sculk_vein") {
                    dischargeVein(veinPos, vein);
                }
            }
            return true;
        }
        return false;
    }

    /// parity: `SculkBlock.canPlaceGrowth`.
    [[nodiscard]] bool canPlaceGrowth(const Pos& pos) const {
        const std::int32_t above = at(offset(pos, 1));
        if (!registry::blocks::isAir(above) && pathOf(above) != "water") {
            return false;
        }
        std::int32_t found = 0;
        for (std::int32_t x = pos[0] - 4; x <= pos[0] + 4; ++x) {
            for (std::int32_t y = pos[1]; y <= pos[1] + 2; ++y) {
                for (std::int32_t z = pos[2] - 4; z <= pos[2] + 4; ++z) {
                    if (is("minecraft:sculk_growth_inhibitors", at({x, y, z})) && ++found > 2) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    std::int32_t sculkUseCharge(const Cursor& cursor, const Pos& origin) {
        const std::int32_t charge = cursor.charge;
        if (charge == 0 || random_->nextInt(kChargeDecayRate) != 0) {
            return charge;
        }
        const Pos& pos = cursor.pos;
        const double dx = pos[0] - origin[0];
        const double dy = pos[1] - origin[1];
        const double dz = pos[2] - origin[2];
        const double distanceSquared = (dx * dx) + (dy * dy) + (dz * dz);
        const bool nearCatalyst = distanceSquared < static_cast<double>(kNoGrowthRadius * kNoGrowthRadius);
        if (!nearCatalyst && canPlaceGrowth(pos)) {
            if (random_->nextInt(kGrowthSpawnCost) < charge) {
                const Pos growthPos = offset(pos, 1);
                // parity: `getRandomGrowthState`, a shrieker that cannot summon one time in eleven.
                const bool shrieker = random_->nextInt(kShriekerRate) == 0;
                std::int32_t growth = defaultOf(shrieker ? "sculk_shrieker" : "sculk_sensor");
                if (registry::blockFluids::hasFluid(at(growthPos))) {
                    growth = withProperty(growth, "waterlogged", true);
                }
                set(growthPos, growth, true);
                sound(pos, shrieker ? "minecraft:block.sculk_shrieker.place" : "minecraft:block.sculk_sensor.place");
            }
            return std::max(0, charge - kGrowthSpawnCost);
        }
        if (random_->nextInt(kAdditionalDecayRate) != 0) {
            return charge;
        }
        if (nearCatalyst) {
            return charge - 1;
        }
        // parity: `getDecayPenalty`, in floats.
        const float outer = static_cast<float>(std::sqrt(distanceSquared)) - static_cast<float>(kNoGrowthRadius);
        const auto reach =
            static_cast<float>((kMaxGrowthRadius - kNoGrowthRadius) * (kMaxGrowthRadius - kNoGrowthRadius));
        const float factor = std::min(1.0F, (outer * outer) / reach);
        return charge - std::max(1, static_cast<std::int32_t>(static_cast<float>(charge) * factor * 0.5F));
    }

    std::int32_t useCharge(Behaviour behaviour, const Cursor& cursor, const Pos& origin) {
        switch (behaviour) {
        case Behaviour::Sculk:
            return sculkUseCharge(cursor, origin);
        case Behaviour::Vein:
            if (placeSculk(cursor.pos)) {
                return cursor.charge - 1;
            }
            return random_->nextInt(kChargeDecayRate) == 0
                       ? static_cast<std::int32_t>(std::floor(static_cast<float>(cursor.charge) * 0.5F))
                       : cursor.charge;
        case Behaviour::Default:
            break;
        }
        return cursor.decayDelay > 0 ? cursor.charge : 0;
    }

    [[nodiscard]] bool unobstructed(const Pos& from, std::size_t direction) const {
        return !registry::blockShapes::isFaceSturdy(at(offset(from, direction)),
                                                    faceOf(kDirections[direction].opposite));
    }

    /// parity: `ChargeCursor.isMovementUnobstructed`.
    [[nodiscard]] bool movementUnobstructed(const Pos& from, const Pos& delta) const {
        if (std::abs(delta[0]) + std::abs(delta[1]) + std::abs(delta[2]) == 1) {
            return true;
        }
        const std::size_t x = delta[0] < 0 ? 4 : 5;
        const std::size_t y = delta[1] < 0 ? 0 : 1;
        const std::size_t z = delta[2] < 0 ? 2 : 3;
        if (delta[0] == 0) {
            return unobstructed(from, y) || unobstructed(from, z);
        }
        if (delta[1] == 0) {
            return unobstructed(from, x) || unobstructed(from, z);
        }
        return unobstructed(from, x) || unobstructed(from, y);
    }

    /// parity: `SculkVeinBlock.hasSubstrateAccess`.
    [[nodiscard]] bool substrateAccess(std::int32_t state, const Pos& pos) const {
        if (pathOf(state) != "sculk_vein") {
            return false;
        }
        for (std::size_t face = 0; face < kDirections.size(); ++face) {
            if (hasFace(state, face) && is("minecraft:sculk_replaceable", at(offset(pos, face)))) {
                return true;
            }
        }
        return false;
    }

    /// parity: `ChargeCursor.getValidMovementPos` over the shuffled non-corner neighbours.
    [[nodiscard]] std::optional<Pos> movementPos(const Pos& pos) {
        static const std::vector<Pos> kNeighbours = [] {
            std::vector<Pos> built;
            for (std::int32_t z = -1; z <= 1; ++z) {
                for (std::int32_t y = -1; y <= 1; ++y) {
                    for (std::int32_t x = -1; x <= 1; ++x) {
                        if ((x == 0 || y == 0 || z == 0) && (x != 0 || y != 0 || z != 0)) {
                            built.push_back({x, y, z});
                        }
                    }
                }
            }
            return built;
        }();
        std::vector<Pos> order = kNeighbours;
        shuffle(order);
        std::optional<Pos> chosen;
        for (const Pos& delta : order) {
            const Pos neighbour = offset(pos, delta);
            const std::int32_t state = at(neighbour);
            if (behaviourOf(state) == Behaviour::Default || !movementUnobstructed(pos, delta)) {
                continue;
            }
            chosen = neighbour;
            if (substrateAccess(state, neighbour)) {
                break;
            }
        }
        return chosen;
    }

    /// parity: `ChargeCursor.update` for the level spreader.
    void updateCursor(Cursor& cursor, const Pos& origin) {
        if (cursor.charge <= 0) {
            return;
        }
        if (cursor.updateDelay > 0) {
            --cursor.updateDelay;
            return;
        }
        std::int32_t state = at(cursor.pos);
        Behaviour behaviour = behaviourOf(state);
        if (attemptSpreadVein(behaviour, cursor.pos, state, cursor.facings)) {
            if (behaviour != Behaviour::Sculk) {
                state = at(cursor.pos);
                behaviour = behaviourOf(state);
            }
            sound(cursor.pos, "minecraft:block.sculk.spread");
        }
        cursor.charge = useCharge(behaviour, cursor, origin);
        if (cursor.charge <= 0) {
            discharge(behaviour, cursor.pos, state);
            return;
        }
        if (const std::optional<Pos> moved = movementPos(cursor.pos); moved.has_value()) {
            discharge(behaviour, cursor.pos, state);
            cursor.pos = *moved;
            state = at(*moved);
        }
        if (behaviourOf(state) != Behaviour::Default) {
            cursor.facings = facesOf(state);
        }
        // parity: the decay and the next delay are asked of where the cursor stood, not where it went.
        cursor.decayDelay = behaviour == Behaviour::Default ? std::max(cursor.decayDelay - 1, 0) : 1;
        cursor.updateDelay = 1;
    }

private:
    WorldState* world_;
    random::RandomSource* random_;
    const BlockTagged* tagged_;
    std::vector<Step>* steps_;
};

[[nodiscard]] std::int32_t chessboard(const Pos& one, const Pos& other) {
    return std::max({std::abs(one[0] - other[0]), std::abs(one[1] - other[1]), std::abs(one[2] - other[2])});
}

} // namespace

std::vector<Cursor> read(const NbtCompound& data) {
    std::vector<Cursor> cursors;
    const NbtList* list = data.getList("cursors");
    // parity: `sizeLimitedListOf(32)`, whose failure reads as no cursors at all.
    if (list == nullptr || list->size() > kMaxCursors) {
        return cursors;
    }
    for (const NbtTag& tag : list->elements()) {
        const auto* entry = tag.get<NbtCompound>();
        const std::span<const std::int32_t> pos =
            entry == nullptr ? std::span<const std::int32_t>{} : entry->getIntArray("pos");
        if (pos.size() != 3) {
            continue;
        }
        Cursor cursor{.pos = {pos[0], pos[1], pos[2]},
                      .charge = std::clamp(entry->getInt("charge", 0), 0, kMaxCharge),
                      .decayDelay = std::clamp(entry->getInt("decay_delay", 1), 0, 1),
                      .updateDelay = std::max(entry->getInt("update_delay", 0), 0)};
        if (const NbtList* facings = entry->getList("facings"); facings != nullptr) {
            std::uint8_t bits = 0;
            for (const NbtTag& facing : facings->elements()) {
                const auto* name = facing.get<std::string>();
                for (std::size_t direction = 0; name != nullptr && direction < kDirections.size(); ++direction) {
                    if (*name == kDirections[direction].name) {
                        bits = static_cast<std::uint8_t>(bits | (1U << direction));
                    }
                }
            }
            cursor.facings = bits;
        }
        if (cursors.size() < kMaxCursors) {
            cursors.push_back(cursor);
        }
    }
    return cursors;
}

void write(const std::vector<Cursor>& cursors, NbtCompound& data) {
    NbtList list{wire::nbt::NbtType::Compound};
    for (const Cursor& cursor : cursors) {
        NbtCompound entry;
        entry.putIntArray("pos", {cursor.pos[0], cursor.pos[1], cursor.pos[2]});
        entry.putInt("charge", cursor.charge);
        entry.putInt("decay_delay", cursor.decayDelay);
        entry.putInt("update_delay", cursor.updateDelay);
        if (cursor.facings.has_value()) {
            NbtList facings{wire::nbt::NbtType::String};
            for (std::size_t direction = 0; direction < kDirections.size(); ++direction) {
                if ((*cursor.facings & (1U << direction)) != 0) {
                    static_cast<void>(facings.push(NbtTag::ofString(std::string{kDirections[direction].name})));
                }
            }
            entry.set("facings", NbtTag::ofList(std::move(facings)));
        }
        static_cast<void>(list.push(NbtTag::ofCompound(std::move(entry))));
    }
    data.set("cursors", NbtTag::ofList(std::move(list)));
}

void addCursors(std::vector<Cursor>& cursors, const Pos& start, std::int32_t charge) {
    while (charge > 0) {
        const std::int32_t current = std::min(charge, kMaxCharge);
        if (cursors.size() < kMaxCursors) {
            cursors.push_back(Cursor{.pos = start, .charge = current});
        }
        charge -= current;
    }
}

void update(WorldState& world, std::vector<Cursor>& cursors, const Pos& origin, random::RandomSource& random,
            const BlockTagged& tagged, std::vector<Step>& steps) {
    if (cursors.empty()) {
        return;
    }
    Spreader spreader{world, random, tagged, steps};
    std::vector<Cursor> processed;
    std::vector<std::pair<Pos, std::size_t>> mergeable;
    std::vector<std::pair<Pos, std::int32_t>> charges;
    for (Cursor cursor : cursors) {
        if (chessboard(cursor.pos, origin) > kMaxCursorDistance) {
            continue;
        }
        spreader.updateCursor(cursor, origin);
        if (cursor.charge <= 0) {
            spreader.event(cursor.pos, 0);
            continue;
        }
        const auto summed = std::ranges::find(charges, cursor.pos, &std::pair<Pos, std::int32_t>::first);
        if (summed == charges.end()) {
            charges.emplace_back(cursor.pos, cursor.charge);
        } else {
            summed->second += cursor.charge;
        }
        const auto existing = std::ranges::find(mergeable, cursor.pos, &std::pair<Pos, std::size_t>::first);
        if (existing == mergeable.end()) {
            mergeable.emplace_back(cursor.pos, processed.size());
            processed.push_back(cursor);
            continue;
        }
        Cursor& into = processed[existing->second];
        if (cursor.charge + into.charge <= kMaxCharge) {
            // parity: `mergeWith`, the charge summed and the sooner of the two delays kept.
            into.charge += cursor.charge;
            into.updateDelay = std::min(into.updateDelay, cursor.updateDelay);
            continue;
        }
        processed.push_back(cursor);
        if (cursor.charge < into.charge) {
            existing->second = processed.size() - 1;
        }
    }
    for (const auto& [pos, charge] : charges) {
        const auto owner = std::ranges::find(mergeable, pos, &std::pair<Pos, std::size_t>::first);
        if (charge <= 0 || owner == mergeable.end() || !processed[owner->second].facings.has_value()) {
            continue;
        }
        // parity: the particle count rides above the six face bits.
        const std::int32_t particles =
            static_cast<std::int32_t>(std::log1p(static_cast<double>(charge)) / static_cast<double>(2.3F)) + 1;
        spreader.event(pos, (particles << 6) + *processed[owner->second].facings);
    }
    cursors = std::move(processed);
}

std::optional<std::int32_t> unbloomed(std::int32_t stateId) {
    if (pathOf(stateId) != "sculk_catalyst") {
        return std::nullopt;
    }
    const std::int32_t off = withProperty(stateId, "bloom", false);
    return off == stateId ? std::nullopt : std::optional{off};
}

std::optional<std::int32_t> bloomed(std::int32_t stateId) {
    if (pathOf(stateId) != "sculk_catalyst") {
        return std::nullopt;
    }
    return withProperty(stateId, "bloom", true);
}

} // namespace basalt::server::runtime::world::sculkSpreading
