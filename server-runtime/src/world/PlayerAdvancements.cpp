#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/item/ItemComponentCodecs.hpp>
#include <basalt/server/runtime/world/PlayerAdvancements.hpp>
#include <basalt/server/runtime/world/PlayerProgress.hpp>
#include <basalt/server/runtime/world/RealEnchantments.hpp>
#include <basalt/wire/nbt/JsonToNbt.hpp>

#include <algorithm>

namespace basalt::server::runtime::world {
namespace {

namespace adv = server::world::advancement;
namespace play = wire::protocol::play;

using wire::common::Identifier;

/// parity: `AdvancementVisibilityEvaluator.VisibilityRule`.
enum class Rule : std::uint8_t { Show, Hide, NoChange };

/// parity: `VISIBILITY_DEPTH`.
constexpr std::size_t kVisibilityDepth = 2;

[[nodiscard]] std::int32_t frameOf(std::string_view type) {
    if (type == "challenge") {
        return 1;
    }
    return type == "goal" ? 2 : 0;
}

} // namespace

PlayerAdvancements::PlayerAdvancements(const RealAdvancements& advancements, const PlayerProgress& progress,
                                       const RealEnchantments* enchantments)
    : advancements_(&advancements), progress_(&progress), enchantments_(enchantments), tree_(advancements) {}

void PlayerAdvancements::reload() {
    const std::scoped_lock lock{mutex_};
    tree_ = AdvancementTree{*advancements_};
    std::vector<std::string> online;
    for (const auto& [uuid, state] : players_) {
        online.push_back(uuid);
    }
    players_.clear();
    for (const std::string& uuid : online) {
        State& state = players_[uuid];
        for (const Identifier& id : progress_->started(uuid)) {
            if (tree_.get(id) != nullptr) {
                state.progressChanged.insert(id);
                state.rootsToUpdate.insert(tree_.rootOf(id));
            }
        }
    }
}

void PlayerAdvancements::join(std::string_view uuid) {
    const std::scoped_lock lock{mutex_};
    State state;
    for (const Identifier& id : progress_->started(uuid)) {
        if (tree_.get(id) != nullptr) {
            state.progressChanged.insert(id);
            state.rootsToUpdate.insert(tree_.rootOf(id));
        }
    }
    players_.insert_or_assign(std::string{uuid}, std::move(state));
}

void PlayerAdvancements::leave(std::string_view uuid) {
    const std::scoped_lock lock{mutex_};
    if (const auto found = players_.find(uuid); found != players_.end()) {
        players_.erase(found);
    }
}

void PlayerAdvancements::changed(std::string_view uuid, const Identifier& advancement) {
    const std::scoped_lock lock{mutex_};
    const auto found = players_.find(uuid);
    if (found == players_.end() || tree_.get(advancement) == nullptr) {
        return;
    }
    found->second.progressChanged.insert(advancement);
    found->second.rootsToUpdate.insert(tree_.rootOf(advancement));
}

bool PlayerAdvancements::isDone(std::string_view uuid, const Identifier& advancement) const {
    const adv::Advancement* definition = advancements_->find(advancement);
    return definition != nullptr && definition->isComplete(progress_->grantedCriteria(uuid, advancement));
}

void PlayerAdvancements::updateVisibility(std::string_view uuid, State& state, const Identifier& root,
                                          std::set<Identifier>& added, std::set<Identifier>& removed) {
    // parity: `AdvancementVisibilityEvaluator.evaluateVisibility`.
    std::vector<Rule> stack(kVisibilityDepth + 1, Rule::NoChange);
    const auto visit = [&](const auto& self, const Identifier& id) -> bool {
        const adv::Advancement* definition = advancements_->find(id);
        const bool selfDone = isDone(uuid, id);
        Rule rule = Rule::Hide;
        if (definition != nullptr && definition->display.has_value()) {
            rule = selfDone ? Rule::Show : (definition->display->hidden ? Rule::Hide : Rule::NoChange);
        }
        bool doneBelow = selfDone;
        stack.push_back(rule);
        if (const AdvancementTree::Node* node = tree_.get(id); node != nullptr) {
            for (const Identifier& child : node->children) {
                doneBelow |= self(self, child);
            }
        }
        bool visible = doneBelow;
        if (!visible) {
            for (std::size_t depth = 0; depth <= kVisibilityDepth; ++depth) {
                const Rule above = stack[stack.size() - 1 - depth];
                if (above != Rule::NoChange) {
                    visible = above == Rule::Show;
                    break;
                }
            }
        }
        stack.pop_back();
        if (visible) {
            if (state.visible.insert(id).second) {
                added.insert(id);
                state.progressChanged.insert(id);
            }
        } else if (state.visible.erase(id) != 0) {
            removed.insert(id);
        }
        return doneBelow;
    };
    visit(visit, root);
}

play::AdvancementEntry PlayerAdvancements::entryOf(const Identifier& id) const {
    const adv::Advancement* definition = advancements_->find(id);
    const AdvancementTree::Node* node = tree_.get(id);
    play::AdvancementEntry entry{.id = id.toString()};
    if (definition == nullptr || node == nullptr) {
        return entry;
    }
    if (definition->parent.has_value()) {
        entry.parent = definition->parent->toString();
    }
    entry.requirements = definition->requirements.groups;
    entry.sendsTelemetryEvent = definition->sendsTelemetryEvent;
    entry.x = node->x;
    entry.y = node->y;
    if (!definition->display.has_value()) {
        return entry;
    }
    const adv::AdvancementDisplay& display = *definition->display;
    play::WireItemStack icon;
    if (const registry::Item* item = registry::items::registry().byName(display.icon.item); item != nullptr) {
        icon.itemId = item->protocolId;
    }
    icon.count = display.icon.count;
    if (display.icon.components.has_value()) {
        const auto converted = wire::nbt::jsonToNbt(*display.icon.components);
        const auto* compound = converted.has_value() ? converted->get<wire::nbt::NbtCompound>() : nullptr;
        if (compound != nullptr) {
            wire::nbt::NbtCompound named;
            for (std::size_t index = 0; index < compound->names().size(); ++index) {
                const auto key = Identifier::parse(compound->names()[index]);
                named.set(key.has_value() ? key->toString() : compound->names()[index], compound->values()[index]);
            }
            const RealEnchantments* enchantments = enchantments_;
            icon.components = item::itemComponentCodecs::toWire(named, [enchantments](const Identifier& name) {
                return enchantments == nullptr ? std::nullopt : enchantments->protocolIdOf(name);
            });
        }
    }
    entry.display = play::AdvancementDisplayData{.title = display.title,
                                                 .description = display.description,
                                                 .icon = std::move(icon),
                                                 .frame = frameOf(display.type),
                                                 .background = display.background,
                                                 .showToast = display.showToast,
                                                 .hidden = display.hidden};
    return entry;
}

play::AdvancementProgressData PlayerAdvancements::progressOf(std::string_view uuid, const Identifier& id) const {
    // parity: `AdvancementProgress.update`.
    play::AdvancementProgressData data{.id = id.toString()};
    const adv::Advancement* definition = advancements_->find(id);
    if (definition == nullptr) {
        return data;
    }
    const std::map<std::string, std::int64_t, std::less<>> obtained = progress_->obtained(uuid, id);
    for (const std::vector<std::string>& group : definition->requirements.groups) {
        for (const std::string& criterion : group) {
            if (std::ranges::any_of(data.criteria, [&criterion](const auto& one) { return one.first == criterion; })) {
                continue;
            }
            const auto found = obtained.find(criterion);
            data.criteria.emplace_back(criterion,
                                       found == obtained.end() ? std::nullopt : std::optional{found->second});
        }
    }
    return data;
}

std::optional<play::UpdateAdvancementsPacket> PlayerAdvancements::flush(std::string_view uuid, bool showAdvancements) {
    const std::scoped_lock lock{mutex_};
    const auto found = players_.find(uuid);
    if (found == players_.end()) {
        return std::nullopt;
    }
    State& state = found->second;
    std::optional<play::UpdateAdvancementsPacket> packet;
    if (state.firstPacket || !state.rootsToUpdate.empty() || !state.progressChanged.empty()) {
        std::set<Identifier> added;
        std::set<Identifier> removed;
        const std::set<Identifier> roots = std::move(state.rootsToUpdate);
        state.rootsToUpdate.clear();
        for (const Identifier& root : roots) {
            updateVisibility(uuid, state, root, added, removed);
        }
        play::UpdateAdvancementsPacket built{.reset = state.firstPacket, .showAdvancements = showAdvancements};
        for (const Identifier& id : state.progressChanged) {
            if (state.visible.contains(id)) {
                built.progress.push_back(progressOf(uuid, id));
            }
        }
        state.progressChanged.clear();
        // Parents before children, as the tree was built, so the client can place each one.
        for (const Identifier& id : tree_.order()) {
            if (added.contains(id)) {
                built.added.push_back(entryOf(id));
            }
        }
        for (const Identifier& id : removed) {
            built.removed.push_back(id.toString());
        }
        if (!built.progress.empty() || !built.added.empty() || !built.removed.empty()) {
            packet = std::move(built);
        }
    }
    state.firstPacket = false;
    return packet;
}

std::optional<play::SelectAdvancementsTabPacket> PlayerAdvancements::selectTab(std::string_view uuid,
                                                                               const std::optional<Identifier>& tab) {
    const std::scoped_lock lock{mutex_};
    const auto found = players_.find(uuid);
    if (found == players_.end()) {
        return std::nullopt;
    }
    std::optional<Identifier> selected;
    if (tab.has_value()) {
        const adv::Advancement* definition = advancements_->find(*tab);
        if (definition != nullptr && !definition->parent.has_value() && definition->display.has_value()) {
            selected = tab;
        }
    }
    if (selected == found->second.selectedTab) {
        return std::nullopt;
    }
    found->second.selectedTab = selected;
    return play::SelectAdvancementsTabPacket{.tab = selected.has_value() ? std::optional{selected->toString()}
                                                                         : std::nullopt};
}

} // namespace basalt::server::runtime::world
