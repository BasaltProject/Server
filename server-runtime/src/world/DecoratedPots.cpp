#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/DecoratedPots.hpp>

#include <array>
#include <cstdint>

namespace basalt::server::runtime::world::decoratedPots {
namespace {

/// parity: `PotDecorations` is four sides, back, left, right and front.
constexpr std::size_t kSides = 4;

/// parity: `DecoratedPotBlockEntity.WobbleStyle`, which travels as the second byte.
constexpr std::int32_t kWobblePositive = 0;
constexpr std::int32_t kWobbleNegative = 1;

/// parity: `Item.getMaxStackSize`, which this build does not read yet.
constexpr std::int32_t kMaxStack = 64;

} // namespace

const wire::common::Identifier& block() {
    static const wire::common::Identifier instance = wire::common::Identifier::vanilla("decorated_pot");
    return instance;
}

bool isPot(std::string_view path) { return path == block().path(); }

std::optional<server::world::loot::ItemStack> contents(const wire::nbt::NbtCompound& data) {
    return blockInventory::readFor(block().path(), data, kSize)[0];
}

std::optional<std::vector<BlockEffect>> insert(wire::nbt::NbtCompound& data, const wire::common::Identifier& item) {
    blockInventory::Slots slots = blockInventory::readFor(block().path(), data, kSize);
    const std::optional<server::world::loot::ItemStack>& held = slots[0];
    if (held.has_value() && (held->item != item || held->count >= kMaxStack)) {
        return std::nullopt;
    }
    slots[0] = held.has_value() ? server::world::loot::ItemStack{.item = held->item, .count = held->count + 1}
                                : server::world::loot::ItemStack{.item = item, .count = 1};
    blockInventory::writeFor(block().path(), data, slots);
    // parity: the pitch rises with how full it is, which is what makes filling one audible.
    const float bend = static_cast<float>(slots[0]->count) / static_cast<float>(kMaxStack);
    return std::vector<BlockEffect>{blockEffect::Event{.b0 = 1, .b1 = kWobblePositive},
                                    blockEffect::Sound{.name = "block.decorated_pot.insert",
                                                       .source = wire::protocol::play::SoundSource::Blocks,
                                                       .volume = 1.0F,
                                                       .pitch = 0.7F + (0.5F * bend)}};
}

std::vector<server::world::loot::ItemStack> sherds(const wire::nbt::NbtCompound& data) {
    std::vector<server::world::loot::ItemStack> out;
    const auto add = [&out](std::string_view name) {
        if (const auto parsed = wire::common::Identifier::parse(name); parsed.has_value()) {
            out.push_back(server::world::loot::ItemStack{.item = *parsed, .count = 1});
        }
    };
    if (const wire::nbt::NbtCompound* sides = data.getCompound("sherds"); sides != nullptr) {
        for (const std::string_view side : {"left", "back", "front", "right"}) {
            if (const wire::nbt::NbtCompound* template_ = sides->getCompound(side); template_ != nullptr) {
                add(template_->getString("id"));
            } else if (const std::string_view name = sides->getString(side); !name.empty()) {
                add(name);
            }
        }
        return out;
    }
    const wire::nbt::NbtList* stored = data.getList("sherds");
    if (stored == nullptr) {
        return out;
    }
    // A 26.2 list is back, left, right, front; 26.3 drops in the order left, back, front, right.
    static constexpr std::array<std::size_t, kSides> kNewOrder{1, 0, 3, 2};
    for (const std::size_t index : kNewOrder) {
        if (index < stored->size()) {
            if (const std::string* name = (*stored)[index].get<std::string>(); name != nullptr) {
                add(*name);
            }
        }
    }
    return out;
}

void decorate(wire::nbt::NbtCompound& data, const wire::nbt::NbtCompound& components) {
    if (const wire::nbt::NbtTag* decorations = components.find("minecraft:pot_decorations"); decorations != nullptr) {
        data.set("sherds", *decorations);
    }
}

std::vector<BlockEffect> rebuff() {
    return {blockEffect::Event{.b0 = 1, .b1 = kWobbleNegative},
            blockEffect::Sound{.name = "block.decorated_pot.insert_fail",
                               .source = wire::protocol::play::SoundSource::Blocks,
                               .volume = 1.0F,
                               .pitch = 1.0F}};
}

std::optional<server::world::loot::ItemStack> takeOne(wire::nbt::NbtCompound& data) {
    blockInventory::Slots slots = blockInventory::readFor(block().path(), data, kSize);
    if (!slots[0].has_value()) {
        return std::nullopt;
    }
    const server::world::loot::ItemStack held = *slots[0];
    slots[0] = held.count <= 1
                   ? std::nullopt
                   : std::optional{server::world::loot::ItemStack{.item = held.item, .count = held.count - 1}};
    blockInventory::writeFor(block().path(), data, slots);
    return server::world::loot::ItemStack{.item = held.item, .count = 1};
}

} // namespace basalt::server::runtime::world::decoratedPots
