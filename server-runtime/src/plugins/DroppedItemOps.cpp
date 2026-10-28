#include <basalt/server/runtime/plugins/DroppedItemOps.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>

#include <algorithm>
#include <string>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginEntityRef;
using server::plugins::PluginEntityValue;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;
using world::DroppedItem;
using Value = PluginEntityValue;

constexpr double kItemSize = 0.25;

[[nodiscard]] std::optional<DroppedItem> find(world::LoadedWorld& world, std::int32_t entityId) {
    return world.state().itemById(entityId);
}

[[nodiscard]] NbtCompound encode(const DroppedItem& item) {
    NbtCompound out;
    out.putString("id", "minecraft:item");
    NbtCompound stack;
    stack.putString("id", item.item.toString());
    stack.putInt("count", item.count);
    if (!item.components.empty()) {
        stack.set("components", NbtTag::ofCompound(item.components));
    }
    out.set("Item", NbtTag::ofCompound(std::move(stack)));
    out.putShort("PickupDelay", static_cast<std::int16_t>(item.pickupDelay));
    out.putShort("Age", static_cast<std::int16_t>(item.age));
    out.putShort("Health", static_cast<std::int16_t>(item.health));
    if (item.thrower.has_value()) {
        out.putString("Thrower", item.thrower->toString());
    }
    out.putShort("CanMobPickup", item.mobsMayPickUp ? 1 : 0);
    out.putShort("CanPlayerPickup", item.playersMayPickUp ? 1 : 0);
    out.putShort("FrictionState", item.frictionState);
    return out;
}

void apply(DroppedItem& item, const NbtCompound& patch) {
    if (const NbtCompound* stack = patch.getCompound("Item"); stack != nullptr) {
        const auto id = wire::common::Identifier::parse(std::string{stack->getString("id")});
        if (id.has_value()) {
            item.item = *id;
        }
        item.count = std::max(1, stack->getInt("count", item.count));
        if (const NbtCompound* components = stack->getCompound("components"); components != nullptr) {
            item.components = *components;
        } else {
            item.components = NbtCompound{};
        }
        item.countChanged = true;
    }
    if (patch.contains("PickupDelay")) {
        item.pickupDelay = patch.getShort("PickupDelay");
    }
    if (patch.contains("Age")) {
        item.age = patch.getShort("Age");
    }
    if (patch.contains("Health")) {
        item.health = patch.getShort("Health");
    }
    if (patch.contains("CanMobPickup")) {
        item.mobsMayPickUp = patch.getShort("CanMobPickup") != 0;
    }
    if (patch.contains("CanPlayerPickup")) {
        item.playersMayPickUp = patch.getShort("CanPlayerPickup") != 0;
    }
    if (patch.contains("FrictionState")) {
        item.frictionState = static_cast<std::int8_t>(std::clamp<std::int16_t>(patch.getShort("FrictionState"), 0, 2));
    }
    if (patch.contains("Thrower")) {
        item.thrower = wire::common::Uuid::parse(std::string{patch.getString("Thrower")});
    }
}

class DroppedItemRef final : public PluginEntityRef {
public:
    DroppedItemRef(world::LoadedWorld& world, std::int32_t entityId) : world_(&world), entityId_(entityId) {}

    [[nodiscard]] std::int32_t id() const override { return entityId_; }

    [[nodiscard]] std::string type() const override { return "minecraft:item"; }

    [[nodiscard]] std::string world() const override { return world_->name(); }

    [[nodiscard]] double x() const override { return current().value_or(DroppedItem{}).x; }

    [[nodiscard]] double y() const override { return current().value_or(DroppedItem{}).y; }

    [[nodiscard]] double z() const override { return current().value_or(DroppedItem{}).z; }

    [[nodiscard]] double health() const override {
        return static_cast<double>(current().value_or(DroppedItem{.health = 0}).health);
    }

    void setHealth(double value) override {
        edit([value](DroppedItem& item) { item.health = static_cast<std::int32_t>(value); });
    }

    [[nodiscard]] bool alive() const override { return current().has_value(); }

    void remove() override { static_cast<void>(world_->state().removeItem(entityId_)); }

    bool teleport(double x, double y, double z) override {
        return edit([x, y, z](DroppedItem& item) {
            item.x = x;
            item.y = y;
            item.z = z;
        });
    }

    [[nodiscard]] std::string uuid() const override {
        const std::optional<DroppedItem> item = current();
        return item.has_value() ? item->uuid.toString() : std::string{};
    }

    [[nodiscard]] std::array<double, 3> velocity() const override {
        const std::optional<DroppedItem> item = current();
        return item.has_value() ? std::array<double, 3>{item->velocityX, item->velocityY, item->velocityZ}
                                : std::array<double, 3>{};
    }

    bool setVelocity(double x, double y, double z) override {
        return edit([x, y, z](DroppedItem& item) {
            item.velocityX = x;
            item.velocityY = y;
            item.velocityZ = z;
            item.resting = false;
        });
    }

    [[nodiscard]] bool onGround() const override { return current().value_or(DroppedItem{}).onGround; }

    [[nodiscard]] std::int32_t age() const override { return current().value_or(DroppedItem{}).age; }

    [[nodiscard]] double width() const override { return kItemSize; }

    [[nodiscard]] double height() const override { return kItemSize; }

    [[nodiscard]] std::optional<Value> property(std::string_view name) const override {
        const std::optional<DroppedItem> item = current();
        if (!item.has_value()) {
            return std::nullopt;
        }
        if (name == "ticks_lived") {
            return static_cast<std::int64_t>(item->age);
        }
        if (name == "fall_distance") {
            return item->fallDistance;
        }
        if (name == "passengers") {
            return std::vector<std::string>{};
        }
        if (name == "vehicle") {
            return static_cast<std::int64_t>(-1);
        }
        if (name == "tags") {
            return std::vector<std::string>{};
        }
        if (name == "persistent" || name == "invulnerable" || name == "glowing" || name == "silent" ||
            name == "no_gravity" || name == "custom_name_visible") {
            return false;
        }
        return std::nullopt;
    }

    std::optional<Value> invoke(std::string_view operation, const std::vector<Value>& arguments) override {
        const std::optional<DroppedItem> item = current();
        if (!item.has_value()) {
            return std::nullopt;
        }
        if (operation == "nbt") {
            const auto* path = arguments.empty() ? nullptr : std::get_if<std::string>(&arguments[0]);
            NbtCompound whole = encode(*item);
            if (path != nullptr && !path->empty()) {
                const NbtTag* inner = whole.find(*path);
                const NbtCompound* compound = inner == nullptr ? nullptr : inner->get<NbtCompound>();
                if (compound == nullptr) {
                    return Value{};
                }
                whole = *compound;
            }
            const std::vector<std::uint8_t> bytes = wire::nbt::nbtIO::write(NbtTag::ofCompound(std::move(whole)));
            return std::string{bytes.begin(), bytes.end()};
        }
        if (operation == "nbt_value") {
            const auto* path = arguments.empty() ? nullptr : std::get_if<std::string>(&arguments[0]);
            const NbtCompound whole = encode(*item);
            const NbtTag* found = path == nullptr ? nullptr : whole.find(*path);
            if (found == nullptr) {
                return Value{};
            }
            if (const auto* number = found->get<std::int16_t>(); number != nullptr) {
                return static_cast<std::int64_t>(*number);
            }
            if (const auto* text = found->get<std::string>(); text != nullptr) {
                return *text;
            }
            return Value{};
        }
        if (operation == "merge_nbt" || operation == "nbt_set") {
            NbtCompound patch;
            if (operation == "merge_nbt") {
                const auto* bytes = arguments.empty() ? nullptr : std::get_if<std::string>(&arguments[0]);
                if (bytes == nullptr) {
                    return false;
                }
                const std::vector<std::uint8_t> raw{bytes->begin(), bytes->end()};
                const auto parsed = wire::nbt::nbtIO::read(raw);
                const NbtCompound* compound = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
                if (compound == nullptr) {
                    return false;
                }
                patch = *compound;
            } else if (arguments.size() >= 3) {
                const auto* path = std::get_if<std::string>(&arguments[0]);
                if (path == nullptr) {
                    return false;
                }
                if (const auto* text = std::get_if<std::string>(&arguments[2]); text != nullptr) {
                    patch.putString(*path, *text);
                } else if (const auto* whole = std::get_if<std::int64_t>(&arguments[2]); whole != nullptr) {
                    patch.putShort(*path, static_cast<std::int16_t>(*whole));
                } else if (const auto* flag = std::get_if<bool>(&arguments[2]); flag != nullptr) {
                    patch.putShort(*path, *flag ? 1 : 0);
                } else {
                    return false;
                }
            }
            return edit([&patch](DroppedItem& target) { apply(target, patch); });
        }
        if (operation == "push" && arguments.size() >= 3) {
            return edit([&arguments](DroppedItem& target) {
                const auto get = [&arguments](std::size_t at) {
                    const auto* real = std::get_if<double>(&arguments[at]);
                    return real == nullptr ? 0.0 : *real;
                };
                target.velocityX += get(0);
                target.velocityY += get(1);
                target.velocityZ += get(2);
                target.resting = false;
            });
        }
        return std::nullopt;
    }

private:
    [[nodiscard]] std::optional<DroppedItem> current() const { return find(*world_, entityId_); }

    template <typename Change>
    bool edit(Change&& change) {
        std::optional<DroppedItem> taken = world_->state().removeItem(entityId_);
        if (!taken.has_value()) {
            return false;
        }
        change(*taken);
        world_->state().spawnItem(std::move(*taken));
        return true;
    }

    world::LoadedWorld* world_;
    std::int32_t entityId_;
};

} // namespace

std::shared_ptr<PluginEntityRef> droppedItemRef(world::LoadedWorld& world, std::int32_t entityId) {
    return find(world, entityId).has_value() ? std::make_shared<DroppedItemRef>(world, entityId) : nullptr;
}

std::vector<std::shared_ptr<PluginEntityRef>> droppedItemRefs(world::LoadedWorld& world) {
    std::vector<std::shared_ptr<PluginEntityRef>> refs;
    for (const DroppedItem& item : world.state().items()) {
        refs.push_back(std::make_shared<DroppedItemRef>(world, item.entityId));
    }
    return refs;
}

std::int32_t dropItem(world::LoadedWorld& world, std::int32_t entityId, std::span<const std::uint8_t> stack, double x,
                      double y, double z, double velocityX, double velocityY, double velocityZ) {
    const auto parsed = wire::nbt::nbtIO::read(stack);
    const NbtCompound* compound = parsed.has_value() ? parsed->tag.get<NbtCompound>() : nullptr;
    if (compound == nullptr) {
        return -1;
    }
    const std::optional<server::world::loot::ItemStack> read = world::blockInventory::readStack(*compound);
    if (!read.has_value() || read->count <= 0) {
        return -1;
    }
    DroppedItem dropped;
    dropped.entityId = entityId;
    dropped.uuid = wire::common::Uuid::random();
    dropped.item = read->item;
    dropped.count = read->count;
    dropped.components = read->components;
    dropped.x = x;
    dropped.y = y;
    dropped.z = z;
    dropped.velocityX = velocityX;
    dropped.velocityY = velocityY;
    dropped.velocityZ = velocityZ;
    dropped.pickupDelay = world::kSpawnedPickupDelay;
    world.state().spawnItem(std::move(dropped));
    return entityId;
}

} // namespace basalt::server::runtime::plugins
