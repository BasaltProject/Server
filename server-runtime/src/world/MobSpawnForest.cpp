#include <basalt/server/runtime/world/MobSpawnForest.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>

namespace basalt::server::runtime::world {
namespace {

using server::world::ChunkGeneratedEntity;

} // namespace

MobSpawnNode::MobSpawnNode(wire::common::Identifier type, double x, double y, double z, float yaw, float pitch,
                           wire::nbt::NbtCompound data)
    : type_(std::move(type)), x_(x), y_(y), z_(z), yaw_(yaw), pitch_(pitch), data_(std::move(data)) {}

bool MobSpawnNode::hasVehicle(const MobSpawnNode* candidate) const {
    for (const MobSpawnNode* current = vehicle_; current != nullptr; current = current->vehicle_) {
        if (current == candidate) {
            return true;
        }
    }
    return false;
}

MobSpawnForestBuilder::MobSpawnForestBuilder(MobSpawnEnvironment environment) : environment_(std::move(environment)) {}

MobSpawnNode& MobSpawnForestBuilder::create(const wire::common::Identifier& type, double x, double y, double z,
                                            float yaw, float pitch, wire::nbt::NbtCompound data) {
    nodes_.emplace_back(type, x, y, z, yaw, pitch, std::move(data));
    return nodes_.back();
}

bool MobSpawnForestBuilder::mount(MobSpawnNode& passenger, MobSpawnNode& vehicle) {
    if (&passenger == &vehicle || vehicle.hasVehicle(&passenger)) {
        return false;
    }
    if (passenger.vehicle_ == &vehicle) {
        return true;
    }
    // parity: `Entity.startRiding` detaches the old vehicle after its checks have passed.
    if (passenger.vehicle_ != nullptr) {
        std::erase(passenger.vehicle_->passengers_, &passenger);
    }
    passenger.vehicle_ = &vehicle;
    passenger.externalVehicleId_.reset();
    vehicle.passengers_.push_back(&passenger);
    return true;
}

bool MobSpawnForestBuilder::mountExternal(MobSpawnNode& passenger, std::int32_t vehicleId) {
    if (passenger.vehicle_ != nullptr) {
        std::erase(passenger.vehicle_->passengers_, &passenger);
    }
    passenger.vehicle_ = nullptr;
    passenger.externalVehicleId_ = vehicleId;
    return true;
}

MobSpawnForest MobSpawnForestBuilder::freeze(const MobSpawnNode& main) const {
    // The descriptor each node became, so the external links and the main entity can be found again
    // once the tree has been copied into values.
    std::map<const MobSpawnNode*, ChunkGeneratedEntity> descriptors;
    const auto freezeNode = [&descriptors](const MobSpawnNode& node, const auto& self) -> ChunkGeneratedEntity {
        ChunkGeneratedEntity descriptor{.type = node.type(),
                                        .x = node.x(),
                                        .y = node.y(),
                                        .z = node.z(),
                                        .yaw = node.yaw(),
                                        .pitch = node.pitch(),
                                        .data = node.data()};
        for (const MobSpawnNode* passenger : node.passengers()) {
            descriptor.passengers.push_back(self(*passenger, self));
        }
        descriptors.insert_or_assign(&node, descriptor);
        return descriptor;
    };

    MobSpawnForest forest;
    for (const MobSpawnNode& node : nodes_) {
        if (node.vehicle() == nullptr) {
            forest.roots.push_back(freezeNode(node, freezeNode));
        }
    }
    for (const MobSpawnNode& node : nodes_) {
        if (node.externalVehicleId().has_value()) {
            const auto found = descriptors.find(&node);
            if (found != descriptors.end()) {
                forest.externalVehicleLinks.push_back({found->second, *node.externalVehicleId()});
            }
        }
    }
    if (const auto found = descriptors.find(&main); found != descriptors.end()) {
        forest.main = found->second;
    }
    return forest;
}

MobSpawnForest relocateMain(const MobSpawnForest& forest, double x, double y, double z, float yaw) {
    // The main entity is found by value: it is one of the descriptors in the tree, and moving it
    // must move the copy that is really in the forest rather than a look-alike.
    const auto moveTree = [&](const ChunkGeneratedEntity& entity, const auto& self) -> ChunkGeneratedEntity {
        ChunkGeneratedEntity copied = entity;
        copied.passengers.clear();
        for (const ChunkGeneratedEntity& passenger : entity.passengers) {
            copied.passengers.push_back(self(passenger, self));
        }
        if (entity == forest.main) {
            copied.x = x;
            copied.y = y;
            copied.z = z;
            copied.yaw = yaw;
        }
        return copied;
    };

    MobSpawnForest moved;
    moved.main = forest.main;
    moved.main.x = x;
    moved.main.y = y;
    moved.main.z = z;
    moved.main.yaw = yaw;
    for (const ChunkGeneratedEntity& root : forest.roots) {
        moved.roots.push_back(moveTree(root, moveTree));
    }
    for (const MobExternalVehicleLink& link : forest.externalVehicleLinks) {
        moved.externalVehicleLinks.push_back(
            {link.passenger == forest.main ? moved.main : link.passenger, link.vehicleId});
    }
    return moved;
}

} // namespace basalt::server::runtime::world
