#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/bedrock/BedrockSeen.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/bedrock/BedrockEntityIdentifiers.hpp>
#include <basalt/wire/bedrock/BedrockItems.hpp>
#include <basalt/wire/bedrock/BedrockPackets.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>
#include <basalt/wire/text/MiniMessage.hpp>
#include <basalt/wire/text/Serializers.hpp>

#include <algorithm>
#include <array>
#include <cmath>
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
    if (path == "cat") {
        // parity: the two editions list the eleven coats in different orders, and the collar is a
        // dye number that travels as it is. Fourteen is the red every cat is tamed with.
        static constexpr std::array<std::string_view, 11> kCoats{
            "white",   "black", "red",       "siamese", "british_shorthair", "calico", "persian",
            "ragdoll", "tabby", "all_black", "jellie"};
        line.look.variant = placeOf(kCoats, data.getString("variant"));
        line.look.colour = static_cast<std::int8_t>(data.getByte("CollarColor", 14) & 0x0F);
        return;
    }
    if (path == "axolotl") {
        // parity: the two editions agree on every colour but two, which they have the other way
        // round: the wild brown one and the cyan one.
        const std::int32_t colour = data.getInt("Variant");
        line.look.variant = colour == 1 ? 3 : colour == 3 ? 1 : colour;
        return;
    }
    if (path == "mooshroom") {
        // The brown one is the second on both editions, and the red one is what a bad name means.
        line.look.variant = data.getString("Type") == "brown" ? 1 : 0;
        return;
    }
    if (path == "pufferfish") {
        // How far it has puffed up, counted the same on both.
        line.look.variant = data.getInt("PuffState");
        return;
    }
    if (path == "turtle") {
        // parity: the other edition draws a turtle carrying an egg as a pregnant one.
        if (data.getBoolean("has_egg")) {
            line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::IsPregnant);
        }
        return;
    }
    if (path == "bee") {
        // parity: Geyser draws the sting it already used as a mark, which is the only one of a
        // bee's three states that survives being saved. Its anger is an hour of the world's clock
        // rather than a countdown, and nectar is a property rather than a look.
        line.look.markVariant = data.getBoolean("HasStung") ? 1 : 0;
        return;
    }
    if (path == "fox") {
        // The only one that is not the red fox is the snowy one, and it is the second on both.
        line.look.variant = data.getString("Type") == "snow" ? 1 : 0;
        return;
    }
    if (path == "pig" || path == "cow" || path == "chicken") {
        // parity: the three the other edition gave a climate to in 26.2. This one keeps it as a
        // property of the entity rather than a field of its metadata.
        static constexpr std::array<std::string_view, 3> kClimates{"temperate", "warm", "cold"};
        line.look.climate = placeOf(kClimates, data.getString("variant"));
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

/// An item frame as the other edition holds one: a block on a wall with what it carries inside.
void frameFrom(const entity::ServerEntity& hung, const bool glowing, Seen& line) {
    Seen::Frame frame;
    frame.glowing = glowing;
    frame.x = static_cast<std::int32_t>(std::floor(hung.x));
    frame.y = static_cast<std::int32_t>(std::floor(hung.y));
    frame.z = static_cast<std::int32_t>(std::floor(hung.z));
    if (hung.sourceNbt.has_value()) {
        const wire::nbt::NbtCompound& data = *hung.sourceNbt;
        frame.face = data.getByte("Facing", 3);
        frame.rotation = data.getByte("ItemRotation", 0);
        if (const wire::nbt::NbtCompound* held = data.getCompound("Item"); held != nullptr) {
            const wire::bedrock::ItemName* named = wire::bedrock::bedrockItem(held->getString("id"));
            if (named != nullptr) {
                frame.item = wire::bedrock::bedrockItemPalette()[static_cast<std::size_t>(named->palettePlace)];
                frame.count = held->getByte("count", 1);
            }
        }
    }
    line.frame = std::move(frame);
}

/// A painting as the other edition hangs one: a picture by name, a way of facing in its own
/// numbering, and the middle of the picture rather than the middle of the block it hangs on.
void paintingFrom(const entity::ServerEntity& painting, Seen& line) {
    /// parity: `PaintingVariant`. This server's registry holds the one, two blocks by two, so both
    /// halves of the offset are half a block; a picture of another size would want its own.
    constexpr double kHalf = 0.5;

    line.motive = "humble";
    const std::int8_t face =
        painting.sourceNbt.has_value() ? painting.sourceNbt->getByte("Facing", 3) : static_cast<std::int8_t>(3);
    // parity: the two editions number the four walls differently, and this one starts at south.
    switch (static_cast<server::registry::BlockFace>(face)) {
    case server::registry::BlockFace::North:
        line.facing = 2;
        line.x -= kHalf;
        break;
    case server::registry::BlockFace::West:
        line.facing = 1;
        line.z += kHalf;
        break;
    case server::registry::BlockFace::East:
        line.facing = 3;
        line.z -= kHalf;
        break;
    case server::registry::BlockFace::South:
    case server::registry::BlockFace::Down:
    case server::registry::BlockFace::Up:
        line.facing = 0;
        line.x += kHalf;
        break;
    }
    line.y += kHalf;
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
    if (path == "painting") {
        paintingFrom(entity, line);
    }
    if (path == "item_frame" || path == "glow_item_frame") {
        frameFrom(entity, path == "glow_item_frame", line);
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
