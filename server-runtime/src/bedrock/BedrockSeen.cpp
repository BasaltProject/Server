#include <basalt/server/runtime/bedrock/BedrockSeen.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::bedrock {
namespace {

/// What a mob is doing, as the flags that edition draws it by. Sent again whenever they change: a
/// client that is only ever told once shows a lamb that never grows and a mob that burns without
/// flames.
[[nodiscard]] std::uint64_t flagsOf(const entity::ServerEntity& mob) {
    std::uint64_t flags = wire::bedrock::flagBit(wire::bedrock::EntityFlag::HasCollision) |
                          wire::bedrock::flagBit(wire::bedrock::EntityFlag::HasGravity) |
                          wire::bedrock::flagBit(wire::bedrock::EntityFlag::Breathing) |
                          wire::bedrock::flagBit(wire::bedrock::EntityFlag::CanShowName);
    if (mob.fireTicks > 0) {
        flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::OnFire);
    }
    // parity: `AgeableMob.age`, where anything below zero is still a baby.
    if (mob.age < 0) {
        flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Baby);
    }
    if (mob.sitting) {
        flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sitting);
    }
    return flags;
}

/// The name a saved variant carries without its namespace, since every one of these is vanilla's.
[[nodiscard]] std::string_view pathOf(const std::string_view named) {
    const std::size_t colon = named.find(':');
    return colon == std::string_view::npos ? named : named.substr(colon + 1);
}

/// Where a name sits in a list, which for these is the number the other edition draws.
[[nodiscard]] std::int32_t placeOf(const std::span<const std::string_view> order, const std::string_view named) {
    const auto found = std::ranges::find(order, pathOf(named));
    return found == order.end() ? 0 : static_cast<std::int32_t>(found - order.begin());
}

/// parity: the seven a panda can carry, in the order both editions number them.
[[nodiscard]] std::int32_t idOf(const std::array<std::string_view, 7>& names, const std::string_view name) {
    const auto found = std::ranges::find(names, name);
    return found == names.end() ? 0 : static_cast<std::int32_t>(found - names.begin());
}

[[nodiscard]] const std::array<std::string_view, 7>& pandaGenes() {
    static constexpr std::array<std::string_view, 7> kGenes{"normal", "lazy", "worried",   "playful",
                                                            "brown",  "weak", "aggressive"};
    return kGenes;
}

/// parity: Geyser's own table. The two editions number a villager's trade in different orders, so
/// the name is looked up here and the number that comes out is the one the other edition draws.
[[nodiscard]] std::int32_t villagerVariantOf(const std::string_view profession) {
    static constexpr std::array<std::pair<std::string_view, std::int32_t>, 15> kTrades{{
        {"none", 0},
        {"armorer", 8},
        {"butcher", 11},
        {"cartographer", 6},
        {"cleric", 7},
        {"farmer", 1},
        {"fisherman", 2},
        {"fletcher", 4},
        {"leatherworker", 12},
        {"librarian", 5},
        {"mason", 13},
        {"nitwit", 14},
        {"shepherd", 3},
        {"toolsmith", 10},
        {"weaponsmith", 9},
    }};
    const auto found = std::ranges::find(kTrades, profession, &std::pair<std::string_view, std::int32_t>::first);
    return found == kTrades.end() ? 0 : found->second;
}

/// What a mob saved with a structure or read back from disk looks like. parity: the other door
/// reads the same tags in `generatedEntityMetadata`; only the numbers they come out as differ.
void lookFrom(const wire::nbt::NbtCompound& data, const std::string_view path, Seen& line) {
    // The three every mob can carry, whatever it is.
    if (data.getBoolean("Invisible")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Invisible);
    }
    if (data.getBoolean("NoGravity")) {
        line.flags &= ~wire::bedrock::flagBit(wire::bedrock::EntityFlag::HasGravity);
    }
    if (data.getBoolean("NoAI")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::NoAi);
    }

    if (path == "sheep") {
        // The dye is numbered the same on both editions; being shorn is a flag here and a bit of
        // the same byte over there.
        line.look.colour = static_cast<std::int8_t>(data.getByte("Color") & 0x0F);
        if (data.getBoolean("Sheared")) {
            line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sheared);
        }
        return;
    }
    if (path == "rabbit") {
        /// parity: the killer bunny is a coat the other edition has no drawing for, so it is drawn
        /// white like Geyser draws it.
        constexpr std::int32_t kKillerBunny = 99;
        const std::int32_t coat = data.getInt("RabbitType");
        line.look.variant = coat == kKillerBunny ? 1 : coat;
        return;
    }
    if (path == "panda") {
        // The seven genes are in the same order on both, so the number travels as it is.
        line.look.variant = idOf(pandaGenes(), data.getString("MainGene"));
        line.look.markVariant = idOf(pandaGenes(), data.getString("HiddenGene"));
        return;
    }
    if (path == "wolf") {
        // parity: 26.2 moved a wolf's coat into a datapack registry, so what is saved is a name.
        // The two editions list the nine in different orders, and this one draws the place in its
        // own list.
        static constexpr std::array<std::string_view, 9> kCoats{"pale",  "ashen",   "black",   "chestnut", "rusty",
                                                                "snowy", "spotted", "striped", "woods"};
        line.look.variant = placeOf(kCoats, data.getString("variant"));
        return;
    }
    if (path == "frog") {
        static constexpr std::array<std::string_view, 3> kFrogs{"temperate", "cold", "warm"};
        line.look.variant = placeOf(kFrogs, data.getString("variant"));
        return;
    }
    if (path == "goat") {
        // parity: the other edition names each horn and this one only counts them.
        line.look.horns =
            (data.getBoolean("HasLeftHorn", true) ? 1 : 0) + (data.getBoolean("HasRightHorn", true) ? 1 : 0);
        return;
    }
    if (path == "horse") {
        // One number over there holds the coat in its low byte and the markings in the next.
        const std::int32_t coat = data.getInt("Variant");
        line.look.variant = coat & 0xFF;
        line.look.markVariant = (coat >> 8) % 5;
        return;
    }
    if (path == "llama" || path == "trader_llama") {
        line.look.variant = data.getInt("Variant");
        line.look.strength = data.getInt("Strength");
        return;
    }
    if (path == "tropical_fish") {
        // parity: Geyser's own unpacking. The other edition packs shape, pattern and two colours
        // into one number; this one keeps the four apart, and each is clamped to what it can draw.
        const std::int32_t packed = data.getInt("Variant");
        const auto dye = [](const std::int32_t value) {
            return value >= 0 && value <= 15 ? static_cast<std::int8_t>(value) : std::int8_t{0};
        };
        line.look.variant = std::min(packed & 0xFF, 1);
        line.look.markVariant = std::min((packed >> 8) & 0xFF, 5);
        line.look.colour = dye((packed >> 16) & 0xFF);
        line.look.colour2 = dye((packed >> 24) & 0xFF);
        return;
    }
    if (path == "slime" || path == "magma_cube") {
        /// parity: Geyser's own formula, where a size of nought is still a mob you can see.
        constexpr float kSmallest = 0.10F;
        line.look.scale = kSmallest + static_cast<float>(data.getInt("Size"));
    }
}

} // namespace

Seen seenOf(const entity::ServerEntity& entity) {
    // Nearly every name is the same on both; the fourteen that are not are a table of their own,
    // because nothing about a name says which kind it is.
    Seen line{.entityId = entity.entityId(),
              .type = std::string{wire::bedrock::bedrockEntityName(entity.type().toString())},
              .x = entity.x,
              .y = entity.y,
              .z = entity.z,
              .yaw = entity.yaw,
              .pitch = entity.pitch,
              .width = static_cast<float>(entity.width()),
              .height = static_cast<float>(entity.height()),
              .stack = std::nullopt,
              .flags = flagsOf(entity)};

    const std::string_view path = entity.type().path();
    if (path == "villager" || path == "zombie_villager") {
        line.look.variant = villagerVariantOf(entity.profession);
    }
    if (entity.sourceNbt.has_value()) {
        lookFrom(*entity.sourceNbt, path, line);
    }
    if (entity.customName.has_value()) {
        // Drawn when it is looked at, as vanilla draws a name nobody made visible.
        line.name = *entity.customName;
    }
    // parity: Geyser draws a text display as an armour stand nobody can see with the words over it,
    // because that edition has no display entity of its own.
    if (entity.displayText.has_value()) {
        line.type = "minecraft:armor_stand";
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Invisible) |
                      wire::bedrock::flagBit(wire::bedrock::EntityFlag::NoAi);
        line.width = 0.0F;
        line.height = 0.0F;
        line.name = wire::text::toLegacyText(wire::text::miniMessage::parse(*entity.displayText));
        line.alwaysShowName = true;
    }
    return line;
}

} // namespace basalt::server::runtime::bedrock
