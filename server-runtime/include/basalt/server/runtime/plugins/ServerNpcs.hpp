#pragma once

#include <basalt/server/plugins/PluginWorldAccess.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::runtime::plugins {

class Npc {
public:
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

    [[nodiscard]] std::int32_t version() const noexcept { return version_; }

    [[nodiscard]] bool removed() const noexcept { return removed_; }

    void changed() noexcept { version_++; }

    void remove() noexcept { removed_ = true; }

private:
    std::int32_t version_ = 0;
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

private:
    std::shared_ptr<Npc> npc_;
    NpcRegistry* registry_;
    Profiles profiles_;
    std::shared_ptr<server::plugins::PluginHologramRef> label_;
};

} // namespace basalt::server::runtime::plugins
