#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>

#include <cmath>

namespace basalt::server::runtime::world::itemFrames {
namespace {

/// parity: `ItemFrame.NUM_ROTATIONS`.
constexpr std::int32_t kRotations = 8;

/// parity: `createBoundingBox`'s own `0.46875`, which is what puts the frame against the wall rather
/// than in the middle of its square.
constexpr double kAgainstTheWall = 0.46875;

} // namespace

bool isFrame(const wire::common::Identifier& type) {
    return type.path() == "item_frame" || type.path() == "glow_item_frame";
}

bool canHang(const WorldState& world, const std::int32_t x, const std::int32_t y, const std::int32_t z,
             const server::registry::BlockFace facing) {
    // parity: `calculateSupportBox` is the frame's own box pushed half a block back, which lands in
    // the square behind it, and every block in it has to be solid or a diode.
    const std::optional<std::int32_t> behind =
        world.blockState(x - registry::stepX(facing), y - registry::stepY(facing), z - registry::stepZ(facing));
    if (!behind.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*behind);
    const std::string_view path = block == nullptr ? std::string_view{} : block->path();
    const bool holds = registry::blockShapes::isSolidState(*behind) || path == "repeater" || path == "comparator";
    if (!holds) {
        return false;
    }
    // parity: `hasLevelCollision(getPopBox())`, and the pop box lies inside the frame's own square.
    const std::optional<std::int32_t> here = world.blockState(x, y, z);
    if (!here.has_value() || !registry::blockShapes::collisionBoxes(*here).empty()) {
        return false;
    }
    // parity: `canCoexist(false)`, which refuses another hanging entity of the same kind and any
    // hanging entity facing the same way.
    for (const entity::ServerEntity* other : world.entities()) {
        if (other == nullptr || !isFrame(other->type()) || other->removed) {
            continue;
        }
        const std::array<std::int32_t, 3> square = squareOf(*other);
        if (square != std::array<std::int32_t, 3>{x, y, z}) {
            continue;
        }
        return false;
    }
    return true;
}

bool survives(const WorldState& world, const entity::ServerEntity& frame) {
    const wire::nbt::NbtCompound* data = frame.sourceNbt.has_value() ? &*frame.sourceNbt : nullptr;
    if (data != nullptr && data->getBoolean("Fixed")) {
        return true;
    }
    const auto facing = static_cast<registry::BlockFace>(data == nullptr ? 0 : data->getByte("Facing", 0));
    const std::array<std::int32_t, 3> square = squareOf(frame);
    const std::optional<std::int32_t> here = world.blockState(square[0], square[1], square[2]);
    if (!here.has_value() || !registry::blockShapes::collisionBoxes(*here).empty()) {
        return false;
    }
    const std::optional<std::int32_t> behind = world.blockState(
        square[0] - registry::stepX(facing), square[1] - registry::stepY(facing), square[2] - registry::stepZ(facing));
    if (!behind.has_value()) {
        return true;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*behind);
    const std::string_view path = block == nullptr ? std::string_view{} : block->path();
    const bool horizontal = facing != registry::BlockFace::Up && facing != registry::BlockFace::Down;
    if (!registry::blockShapes::isSolidState(*behind) &&
        !(horizontal && (path == "repeater" || path == "comparator"))) {
        return false;
    }
    // parity: `canCoexist(true)`: only another hanging entity facing the same way is in the way.
    for (const entity::ServerEntity* other : world.entities()) {
        if (other == nullptr || other == &frame || other->removed || !isFrame(other->type()) ||
            squareOf(*other) != square) {
            continue;
        }
        const std::int32_t otherFacing = other->sourceNbt.has_value() ? other->sourceNbt->getByte("Facing", 0) : 0;
        if (otherFacing == static_cast<std::int32_t>(facing)) {
            return false;
        }
    }
    return true;
}

wire::nbt::NbtCompound freshTag(const server::registry::BlockFace facing) {
    wire::nbt::NbtCompound tag;
    tag.putByte("Facing", static_cast<std::int8_t>(facing));
    tag.putByte("ItemRotation", 0);
    tag.putFloat("ItemDropChance", 1.0F);
    tag.putBoolean("Invisible", false);
    tag.putBoolean("Fixed", false);
    return tag;
}

std::array<double, 3> hangingAt(const std::int32_t x, const std::int32_t y, const std::int32_t z,
                                const server::registry::BlockFace facing) {
    return {x + 0.5 - (registry::stepX(facing) * kAgainstTheWall),
            y + 0.5 - (registry::stepY(facing) * kAgainstTheWall),
            z + 0.5 - (registry::stepZ(facing) * kAgainstTheWall)};
}

std::array<std::int32_t, 3> squareOf(const entity::ServerEntity& frame) {
    return {static_cast<std::int32_t>(std::floor(frame.x)), static_cast<std::int32_t>(std::floor(frame.y)),
            static_cast<std::int32_t>(std::floor(frame.z))};
}

std::optional<server::world::loot::ItemStack> shownIn(const entity::ServerEntity& frame) {
    const wire::nbt::NbtCompound* data = frame.sourceNbt.has_value() ? &*frame.sourceNbt : nullptr;
    const wire::nbt::NbtCompound* held = data == nullptr ? nullptr : data->getCompound("Item");
    return held == nullptr ? std::nullopt : blockInventory::readStack(*held);
}

std::int32_t rotationOf(const entity::ServerEntity& frame) {
    return frame.sourceNbt.has_value() ? frame.sourceNbt->getByte("ItemRotation", 0) : 0;
}

Touched interact(entity::ServerEntity& frame, const std::optional<server::world::loot::ItemStack>& held) {
    if (!isFrame(frame.type())) {
        return Touched::Nothing;
    }
    if (!frame.sourceNbt.has_value()) {
        frame.sourceNbt = wire::nbt::NbtCompound{};
    }
    wire::nbt::NbtCompound& data = *frame.sourceNbt;
    // parity: a fixed frame passes the click through, which is what a map wall in a structure does.
    if (data.getBoolean("Fixed")) {
        return Touched::Nothing;
    }
    if (data.getCompound("Item") == nullptr) {
        if (!held.has_value()) {
            return Touched::Nothing;
        }
        // parity: `copyWithCount(1)`, so a stack of sixty-four leaves one behind the glass.
        server::world::loot::ItemStack one = *held;
        one.count = 1;
        data.set("Item", wire::nbt::NbtTag::ofCompound(blockInventory::writeStack(one)));
        return Touched::Took;
    }
    // parity: `setRotation(getRotation() + 1)`, which is taken modulo eight.
    data.putByte("ItemRotation", static_cast<std::int8_t>((rotationOf(frame) + 1) % kRotations));
    return Touched::Rotated;
}

std::optional<std::int32_t> analogOutput(const WorldState& world, const std::int32_t x, const std::int32_t y,
                                         const std::int32_t z, const registry::BlockFace facing) {
    std::optional<std::int32_t> reading;
    std::int32_t seen = 0;
    for (const entity::ServerEntity* frame : world.entities()) {
        if (frame == nullptr || !isFrame(frame->type())) {
            continue;
        }
        // parity: the box `getItemFrame` searches is the square itself, and a frame always hangs
        // inside the square it occupies rather than on its edge.
        if (static_cast<std::int32_t>(std::floor(frame->x)) != x ||
            static_cast<std::int32_t>(std::floor(frame->y)) != y ||
            static_cast<std::int32_t>(std::floor(frame->z)) != z) {
            continue;
        }
        const wire::nbt::NbtCompound* data = frame->sourceNbt.has_value() ? &*frame->sourceNbt : nullptr;
        // parity: `Facing` is `Direction.LEGACY_ID_CODEC`, the same order the block faces are in,
        // and a frame that never said which way it hangs hangs downwards.
        const std::int32_t hangs = data == nullptr ? 0 : data->getByte("Facing", 0);
        if (hangs != static_cast<std::int32_t>(facing)) {
            continue;
        }
        ++seen;
        // parity: `getAnalogOutput`, which is the rotation plus one, and zero for an empty frame.
        const wire::nbt::NbtCompound* held = data == nullptr ? nullptr : data->getCompound("Item");
        const std::int32_t rotation = data == nullptr ? 0 : data->getByte("ItemRotation", 0);
        reading = held == nullptr ? 0 : (rotation % kRotations) + 1;
    }
    // parity: `getItemFrame` answers only when the square holds exactly one frame facing that way.
    return seen == 1 ? reading : std::nullopt;
}

} // namespace basalt::server::runtime::world::itemFrames
