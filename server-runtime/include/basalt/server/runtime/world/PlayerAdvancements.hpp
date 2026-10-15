#pragma once

#include <basalt/server/runtime/world/AdvancementTree.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/AdvancementPackets.hpp>

#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::world {

class PlayerProgress;
class RealAdvancements;
class RealEnchantments;

/// parity: `PlayerAdvancements`.
class PlayerAdvancements {
public:
    PlayerAdvancements(const RealAdvancements& advancements, const PlayerProgress& progress,
                       const RealEnchantments* enchantments = nullptr);

    void reload();

    /// parity: loading a player's progress: what they have started is changed and its tree re-evaluated.
    void join(std::string_view uuid);

    void leave(std::string_view uuid);

    void changed(std::string_view uuid, const wire::common::Identifier& advancement);

    /// parity: `flushDirty`. Nothing when there is nothing to say.
    [[nodiscard]] std::optional<wire::protocol::play::UpdateAdvancementsPacket> flush(std::string_view uuid,
                                                                                      bool showAdvancements);

    /// parity: `setSelectedTab`.
    [[nodiscard]] std::optional<wire::protocol::play::SelectAdvancementsTabPacket>
    selectTab(std::string_view uuid, const std::optional<wire::common::Identifier>& tab);

    [[nodiscard]] bool isDone(std::string_view uuid, const wire::common::Identifier& advancement) const;

private:
    struct State {
        std::set<wire::common::Identifier> visible;
        std::set<wire::common::Identifier> progressChanged;
        std::set<wire::common::Identifier> rootsToUpdate;
        std::optional<wire::common::Identifier> selectedTab;
        bool firstPacket = true;
    };

    void updateVisibility(std::string_view uuid, State& state, const wire::common::Identifier& root,
                          std::set<wire::common::Identifier>& added, std::set<wire::common::Identifier>& removed);

    [[nodiscard]] wire::protocol::play::AdvancementEntry entryOf(const wire::common::Identifier& id) const;

    [[nodiscard]] wire::protocol::play::AdvancementProgressData progressOf(std::string_view uuid,
                                                                           const wire::common::Identifier& id) const;

    const RealAdvancements* advancements_;
    const PlayerProgress* progress_;
    const RealEnchantments* enchantments_;
    AdvancementTree tree_;
    // Criteria change on whichever thread fired the trigger; the tick thread flushes.
    mutable std::mutex mutex_;
    std::map<std::string, State, std::less<>> players_;
};

} // namespace basalt::server::runtime::world
