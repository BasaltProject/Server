#include <basalt/server/registry/Items.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <utility>

namespace basalt::server::runtime::entity::copperGolems {
namespace {

using server::world::loot::ItemStack;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

constexpr std::array<std::string_view, 4> kWeatherNames{"unaffected", "exposed", "weathered", "oxidized"};
constexpr std::string_view kWeatherKey = "weather_state";
constexpr std::string_view kNextWeatherKey = "next_weather_age";
/// parity: `Mob`'s guaranteed-drop chance, anything over one.
constexpr float kGuaranteedDrop = 2.0F;

[[nodiscard]] bool endsWith(std::string_view text, std::string_view suffix) { return text.ends_with(suffix); }

} // namespace

bool isCopperGolem(const wire::common::Identifier& type) {
    return type == wire::common::Identifier::vanilla("copper_golem");
}

GolemState& stateOf(ServerEntity& golem, random::RandomSource& random) {
    if (golem.copperGolem == nullptr) {
        golem.copperGolem = std::make_shared<GolemState>();
        GolemState& state = *golem.copperGolem;
        // parity: the constructor's `nextInt(60, 100)`.
        state.transportCooldown = random.nextInt(kSpawnCooldownMin, kSpawnCooldownMax);
        if (golem.sourceNbt.has_value()) {
            state.nextWeatheringTick = golem.sourceNbt->getLong(kNextWeatherKey, kUnset);
            const std::string_view name = golem.sourceNbt->getString(kWeatherKey, kWeatherNames[0]);
            for (std::size_t index = 0; index < kWeatherNames.size(); ++index) {
                if (kWeatherNames[index] == name) {
                    state.weather = static_cast<Weather>(index);
                }
            }
        }
    }
    return *golem.copperGolem;
}

void save(ServerEntity& golem) {
    if (golem.copperGolem == nullptr) {
        return;
    }
    if (!golem.sourceNbt.has_value()) {
        golem.sourceNbt.emplace();
    }
    golem.sourceNbt->putLong(kNextWeatherKey, golem.copperGolem->nextWeatheringTick);
    golem.sourceNbt->putString(kWeatherKey, std::string{weatherName(golem.copperGolem->weather)});
}

std::string_view weatherName(Weather weather) { return kWeatherNames[static_cast<std::size_t>(weather)]; }

Weather next(Weather weather) {
    return weather == Weather::Oxidized ? weather : static_cast<Weather>(static_cast<std::uint8_t>(weather) + 1);
}

Weather previous(Weather weather) {
    return weather == Weather::Unaffected ? weather : static_cast<Weather>(static_cast<std::uint8_t>(weather) - 1);
}

Weather weatherOfBlock(std::string_view path) {
    if (path.starts_with("waxed_")) {
        path.remove_prefix(6);
    }
    if (path.starts_with("exposed_")) {
        return Weather::Exposed;
    }
    if (path.starts_with("weathered_")) {
        return Weather::Weathered;
    }
    if (path.starts_with("oxidized_")) {
        return Weather::Oxidized;
    }
    return Weather::Unaffected;
}

std::string_view spinSound(Weather weather) {
    switch (weather) {
    case Weather::Weathered:
        return "entity.copper_golem_weathered.spin";
    case Weather::Oxidized:
        return "entity.copper_golem_oxidized.spin";
    default:
        return "entity.copper_golem.spin";
    }
}

std::string_view hurtSound(Weather weather) {
    switch (weather) {
    case Weather::Weathered:
        return "entity.copper_golem_weathered.hurt";
    case Weather::Oxidized:
        return "entity.copper_golem_oxidized.hurt";
    default:
        return "entity.copper_golem.hurt";
    }
}

std::string_view deathSound(Weather weather) {
    switch (weather) {
    case Weather::Weathered:
        return "entity.copper_golem_weathered.death";
    case Weather::Oxidized:
        return "entity.copper_golem_oxidized.death";
    default:
        return "entity.copper_golem.death";
    }
}

bool isSourceChest(std::string_view path) { return endsWith(path, "copper_chest"); }

bool isDestinationChest(std::string_view path) { return path == "chest" || path == "trapped_chest"; }

bool isChest(std::string_view path) { return isSourceChest(path) || isDestinationChest(path); }

std::optional<ItemStack> mainHand(const ServerEntity& golem) {
    const NbtCompound* equipment = golem.sourceNbt.has_value() ? golem.sourceNbt->getCompound("equipment") : nullptr;
    const NbtCompound* held = equipment == nullptr ? nullptr : equipment->getCompound("mainhand");
    return held == nullptr ? std::nullopt : world::blockInventory::readStack(*held);
}

void setMainHand(ServerEntity& golem, const std::optional<ItemStack>& stack, bool guaranteed) {
    if (!golem.sourceNbt.has_value()) {
        golem.sourceNbt.emplace();
    }
    NbtCompound equipment;
    if (const NbtCompound* existing = golem.sourceNbt->getCompound("equipment"); existing != nullptr) {
        equipment = *existing;
    }
    if (stack.has_value() && stack->count > 0) {
        equipment.set("mainhand", NbtTag::ofCompound(world::blockInventory::writeStack(*stack)));
    } else {
        static_cast<void>(equipment.remove("mainhand"));
    }
    golem.sourceNbt->set("equipment", NbtTag::ofCompound(std::move(equipment)));
    if (guaranteed) {
        NbtCompound chances;
        if (const NbtCompound* existing = golem.sourceNbt->getCompound("drop_chances"); existing != nullptr) {
            chances = *existing;
        }
        chances.putFloat("mainhand", kGuaranteedDrop);
        golem.sourceNbt->set("drop_chances", NbtTag::ofCompound(std::move(chances)));
    }
}

bool isEmpty(const Slots& slots) {
    return std::ranges::none_of(slots, [](const auto& slot) { return slot.has_value() && slot->count > 0; });
}

bool holdsSameItem(const Slots& slots, const wire::common::Identifier& item) {
    return std::ranges::any_of(
        slots, [&item](const auto& slot) { return slot.has_value() && slot->count > 0 && slot->item == item; });
}

std::optional<ItemStack> pickUp(Slots& slots) {
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value() || slot->count <= 0) {
            continue;
        }
        const std::int32_t taken = std::min(slot->count, kMaxCarried);
        ItemStack carried = *slot;
        carried.count = taken;
        slot->count -= taken;
        if (slot->count <= 0) {
            slot.reset();
        }
        return carried;
    }
    return std::nullopt;
}

std::optional<ItemStack> putDown(Slots& slots, ItemStack held) {
    for (std::optional<ItemStack>& slot : slots) {
        if (!slot.has_value() || slot->count <= 0) {
            slot = std::move(held);
            return std::nullopt;
        }
        const std::int32_t most = server::registry::itemStacking::maxStackSize(slot->item);
        if (slot->item == held.item && slot->components == held.components && slot->count < most) {
            const std::int32_t room = most - slot->count;
            slot->count += std::min(room, held.count);
            // parity: the jar takes `room` off the hand, not what it moved.
            held.count -= room;
            if (held.count <= 0) {
                return std::nullopt;
            }
        }
    }
    return held;
}

} // namespace basalt::server::runtime::entity::copperGolems
