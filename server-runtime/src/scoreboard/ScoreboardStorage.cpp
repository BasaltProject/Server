#include <basalt/server/runtime/scoreboard/ScoreboardStorage.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>

#include <array>
#include <string>

namespace basalt::server::runtime::scoreboard {
namespace {

using wire::nbt::NbtCompound;
using wire::nbt::NbtList;
using wire::nbt::NbtTag;
using wire::nbt::NbtType;
using wire::protocol::play::DisplaySlot;
using wire::protocol::play::NumberFormat;
using wire::protocol::play::ObjectiveRenderType;

constexpr std::int32_t kSlotCount = 19;

/// parity: `DisplaySlot.CODEC`, the three named slots then one sidebar per team colour.
[[nodiscard]] std::string slotName(std::int32_t slot) {
    static constexpr std::array<std::string_view, 3> kNamed{"list", "sidebar", "below_name"};
    if (slot < 3) {
        return std::string{kNamed[static_cast<std::size_t>(slot)]};
    }
    return "sidebar.team." +
           std::string{Objective::teamColorName(static_cast<wire::protocol::play::TeamColor>(slot - 3))};
}

[[nodiscard]] std::optional<std::int32_t> slotByName(std::string_view name) {
    for (std::int32_t slot = 0; slot < kSlotCount; ++slot) {
        if (slotName(slot) == name) {
            return slot;
        }
    }
    return std::nullopt;
}

[[nodiscard]] NbtTag packFormat(const NumberFormat& format) {
    NbtCompound out;
    if (const auto* styled = format.as<NumberFormat::Styled>(); styled != nullptr) {
        out.putString("type", "minecraft:styled");
        out.set("style", NbtTag::ofCompound(styled->style));
    } else if (const auto* fixed = format.as<NumberFormat::Fixed>(); fixed != nullptr) {
        out.putString("type", "minecraft:fixed");
        out.set("value", wire::text::componentNbt::serialize(fixed->value));
    } else {
        out.putString("type", "minecraft:blank");
    }
    return NbtTag::ofCompound(std::move(out));
}

[[nodiscard]] std::optional<NumberFormat> unpackFormat(const NbtCompound& holder, std::string_view key) {
    const NbtCompound* tag = holder.getCompound(key);
    if (tag == nullptr) {
        return std::nullopt;
    }
    const std::string_view type = tag->getString("type");
    if (type == "minecraft:styled") {
        const NbtCompound* style = tag->getCompound("style");
        return NumberFormat{NumberFormat::Styled{style == nullptr ? NbtCompound{} : *style}};
    }
    if (type == "minecraft:fixed") {
        const NbtTag* value = tag->find("value");
        return value == nullptr
                   ? std::nullopt
                   : std::optional{NumberFormat{NumberFormat::Fixed{wire::text::componentNbt::deserialize(*value)}}};
    }
    return NumberFormat{NumberFormat::Blank{}};
}

[[nodiscard]] wire::text::Component componentOf(const NbtCompound& holder, std::string_view key,
                                                wire::text::Component fallback) {
    const NbtTag* value = holder.find(key);
    return value == nullptr ? std::move(fallback) : wire::text::componentNbt::deserialize(*value);
}

} // namespace

NbtCompound pack(const Scoreboard& scoreboard, const Teams& teams) {
    NbtCompound out;
    std::vector<NbtTag> objectives;
    std::vector<NbtTag> scores;
    for (const Objective* objective : scoreboard.allObjectives()) {
        NbtCompound entry;
        entry.putString("Name", objective->name);
        entry.putString("CriteriaName", objective->criterion);
        entry.set("DisplayName", wire::text::componentNbt::serialize(objective->displayName));
        entry.putString("RenderType", objective->renderType == ObjectiveRenderType::Hearts ? "hearts" : "integer");
        entry.putBoolean("display_auto_update", objective->displayAutoUpdate);
        if (objective->numberFormat.has_value()) {
            entry.set("format", packFormat(*objective->numberFormat));
        }
        objectives.push_back(NbtTag::ofCompound(std::move(entry)));
        for (const std::string& holder : scoreboard.holdersIn(objective->name)) {
            NbtCompound score;
            score.putString("Name", holder);
            score.putString("Objective", objective->name);
            score.putInt("Score", scoreboard.score(holder, objective->name).value_or(0));
            score.putBoolean("Locked", !scoreboard.isTriggerEnabled(holder, objective->name));
            if (const wire::text::Component* display = scoreboard.displayNameOf(holder, objective->name);
                display != nullptr) {
                score.set("display", wire::text::componentNbt::serialize(*display));
            }
            if (const NumberFormat* format = scoreboard.numberFormatOf(holder, objective->name); format != nullptr) {
                score.set("format", packFormat(*format));
            }
            scores.push_back(NbtTag::ofCompound(std::move(score)));
        }
    }
    out.set("Objectives", NbtTag::ofList(NbtList{NbtType::Compound, std::move(objectives)}));
    out.set("PlayerScores", NbtTag::ofList(NbtList{NbtType::Compound, std::move(scores)}));

    NbtCompound slots;
    for (std::int32_t slot = 0; slot < kSlotCount; ++slot) {
        if (const std::string* shown = scoreboard.displayed(static_cast<DisplaySlot>(slot)); shown != nullptr) {
            slots.putString(slotName(slot), *shown);
        }
    }
    out.set("DisplaySlots", NbtTag::ofCompound(std::move(slots)));

    std::vector<NbtTag> packedTeams;
    for (const Team* team : teams.all()) {
        NbtCompound entry;
        entry.putString("Name", team->name);
        entry.set("DisplayName", wire::text::componentNbt::serialize(team->displayName));
        if (team->color.has_value()) {
            entry.putString("TeamColor", std::string{Objective::teamColorName(*team->color)});
        }
        entry.putBoolean("AllowFriendlyFire", team->friendlyFire);
        entry.putBoolean("SeeFriendlyInvisibles", team->seeFriendlyInvisibles);
        entry.set("MemberNamePrefix", wire::text::componentNbt::serialize(team->prefix));
        entry.set("MemberNameSuffix", wire::text::componentNbt::serialize(team->suffix));
        entry.putString("NameTagVisibility",
                        std::string{wire::protocol::play::nameTagVisibilityName(team->nameTagVisibility)});
        entry.putString("DeathMessageVisibility",
                        std::string{wire::protocol::play::nameTagVisibilityName(team->deathMessageVisibility)});
        entry.putString("CollisionRule", std::string{wire::protocol::play::collisionRuleName(team->collisionRule)});
        std::vector<NbtTag> players;
        for (const std::string& member : team->members) {
            players.push_back(NbtTag::ofString(member));
        }
        entry.set("Players", NbtTag::ofList(NbtList{NbtType::String, std::move(players)}));
        packedTeams.push_back(NbtTag::ofCompound(std::move(entry)));
    }
    out.set("Teams", NbtTag::ofList(NbtList{NbtType::Compound, std::move(packedTeams)}));
    return out;
}

void unpack(const NbtCompound& packed, Scoreboard& scoreboard, Teams& teams) {
    const auto each = [&packed](std::string_view key, const auto& visit) {
        if (const NbtList* list = packed.getList(key); list != nullptr) {
            for (const NbtTag& element : *list) {
                if (const NbtCompound* entry = element.get<NbtCompound>(); entry != nullptr) {
                    visit(*entry);
                }
            }
        }
    };
    each("Objectives", [&scoreboard](const NbtCompound& entry) {
        Objective objective;
        objective.name = std::string{entry.getString("Name")};
        objective.criterion = entry.contains("CriteriaName") ? std::string{entry.getString("CriteriaName")} : "dummy";
        objective.displayName = componentOf(entry, "DisplayName", wire::text::Component::text(objective.name));
        objective.renderType =
            entry.getString("RenderType") == "hearts" ? ObjectiveRenderType::Hearts : ObjectiveRenderType::Integer;
        objective.displayAutoUpdate = entry.getBoolean("display_auto_update", false);
        objective.numberFormat = unpackFormat(entry, "format");
        scoreboard.add(std::move(objective));
    });
    each("PlayerScores", [&scoreboard](const NbtCompound& entry) {
        const std::string holder{entry.getString("Name")};
        const std::string objective{entry.getString("Objective")};
        if (scoreboard.get(objective) == nullptr) {
            return;
        }
        scoreboard.setScore(holder, objective, entry.getInt("Score"));
        if (!entry.getBoolean("Locked", false)) {
            scoreboard.enableTrigger(holder, objective);
        }
        if (const NbtTag* display = entry.find("display"); display != nullptr) {
            scoreboard.setDisplayName(holder, objective, wire::text::componentNbt::deserialize(*display));
        }
        if (std::optional<NumberFormat> format = unpackFormat(entry, "format"); format.has_value()) {
            scoreboard.setNumberFormat(holder, objective, std::move(format));
        }
    });
    if (const NbtCompound* slots = packed.getCompound("DisplaySlots"); slots != nullptr) {
        for (std::size_t i = 0; i < slots->size(); ++i) {
            const std::optional<std::int32_t> slot = slotByName(slots->names()[i]);
            const std::string* objective = slots->values()[i].get<std::string>();
            if (slot.has_value() && objective != nullptr && scoreboard.get(*objective) != nullptr) {
                scoreboard.display(static_cast<DisplaySlot>(*slot), *objective);
            }
        }
    }
    each("Teams", [&teams](const NbtCompound& entry) {
        Team* team = teams.add(std::string{entry.getString("Name")});
        if (team == nullptr) {
            return;
        }
        team->displayName = componentOf(entry, "DisplayName", wire::text::Component::text(team->name));
        if (entry.contains("TeamColor")) {
            team->color = wire::protocol::play::teamColorByName(entry.getString("TeamColor"));
        }
        team->friendlyFire = entry.getBoolean("AllowFriendlyFire", true);
        team->seeFriendlyInvisibles = entry.getBoolean("SeeFriendlyInvisibles", true);
        team->prefix = componentOf(entry, "MemberNamePrefix", wire::text::Component::empty());
        team->suffix = componentOf(entry, "MemberNameSuffix", wire::text::Component::empty());
        team->nameTagVisibility = wire::protocol::play::nameTagVisibilityByName(entry.getString("NameTagVisibility"))
                                      .value_or(wire::protocol::play::NameTagVisibility::Always);
        team->deathMessageVisibility =
            wire::protocol::play::nameTagVisibilityByName(entry.getString("DeathMessageVisibility"))
                .value_or(wire::protocol::play::NameTagVisibility::Always);
        team->collisionRule = wire::protocol::play::collisionRuleByName(entry.getString("CollisionRule"))
                                  .value_or(wire::protocol::play::CollisionRule::Always);
        if (const NbtList* players = entry.getList("Players"); players != nullptr) {
            for (const NbtTag& member : *players) {
                if (const std::string* name = member.get<std::string>(); name != nullptr) {
                    team->addMember(*name);
                }
            }
        }
    });
}

} // namespace basalt::server::runtime::scoreboard
