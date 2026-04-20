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

// Sent again whenever they change: a client told once shows a lamb that never grows and a mob burning without flames.
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

[[nodiscard]] std::string_view pathOf(const std::string_view named) {
    const std::size_t colon = named.find(':');
    return colon == std::string_view::npos ? named : named.substr(colon + 1);
}

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

void boatFrom(const std::string_view path, Seen& line) {
    /// parity: the other edition's own numbering of the woods, which is also the order it added them.
    static constexpr std::array<std::string_view, 11> kWoods{
        "oak", "spruce", "birch", "jungle", "acacia", "dark_oak", "mangrove", "bamboo", "cherry", "pale_oak", "poplar"};
    for (const std::string_view hull : {"_chest_boat", "_chest_raft", "_boat", "_raft"}) {
        if (path.ends_with(hull)) {
            line.type = hull.starts_with("_chest") ? "minecraft:chest_boat" : "minecraft:boat";
            line.look.variant = placeOf(kWoods, path.substr(0, path.size() - hull.size()));
            return;
        }
    }
}

/// What a mob saved with a structure or read back from disk looks like.
void lookFrom(const wire::nbt::NbtCompound& data, const std::string_view path, Seen& line) {
    if (data.getBoolean("Invisible")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Invisible);
    }
    if (data.getBoolean("NoGravity")) {
        line.flags &= ~wire::bedrock::flagBit(wire::bedrock::EntityFlag::HasGravity);
    }
    if (data.getBoolean("NoAI")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::NoAi);
    }
    if (data.getBoolean("Silent")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Silent);
    }
    // An owner is what being tamed is saved as; without the flag a wolf wears no collar there.
    if (data.contains("Owner")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Tamed);
    }
    // 26.2 made a saddle a slot, so a pig is saddled when the slot is filled at all.
    const wire::nbt::NbtCompound* worn = data.getCompound("equipment");
    if (worn != nullptr && worn->contains("saddle")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Saddled);
    }
    if (data.getBoolean("ChestedHorse")) {
        line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Chested);
    }

    if (path == "cushion") {
        // parity: Geyser's reading. The other edition counts the sixteen dyes from the far end.
        static constexpr std::array<std::string_view, 16> kDyes{
            "white",      "orange", "magenta", "light_blue", "yellow", "lime",  "pink", "gray",
            "light_gray", "cyan",   "purple",  "blue",       "brown",  "green", "red",  "black"};
        line.look.variant = 15 - placeOf(kDyes, data.getString("color", "white"));
        return;
    }
    if (path == "sheep") {
        line.look.colour = static_cast<std::int8_t>(data.getByte("Color") & 0x0F);
        if (data.getBoolean("Sheared")) {
            line.flags |= wire::bedrock::flagBit(wire::bedrock::EntityFlag::Sheared);
        }
        return;
    }
    if (path == "rabbit") {
        constexpr std::int32_t kKillerBunny = 99;
        const std::int32_t coat = data.getInt("RabbitType");
        line.look.variant = coat == kKillerBunny ? 1 : coat;
        return;
    }
    if (path == "panda") {
        line.look.variant = idOf(pandaGenes(), data.getString("MainGene"));
        line.look.markVariant = idOf(pandaGenes(), data.getString("HiddenGene"));
        return;
    }
    if (path == "cat") {
        static constexpr std::array<std::string_view, 11> kCoats{
            "white",   "black", "red",       "siamese", "british_shorthair", "calico", "persian",
            "ragdoll", "tabby", "all_black", "jellie"};
        line.look.variant = placeOf(kCoats, data.getString("variant"));
        line.look.colour = static_cast<std::int8_t>(data.getByte("CollarColor", 14) & 0x0F);
        return;
    }
    if (path == "axolotl") {
        const std::int32_t colour = data.getInt("Variant");
        line.look.variant = colour == 1 ? 3 : colour == 3 ? 1 : colour;
        return;
    }
    if (path == "mooshroom") {
        line.look.variant = data.getString("Type") == "brown" ? 1 : 0;
        return;
    }
    if (path == "pufferfish") {
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
        line.look.markVariant = data.getBoolean("HasStung") ? 1 : 0;
        return;
    }
    if (path == "fox") {
        line.look.variant = data.getString("Type") == "snow" ? 1 : 0;
        return;
    }
    if (path == "pig" || path == "cow" || path == "chicken") {
        static constexpr std::array<std::string_view, 3> kClimates{"temperate", "warm", "cold"};
        line.look.climate = placeOf(kClimates, data.getString("variant"));
        return;
    }
    if (path == "wolf") {
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

void paintingFrom(const entity::ServerEntity& painting, Seen& line) {
    /// parity: `PaintingVariant`.
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
    // Fourteen names differ between editions and nothing about a name says which, hence a table.
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
    boatFrom(path, line);
    if (path == "cushion") {
        line.look.variant = 15;
    }
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
        line.name = *entity.customName;
    }
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
