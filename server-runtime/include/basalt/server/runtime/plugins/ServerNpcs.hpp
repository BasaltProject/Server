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

/// A player-shaped entity a plugin put in the world.
///
/// A real player entity: the client draws it because it was told about a player, with a profile and
/// a skin, not because of any trick. That is the whole reason this is a handful of fields rather
/// than a library: being the server means there is nothing to fake and no client version to detect.
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

    /// The skin, as the two strings a profile property carries. Unset draws the default skin.
    std::optional<std::string> skinTexture;
    std::optional<std::string> skinSignature;

    /// Who can see it, by uuid, or nothing for everybody. Same rule as a hologram's.
    std::optional<std::set<std::string>> visibleTo;

    /// Whether its head turns toward whoever comes near. Off is a statue, which is sometimes right.
    bool looksAtPlayers = true;

    [[nodiscard]] std::int32_t version() const noexcept { return version_; }

    [[nodiscard]] bool removed() const noexcept { return removed_; }

    /// Bumped whenever something a client has already been told changes, so the sync re-sends it.
    void changed() noexcept { version_++; }

    void remove() noexcept { removed_ = true; }

private:
    std::int32_t version_ = 0;
    bool removed_ = false;
};

/// Every NPC on the server, and who is allowed to be told about them.
///
/// One registry rather than one per world because a connection asks it once per tick and a map of
/// maps would only move the filtering around.
class NpcRegistry {
public:
    using OnClick = std::function<void(const std::string& playerUuid)>;

    void add(std::shared_ptr<Npc> npc, OnClick onClick = {});

    void remove(std::int32_t entityId);

    [[nodiscard]] std::vector<std::shared_ptr<Npc>> all() const;

    [[nodiscard]] std::shared_ptr<Npc> of(std::int32_t entityId) const;

    /// Answers whether anybody was listening, so a click on an NPC is not also a click on nothing.
    bool clicked(std::int32_t entityId, const std::string& playerUuid);

    /// Forgets the ones that were removed, after the connections have been told.
    void sweep();

    [[nodiscard]] std::vector<std::int32_t> removedSince(const std::set<std::int32_t>& known) const;

private:
    std::map<std::int32_t, std::shared_ptr<Npc>> npcs_;
    std::map<std::int32_t, OnClick> clicks_;
};

/// What a plugin holds onto after making one.
class NpcHandle final : public server::plugins::PluginNpcRef {
public:
    /// Just over a player's head, which is 1.8 blocks tall.
    static constexpr double kLabelHeight = 2.0;

    /// Where a skin is copied from: the profiles of whoever is connected.
    using Profiles =
        std::function<std::optional<std::pair<std::string, std::optional<std::string>>>(const std::string& playerName)>;

    /// `label` is the text over its head, as a text display rather than as its profile name: a
    /// profile name is sixteen plain characters, so it holds neither colour nor a second line.
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
