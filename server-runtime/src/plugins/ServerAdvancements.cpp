#include <basalt/server/runtime/plugins/ServerAdvancements.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>

namespace basalt::server::runtime::plugins {
namespace {

using server::plugins::PluginAdvancement;
using wire::common::Identifier;

[[nodiscard]] std::string toJson(const wire::text::Component& component) {
    return wire::common::writeJson(wire::text::componentJson::serialize(component));
}

[[nodiscard]] std::optional<Identifier> parse(std::string_view id) {
    const auto parsed = Identifier::parse(std::string{id});
    return parsed.has_value() ? std::optional{*parsed} : std::nullopt;
}

} // namespace

const world::AdvancementTree& ServerAdvancements::tree() {
    const world::RealAdvancements* loaded = hooks_.advancements ? hooks_.advancements() : nullptr;
    if (tree_ == nullptr || treeOf_ != loaded) {
        tree_ = loaded == nullptr ? std::make_unique<world::AdvancementTree>()
                                  : std::make_unique<world::AdvancementTree>(*loaded);
        treeOf_ = loaded;
    }
    return *tree_;
}

std::vector<std::string> ServerAdvancements::ids() {
    std::vector<std::string> out;
    if (const world::RealAdvancements* loaded = hooks_.advancements ? hooks_.advancements() : nullptr;
        loaded != nullptr) {
        for (const Identifier& id : loaded->ids()) {
            out.push_back(id.toString());
        }
    }
    return out;
}

std::optional<PluginAdvancement> ServerAdvancements::advancement(std::string_view id) {
    const world::RealAdvancements* loaded = hooks_.advancements ? hooks_.advancements() : nullptr;
    const std::optional<Identifier> key = parse(id);
    const server::world::advancement::Advancement* found =
        loaded == nullptr || !key.has_value() ? nullptr : loaded->find(*key);
    if (found == nullptr) {
        return std::nullopt;
    }
    PluginAdvancement out;
    out.id = key->toString();
    if (found->parent.has_value()) {
        out.parent = found->parent->toString();
    }
    out.root = tree().rootOf(*key).toString();
    if (const world::AdvancementTree::Node* node = tree().get(*key); node != nullptr) {
        for (const Identifier& child : node->children) {
            out.children.push_back(child.toString());
        }
        out.x = node->x;
        out.y = node->y;
    }
    for (const auto& [name, criterion] : found->criteria) {
        out.criteria.push_back(name);
    }
    out.requirements = found->requirements.groups;
    if (found->display.has_value()) {
        const server::world::advancement::AdvancementDisplay& display = *found->display;
        out.hasDisplay = true;
        out.title = toJson(display.title);
        out.description = toJson(display.description);
        out.iconItem = display.icon.item.toString();
        out.frame = display.type;
        out.background = display.background;
        out.toast = display.showToast;
        out.announce = display.announceToChat;
        out.hidden = display.hidden;
    }
    out.experience = found->rewards.experience;
    for (const Identifier& recipe : found->rewards.recipes) {
        out.recipes.push_back(recipe.toString());
    }
    for (const Identifier& table : found->rewards.loot) {
        out.loot.push_back(table.toString());
    }
    return out;
}

std::map<std::string, std::int64_t> ServerAdvancements::obtained(std::string_view uuid, std::string_view id) {
    world::PlayerProgress* progress = hooks_.progress ? hooks_.progress() : nullptr;
    const std::optional<Identifier> key = parse(id);
    std::map<std::string, std::int64_t> out;
    if (progress != nullptr && key.has_value()) {
        for (const auto& [name, when] : progress->obtained(uuid, *key)) {
            out.emplace(name, when);
        }
    }
    return out;
}

bool ServerAdvancements::grant(std::string_view uuid, std::string_view id, std::string_view criterion) {
    world::PlayerProgress* progress = hooks_.progress ? hooks_.progress() : nullptr;
    const std::optional<Identifier> key = parse(id);
    if (progress == nullptr || !key.has_value()) {
        return false;
    }
    if (world::AdvancementTriggers* triggers = hooks_.triggers ? hooks_.triggers() : nullptr; triggers != nullptr) {
        std::string name;
        for (const player::PlayerSession* session :
             hooks_.sessions ? hooks_.sessions() : std::vector<player::PlayerSession*>{}) {
            if (session->uuid().toString() == uuid) {
                name = session->name();
            }
        }
        return triggers->award(name, uuid, *key, criterion);
    }
    return progress->grantCriterion(uuid, *key, criterion);
}

bool ServerAdvancements::revoke(std::string_view uuid, std::string_view id, std::string_view criterion) {
    world::PlayerProgress* progress = hooks_.progress ? hooks_.progress() : nullptr;
    const std::optional<Identifier> key = parse(id);
    return progress != nullptr && key.has_value() && progress->revokeCriterion(uuid, *key, criterion);
}

} // namespace basalt::server::runtime::plugins
