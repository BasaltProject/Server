#include <basalt/server/runtime/command/EntitySelector.hpp>
#include <basalt/server/runtime/command/ExecutionSource.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace basalt::server::runtime::command {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

[[nodiscard]] NbtTag doublesOf(double first, double second, double third) {
    NbtList list{NbtType::Double};
    list.push(NbtTag::ofDouble(first));
    list.push(NbtTag::ofDouble(second));
    list.push(NbtTag::ofDouble(third));
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] NbtTag floatsOf(float first, float second) {
    NbtList list{NbtType::Float};
    list.push(NbtTag::ofFloat(first));
    list.push(NbtTag::ofFloat(second));
    return NbtTag::ofList(std::move(list));
}

template <typename Tags>
[[nodiscard]] NbtTag tagsOf(const Tags& tags) {
    NbtList list{NbtType::String};
    for (const std::string& tag : tags) {
        list.push(NbtTag::ofString(tag));
    }
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] double distanceTo(const CommandTarget& target, double x, double y, double z) {
    const double dx = target.x() - x;
    const double dy = target.y() - y;
    const double dz = target.z() - z;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

} // namespace

namespace entityNbtView {

NbtCompound of(const player::PlayerSession& player) {
    NbtCompound tag;
    tag.putString("id", "minecraft:player");
    tag.putString("Name", player.name());
    tag.putFloat("Health", player.health());
    tag.putInt("XpLevel", player.experience.level());
    tag.putFloat("XpP", player.experience.progress());
    tag.putInt("XpTotal", player.experience.total());
    tag.putString("Dimension", player.worldName);
    tag.set("Pos", doublesOf(player.x, player.y, player.z));
    tag.set("Rotation", floatsOf(player.yaw, player.pitch));
    tag.set("Tags", tagsOf(player.tags));
    tag.putInt("foodLevel", player.food.foodLevel());
    return tag;
}

NbtCompound of(const entity::ServerEntity& entity) {
    NbtCompound tag;
    tag.putString("id", entity.type().toString());
    if (entity.customName.has_value()) {
        tag.putString("CustomName", *entity.customName);
    }
    tag.putFloat("Health", static_cast<float>(entity.health));
    tag.set("Pos", doublesOf(entity.x, entity.y, entity.z));
    tag.set("Rotation", floatsOf(entity.yaw, entity.pitch));
    tag.set("Tags", tagsOf(entity.tags));
    return tag;
}

NbtCompound of(const CommandTarget& target) {
    if (const player::PlayerSession* session = target.player(); session != nullptr) {
        return of(*session);
    }
    if (const entity::ServerEntity* entity = target.entity(); entity != nullptr) {
        return of(*entity);
    }
    // A dropped item carries no NBT this build models, so `nbt=` never matches one.
    return NbtCompound{};
}

} // namespace entityNbtView

EntitySelector::EntitySelector(Parts parts) : parts_(std::move(parts)) {}

std::vector<CommandTarget> EntitySelector::select(const CommandSource& source, const CommandEnvironment& environment,
                                                  bool playersOnly) const {
    const bool withEntities = parts_.includesEntities && !playersOnly;
    if (parts_.playerName.has_value()) {
        std::vector<CommandTarget> named;
        for (player::PlayerSession* player : environment.players().snapshot()) {
            if (player->name().size() == parts_.playerName->size() &&
                std::ranges::equal(player->name(), *parts_.playerName, [](char left, char right) {
                    return std::tolower(static_cast<unsigned char>(left)) ==
                           std::tolower(static_cast<unsigned char>(right));
                })) {
                named.emplace_back(*player);
            }
        }
        return named;
    }

    const auto entitiesIn = [&environment](world::LoadedWorld* only) {
        std::vector<CommandTarget> found;
        std::vector<world::LoadedWorld*> worlds;
        if (only != nullptr) {
            worlds.push_back(only);
        } else {
            for (const auto& [name, world] : environment.options().worlds) {
                worlds.push_back(world);
            }
        }
        for (world::LoadedWorld* loaded : worlds) {
            for (entity::ServerEntity* entity : loaded->state().entities()) {
                if (!entity->removed) {
                    found.emplace_back(*entity, loaded->name());
                }
            }
            // parity: `@e` is every entity, and what is lying on the floor is one of them.
            for (const world::DroppedItem& item : loaded->state().items()) {
                found.emplace_back(item, loaded->name());
            }
        }
        return found;
    };

    if (parts_.entityUuid.has_value()) {
        for (player::PlayerSession* player : environment.players().snapshot()) {
            if (player->uuid().toString() == *parts_.entityUuid) {
                return {CommandTarget{*player}};
            }
        }
        if (!withEntities) {
            return {};
        }
        for (CommandTarget& target : entitiesIn(nullptr)) {
            if (target.uuid() == *parts_.entityUuid) {
                return {target};
            }
        }
        return {};
    }

    const std::array<double, 3> at = source.position();
    const double x = parts_.anchorX.value_or(at[0]);
    const double y = parts_.anchorY.value_or(at[1]);
    const double z = parts_.anchorZ.value_or(at[2]);

    if (parts_.currentEntity) {
        // `/execute as` may have set something that is not a player, and `@s` is whoever it set.
        std::optional<CommandTarget> self;
        if (const ExecutionSource* execution = source.asExecution();
            execution != nullptr && execution->entity().has_value()) {
            self = execution->entity();
        } else if (player::PlayerSession* player = source.player(); player != nullptr) {
            self = CommandTarget{*player};
        }
        if (!self.has_value() || !matches(*self, x, y, z)) {
            return {};
        }
        return {*self};
    }

    world::LoadedWorld* world = parts_.worldLimited ? worldIn(source, environment) : nullptr;
    std::vector<CommandTarget> candidates;
    for (player::PlayerSession* player : environment.players().snapshot()) {
        if (world != nullptr && player->worldName != world->name()) {
            continue;
        }
        candidates.emplace_back(*player);
    }
    if (withEntities) {
        for (CommandTarget& target : entitiesIn(world)) {
            candidates.push_back(std::move(target));
        }
    }
    std::vector<CommandTarget> matched;
    for (CommandTarget& candidate : candidates) {
        if (matches(candidate, x, y, z)) {
            matched.push_back(std::move(candidate));
        }
    }
    return sortAndLimit(std::move(matched), x, y, z);
}

bool EntitySelector::matches(const CommandTarget& target, double x, double y, double z) const {
    if (parts_.typeLimit.has_value() && target.type() != *parts_.typeLimit) {
        return false;
    }
    if (!withinBox(target, x, y, z)) {
        return false;
    }
    if (parts_.distance.has_value() && !parts_.distance->matches(distanceTo(target, x, y, z))) {
        return false;
    }
    return std::ranges::all_of(parts_.predicates, [&target](const Predicate& predicate) { return predicate(target); });
}

bool EntitySelector::withinBox(const CommandTarget& target, double x, double y, double z) const {
    if (!parts_.deltaX.has_value() && !parts_.deltaY.has_value() && !parts_.deltaZ.has_value()) {
        return true;
    }
    // parity: `AABB.intersects`, with the box built by `EntitySelectorParser.createAabb` and moved
    // to the anchor.
    const double dx = parts_.deltaX.value_or(0.0);
    const double dy = parts_.deltaY.value_or(0.0);
    const double dz = parts_.deltaZ.value_or(0.0);
    const double minX = x + (dx < 0.0 ? dx : 0.0);
    const double minY = y + (dy < 0.0 ? dy : 0.0);
    const double minZ = z + (dz < 0.0 ? dz : 0.0);
    const double maxX = x + (dx < 0.0 ? 0.0 : dx) + 1.0;
    const double maxY = y + (dy < 0.0 ? 0.0 : dy) + 1.0;
    const double maxZ = z + (dz < 0.0 ? 0.0 : dz) + 1.0;
    // A target this build has no dimensions for is a point, and vanilla's strict intersection test
    // can never contain one, so it is asked the containment question instead.
    if (target.width() == 0.0 && target.height() == 0.0) {
        return target.x() >= minX && target.x() <= maxX && target.y() >= minY && target.y() <= maxY &&
               target.z() >= minZ && target.z() <= maxZ;
    }
    const double halfWidth = target.width() / 2.0;
    return minX < target.x() + halfWidth && maxX > target.x() - halfWidth && minY < target.y() + target.height() &&
           maxY > target.y() && minZ < target.z() + halfWidth && maxZ > target.z() - halfWidth;
}

std::vector<CommandTarget> EntitySelector::sortAndLimit(std::vector<CommandTarget> matched, double x, double y,
                                                        double z) const {
    if (matched.size() <= 1) {
        return matched;
    }
    switch (parts_.order) {
    case SelectorOrder::Nearest:
        std::ranges::stable_sort(matched, [x, y, z](const CommandTarget& left, const CommandTarget& right) {
            return distanceTo(left, x, y, z) < distanceTo(right, x, y, z);
        });
        break;
    case SelectorOrder::Furthest:
        std::ranges::stable_sort(matched, [x, y, z](const CommandTarget& left, const CommandTarget& right) {
            return distanceTo(left, x, y, z) > distanceTo(right, x, y, z);
        });
        break;
    case SelectorOrder::Random: {
        std::random_device source;
        std::mt19937 shuffler{source()};
        std::ranges::shuffle(matched, shuffler);
        break;
    }
    case SelectorOrder::Arbitrary:
    default:
        break;
    }
    // Erased rather than resized: a target has nothing sensible to be default-built as.
    if (matched.size() > static_cast<std::size_t>(parts_.maxResults)) {
        matched.erase(matched.begin() + parts_.maxResults, matched.end());
    }
    return matched;
}

} // namespace basalt::server::runtime::command
