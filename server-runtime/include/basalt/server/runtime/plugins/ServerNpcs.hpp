#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins {

struct NpcAttribute {
    double base = 0.0;
    double fallback = 0.0;
    double lowest = 0.0;
    double highest = 0.0;
    std::vector<server::plugins::NpcModifier> modifiers;

    [[nodiscard]] double value() const;
};

class Npc {
public:
    static constexpr std::size_t kItemSlots = 41;
    static constexpr double kDefaultMaxHealth = 20.0;

    Npc(std::int32_t entityId, wire::common::Uuid uuid, std::string name, std::string world, double x, double y,
        double z)
        : entityId(entityId), uuid(uuid), name(std::move(name)), world(std::move(world)), x(x), y(y), z(z) {}

    std::int32_t entityId = 0;
    wire::common::Uuid uuid;
    std::string name;
    std::string world;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    float yaw = 0.0F;
    float pitch = 0.0F;

    std::optional<std::string> skinTexture;
    std::optional<std::string> skinSignature;

    std::optional<std::set<std::string>> visibleTo;

    bool looksAtPlayers = true;

    bool fake = false;
    bool placed = false;
    bool listed = false;
    bool onGround = true;
    float headYaw = 0.0F;
    std::uint8_t flags = 0;
    std::int32_t pose = 0;
    std::vector<entity::GeneratedEquipment> equipment;
    double health = kDefaultMaxHealth;
    std::int32_t held = 0;
    std::array<std::vector<std::uint8_t>, kItemSlots> items;
    std::map<std::string, NpcAttribute, std::less<>> attributes;
    std::vector<std::pair<std::int32_t, std::int32_t>> animations;

    [[nodiscard]] std::int32_t version() const noexcept { return version_; }

    [[nodiscard]] bool removed() const noexcept { return removed_; }

    void changed() noexcept { version_++; }

    [[nodiscard]] std::int32_t stateVersion() const noexcept { return stateVersion_; }

    void stateChanged() noexcept { stateVersion_++; }

    void animate(std::int32_t action) {
        constexpr std::size_t kKeptAnimations = 32;
        if (animations.size() >= kKeptAnimations) {
            animations.erase(animations.begin());
        }
        animations.emplace_back(++animationSeq_, action);
        stateVersion_++;
    }

    void remove() noexcept { removed_ = true; }

private:
    std::int32_t version_ = 0;
    std::int32_t stateVersion_ = 0;
    std::int32_t animationSeq_ = 0;
    bool removed_ = false;
};

class NpcRegistry {
public:
    using OnClick = std::function<void(const std::string& playerUuid)>;

    void add(std::shared_ptr<Npc> npc, OnClick onClick = {});

    void remove(std::int32_t entityId);

    [[nodiscard]] std::vector<std::shared_ptr<Npc>> all() const;

    [[nodiscard]] std::shared_ptr<Npc> of(std::int32_t entityId) const;

    bool clicked(std::int32_t entityId, const std::string& playerUuid);

    void sweep();

    [[nodiscard]] std::vector<std::int32_t> removedSince(const std::set<std::int32_t>& known) const;

private:
    std::map<std::int32_t, std::shared_ptr<Npc>> npcs_;
    std::map<std::int32_t, OnClick> clicks_;
};

class NpcHandle final : public server::plugins::PluginNpcRef {
public:
    static constexpr double kLabelHeight = 2.0;

    using Profiles =
        std::function<std::optional<std::pair<std::string, std::optional<std::string>>>(const std::string& playerName)>;

    NpcHandle(std::shared_ptr<Npc> npc, NpcRegistry& registry, Profiles profiles,
              std::shared_ptr<server::plugins::PluginHologramRef> label)
        : npc_(std::move(npc)), registry_(&registry), profiles_(std::move(profiles)), label_(std::move(label)) {}

    [[nodiscard]] std::int32_t id() const override { return npc_->entityId; }

    [[nodiscard]] std::string uuid() const override { return npc_->uuid.toString(); }

    [[nodiscard]] std::string world() const override { return npc_->world; }

    [[nodiscard]] double x() const override { return npc_->x; }

    [[nodiscard]] double y() const override { return npc_->y; }

    [[nodiscard]] double z() const override { return npc_->z; }

    [[nodiscard]] bool alive() const override { return registry_->of(npc_->entityId) != nullptr; }

    [[nodiscard]] std::string name() const override;
    void setName(std::string_view value) override;

    void skin(std::string_view texture, const std::optional<std::string>& signature) override;

    bool skinFrom(std::string_view playerName) override;

    void look(float yaw, float pitch) override;

    void looksAtPlayers(bool follows) override;

    bool teleport(double x, double y, double z) override;

    void showOnlyTo(const std::vector<std::string>& uuids) override;

    void showToAll() override;

    [[nodiscard]] std::optional<std::vector<std::string>> audience() const override;

    void remove() override;

    void place(double x, double y, double z, float yaw, float pitch, float headYaw, bool onGround) override;

    void posture(std::uint8_t flags, std::int32_t pose) override;

    void listed(bool shown) override;

    [[nodiscard]] double health() const override { return npc_->health; }

    void setHealth(double value) override;

    [[nodiscard]] std::uint8_t flags() const override { return npc_->flags; }

    [[nodiscard]] std::int32_t pose() const override { return npc_->pose; }

    [[nodiscard]] std::vector<std::uint8_t> item(std::int32_t slot) const override;
    void setItem(std::int32_t slot, std::span<const std::uint8_t> stack) override;
    bool setItemById(std::int32_t slot, std::string_view item, std::int32_t count) override;
    void clearItems() override;

    [[nodiscard]] std::int32_t held() const override { return npc_->held; }

    void setHeld(std::int32_t slot) override;

    void registerAttribute(std::string_view id, double fallback, double lowest, double highest) override;
    [[nodiscard]] std::optional<double> attributeBase(std::string_view id) const override;
    void setAttributeBase(std::string_view id, double value) override;
    [[nodiscard]] std::optional<double> attributeValue(std::string_view id) const override;
    [[nodiscard]] std::vector<server::plugins::NpcModifier> attributeModifiers(std::string_view id) const override;
    void addAttributeModifier(std::string_view id, const server::plugins::NpcModifier& modifier) override;
    void removeAttributeModifier(std::string_view id, std::string_view modifierId) override;

    void animate(std::int32_t action) override;

private:
    std::shared_ptr<Npc> npc_;
    NpcRegistry* registry_;
    Profiles profiles_;
    std::shared_ptr<server::plugins::PluginHologramRef> label_;

    void refreshEquipment(std::int32_t equipSlot);
};

} // namespace basalt::server::runtime::plugins
