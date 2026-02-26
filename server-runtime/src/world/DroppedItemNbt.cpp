#include <basalt/server/runtime/world/DroppedItemNbt.hpp>

#include <string>

namespace basalt::server::runtime::world::droppedItemNbt {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;

constexpr std::string_view kType = "minecraft:item";

/// parity: `Entity.saveWithoutId` writes both as a list of three doubles.
[[nodiscard]] NbtTag doubles(double one, double two, double three) {
    NbtList list{NbtType::Double};
    list.push(NbtTag::ofDouble(one));
    list.push(NbtTag::ofDouble(two));
    list.push(NbtTag::ofDouble(three));
    return NbtTag::ofList(std::move(list));
}

[[nodiscard]] double at(const NbtList* list, std::size_t index) {
    if (list == nullptr || list->size() <= index) {
        return 0.0;
    }
    const double* value = list->elements()[index].get<double>();
    return value == nullptr ? 0.0 : *value;
}

} // namespace

NbtCompound encode(const DroppedItem& item) {
    NbtCompound stack;
    stack.putString("id", item.item.toString());
    stack.putInt("count", item.count);
    if (!item.components.empty()) {
        stack.set("components", NbtTag::ofCompound(item.components));
    }

    NbtCompound tag;
    tag.putString("id", std::string{kType});
    tag.putString("UUID", item.uuid.toString());
    tag.set("Pos", doubles(item.x, item.y, item.z));
    tag.set("Motion", doubles(item.velocityX, item.velocityY, item.velocityZ));
    tag.putShort("Health", static_cast<std::int16_t>(item.health));
    tag.putShort("Age", static_cast<std::int16_t>(item.age));
    tag.putShort("PickupDelay", static_cast<std::int16_t>(item.pickupDelay));
    tag.set("Item", NbtTag::ofCompound(std::move(stack)));
    return tag;
}

bool isItem(const NbtCompound& tag) { return tag.getString("id") == kType; }

std::optional<DroppedItem> decode(const NbtCompound& tag, const std::function<std::int32_t()>& allocateEntityId) {
    if (!isItem(tag)) {
        return std::nullopt;
    }
    const NbtCompound* stack = tag.getCompound("Item");
    if (stack == nullptr) {
        return std::nullopt;
    }
    const std::expected<wire::common::Identifier, wire::common::IdentifierError> item =
        wire::common::Identifier::parse(stack->getString("id"));
    if (!item.has_value()) {
        return std::nullopt;
    }
    const NbtList* pos = tag.getList("Pos");
    const NbtList* motion = tag.getList("Motion");
    DroppedItem dropped{.entityId = allocateEntityId(),
                        .uuid = wire::common::Uuid::parse(tag.getString("UUID")).value_or(wire::common::Uuid::random()),
                        .item = *item,
                        .count = stack->getInt("count", 1),
                        .x = at(pos, 0),
                        .y = at(pos, 1),
                        .z = at(pos, 2),
                        .velocityX = at(motion, 0),
                        .velocityY = at(motion, 1),
                        .velocityZ = at(motion, 2)};
    dropped.pickupDelay = tag.getShort("PickupDelay", 0);
    dropped.age = tag.getShort("Age", 0);
    dropped.health = tag.getShort("Health", static_cast<std::int16_t>(kItemHealth));
    if (const NbtCompound* components = stack->getCompound("components"); components != nullptr) {
        dropped.components = *components;
    }
    return dropped;
}

} // namespace basalt::server::runtime::world::droppedItemNbt
