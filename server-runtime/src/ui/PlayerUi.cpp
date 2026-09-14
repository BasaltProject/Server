#include <basalt/server/runtime/ui/PlayerUi.hpp>
#include <basalt/wire/common/placeholder/PlaceholderText.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime::ui {
namespace {

using wire::common::placeholder::PlaceholderContext;
using wire::protocol::play::BossBarColor;
using wire::protocol::play::BossBarOverlay;
using wire::protocol::play::BossEventPacket;
using wire::protocol::play::NumberFormat;
using wire::protocol::play::ResetScorePacket;
using wire::protocol::play::SetDisplayObjectivePacket;
using wire::protocol::play::SetObjectivePacket;
using wire::protocol::play::SetScorePacket;
using wire::protocol::play::TabListPacket;
using wire::text::Component;

/// One objective per player connection; the name only has to be unique within that client.
constexpr std::string_view kObjective = "basalt";

/// How often a plugin's sidebar is redrawn where the operator's own file is switched off.
constexpr std::int32_t kPluginSidebarInterval = 20;

std::string owner(std::size_t index) { return "basalt.line." + std::to_string(index); }

/// An interval of 0 means "once, on the first tick": some panels are static, and re-sending a fixed
/// header twenty times a second is pure waste.
bool due(std::int64_t tick, bool enabled, std::int32_t interval) {
    if (!enabled) {
        return false;
    }
    return interval <= 0 ? tick == 0 : tick % interval == 0;
}

std::optional<float> parseFloat(std::string_view text) {
    float value = 0.0F;
    const char* end = text.data() + text.size();
    const std::from_chars_result parsed = std::from_chars(text.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end) {
        return std::nullopt;
    }
    return value;
}

} // namespace

PlayerUi::PlayerUi(std::function<config::UiConfigs()> configs,
                   const wire::common::placeholder::PlaceholderRegistry& placeholders, Send send, Plugin plugin,
                   wire::common::Logger logger)
    : configs_(std::move(configs)), placeholders_(&placeholders), send_(std::move(send)), plugin_(std::move(plugin)),
      logger_(std::move(logger)) {}

void PlayerUi::tick(std::int64_t tick, const PlaceholderContext& context) {
    const config::UiConfigs configs = configs_();
    if (due(tick, configs.tabList.enabled, configs.tabList.updateIntervalTicks)) {
        updateTabList(context);
    }
    // A plugin holding the sidebar turns it on even where the operator's file left it off:
    // installing a plugin that draws one should not also mean editing scoreboard.toml.
    const bool boardOn = configs.scoreboard.enabled || (plugin_.holdsSidebar && plugin_.holdsSidebar());
    const std::int32_t boardEvery =
        configs.scoreboard.enabled ? configs.scoreboard.updateIntervalTicks : kPluginSidebarInterval;
    if (due(tick, boardOn, boardEvery)) {
        updateScoreboard(context);
    }
    const bool barOn = configs.bossBar.enabled || (plugin_.holdsBossBar && plugin_.holdsBossBar());
    const std::int32_t barEvery =
        configs.bossBar.enabled ? configs.bossBar.updateIntervalTicks : kPluginSidebarInterval;
    if (due(tick, barOn, barEvery)) {
        updateBossBar(context);
    }
}

void PlayerUi::clearScoreboard() {
    if (!scoreboardShown_) {
        return;
    }
    if (surfaces_.clearScoreboard) {
        surfaces_.clearScoreboard();
        scoreboardShown_ = false;
        lastScoreboardLines_.clear();
        lastScoreboardTitle_.reset();
        return;
    }
    send_(SetObjectivePacket::kId, [](wire::io::ByteBuffer& buffer) {
        SetObjectivePacket{.objectiveName = std::string{kObjective}, .method = SetObjectivePacket::kMethodRemove}.write(
            buffer);
    });
    scoreboardShown_ = false;
    lastScoreboardLines_.clear();
    lastScoreboardTitle_.reset();
}

void PlayerUi::clear() {
    clearScoreboard();
    clearBossBar();
}

void PlayerUi::updateTabList(const PlaceholderContext& context) {
    const config::UiConfigs configs = configs_();
    const config::TabListProfile profile =
        config::selectProfile(configs.tabList.profiles, configs.tabList.defaultProfile(), *placeholders_, context,
                              logger_, reporter_, "tab list")
            .inheriting(configs.tabList.defaultProfile());
    const std::pair<std::string, Component> header =
        render(profile.headerLines.value_or(std::vector<std::string>{}), context);
    const std::pair<std::string, Component> footer =
        render(profile.footerLines.value_or(std::vector<std::string>{}), context);
    if (lastTabList_.has_value() && lastTabList_->first == header.first && lastTabList_->second == footer.first) {
        return;
    }
    lastTabList_ = std::pair{header.first, footer.first};
    send_(TabListPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        TabListPacket{.header = header.second, .footer = footer.second}.write(buffer);
    });
}

void PlayerUi::updateScoreboard(const PlaceholderContext& context) {
    const config::UiConfigs configs = configs_();
    if (plugin_.sidebar) {
        const std::optional<SurfaceContent> fromPlugin = plugin_.sidebar();
        if (fromPlugin.has_value()) {
            std::vector<std::string> lines;
            for (const std::string& line : fromPlugin->lines) {
                if (static_cast<std::int32_t>(lines.size()) >= config::ScoreboardConfig::kMaxLines) {
                    break;
                }
                lines.push_back(substitute(line, context));
            }
            drawScoreboard(substitute(fromPlugin->title, context), lines);
            return;
        }
    }
    if (!configs.scoreboard.enabled) {
        // The plugin that was drawing it let go and the operator never wanted one.
        clearScoreboard();
        return;
    }
    const config::ScoreboardProfile profile =
        config::selectProfile(configs.scoreboard.profiles, configs.scoreboard.defaultProfile(), *placeholders_, context,
                              logger_, reporter_, "scoreboard")
            .inheriting(configs.scoreboard.defaultProfile());
    std::vector<std::string> lines;
    for (const std::string& line : profile.lines.value_or(std::vector<std::string>{})) {
        if (static_cast<std::int32_t>(lines.size()) >= config::ScoreboardConfig::kMaxLines) {
            break;
        }
        lines.push_back(substitute(line, context));
    }
    drawScoreboard(substitute(profile.title.value_or(std::string{}), context), lines);
}

void PlayerUi::drawScoreboard(const std::string& title, const std::vector<std::string>& lines) {
    // A client that draws its own gets the whole board every time: what changed is its business,
    // and the two editions do not agree on how a line is named.
    if (surfaces_.scoreboard) {
        if (scoreboardShown_ && lastScoreboardTitle_.has_value() && *lastScoreboardTitle_ == title &&
            lastScoreboardLines_ == lines) {
            return;
        }
        surfaces_.scoreboard(title, lines);
        scoreboardShown_ = true;
        lastScoreboardTitle_ = title;
        lastScoreboardLines_ = lines;
        return;
    }
    if (!scoreboardShown_) {
        send_(SetObjectivePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetObjectivePacket{.objectiveName = std::string{kObjective},
                               .method = SetObjectivePacket::kMethodAdd,
                               .displayName = wire::text::miniMessage::parse(title),
                               .numberFormat = NumberFormat{NumberFormat::Blank{}}}
                .write(buffer);
        });
        send_(SetDisplayObjectivePacket::kId, [](wire::io::ByteBuffer& buffer) {
            SetDisplayObjectivePacket{.slot = wire::protocol::play::DisplaySlot::Sidebar,
                                      .objectiveName = std::string{kObjective}}
                .write(buffer);
        });
        scoreboardShown_ = true;
        lastScoreboardTitle_ = title;
    } else if (!lastScoreboardTitle_.has_value() || *lastScoreboardTitle_ != title) {
        send_(SetObjectivePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetObjectivePacket{.objectiveName = std::string{kObjective},
                               .method = SetObjectivePacket::kMethodChange,
                               .displayName = wire::text::miniMessage::parse(title),
                               .numberFormat = NumberFormat{NumberFormat::Blank{}}}
                .write(buffer);
        });
        lastScoreboardTitle_ = title;
    }

    for (std::size_t index = 0; index < lines.size(); index++) {
        if (index < lastScoreboardLines_.size() && lastScoreboardLines_[index] == lines[index]) {
            continue;
        }
        // The score orders the rows, highest at the top, so it counts down from the line count.
        const auto score = static_cast<std::int32_t>(lines.size() - index);
        send_(SetScorePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetScorePacket{.owner = owner(index),
                           .objectiveName = std::string{kObjective},
                           .score = score,
                           .display = wire::text::miniMessage::parse(lines[index]),
                           .numberFormat = NumberFormat{NumberFormat::Blank{}}}
                .write(buffer);
        });
    }
    // A board that got shorter has to have its leftovers taken away, or they stay on screen.
    for (std::size_t index = lines.size(); index < lastScoreboardLines_.size(); index++) {
        send_(ResetScorePacket::kId, [&](wire::io::ByteBuffer& buffer) {
            ResetScorePacket{.owner = owner(index), .objectiveName = std::string{kObjective}}.write(buffer);
        });
    }
    lastScoreboardLines_ = lines;
}

void PlayerUi::updateBossBar(const PlaceholderContext& context) {
    const config::UiConfigs configs = configs_();
    if (plugin_.bossBar) {
        const std::optional<SurfaceContent> fromPlugin = plugin_.bossBar();
        if (fromPlugin.has_value()) {
            // A bar has a title and one number, so the plugin's first line is its progress: the
            // surfaces all carry the same shape and each one reads what it needs from it.
            float progress = 1.0F;
            if (!fromPlugin->lines.empty()) {
                progress = std::clamp(parseFloat(fromPlugin->lines.front()).value_or(1.0F), 0.0F, 1.0F);
            }
            drawBossBar(substitute(fromPlugin->title, context), progress, BossBarColor::White,
                        BossBarOverlay::Progress);
            return;
        }
    }
    if (!configs.bossBar.enabled) {
        clearBossBar();
        return;
    }
    const config::BossBarProfile profile =
        config::selectProfile(configs.bossBar.profiles, configs.bossBar.defaultProfile(), *placeholders_, context,
                              logger_, reporter_, "boss bar")
            .inheriting(configs.bossBar.defaultProfile());
    drawBossBar(substitute(profile.text.value_or(std::string{}), context), progressOf(profile.progress, context),
                profile.color.value_or(BossBarColor::White), profile.overlay.value_or(BossBarOverlay::Progress));
}

void PlayerUi::clearBossBar() {
    if (!bossBarId_.has_value()) {
        return;
    }
    if (surfaces_.clearBossBar) {
        surfaces_.clearBossBar();
        bossBarId_.reset();
        lastBossText_.reset();
        lastBossProgress_.reset();
        lastBossStyle_.reset();
        return;
    }
    const wire::common::Uuid id = *bossBarId_;
    send_(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
        BossEventPacket{.id = id, .operation = BossEventPacket::Remove{}}.write(buffer);
    });
    bossBarId_.reset();
    lastBossText_.reset();
    lastBossProgress_.reset();
    lastBossStyle_.reset();
}

void PlayerUi::drawBossBar(const std::string& text, float progress, BossBarColor color, BossBarOverlay overlay) {
    if (surfaces_.bossBar) {
        // One bar, kept by whether it has an id at all: over there the bar belongs to an entity and
        // adding it twice is what makes it flicker, so this only says what changed.
        const bool fresh = !bossBarId_.has_value();
        if (fresh) {
            bossBarId_ = wire::common::Uuid::random();
        }
        if (fresh || !lastBossText_.has_value() || *lastBossText_ != text || !lastBossProgress_.has_value() ||
            *lastBossProgress_ != progress) {
            surfaces_.bossBar(text, progress);
        }
        lastBossText_ = text;
        lastBossProgress_ = progress;
        lastBossStyle_ = std::pair{color, overlay};
        return;
    }
    if (!bossBarId_.has_value()) {
        const wire::common::Uuid fresh = wire::common::Uuid::random();
        bossBarId_ = fresh;
        send_(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            BossEventPacket{.id = fresh,
                            .operation = BossEventPacket::Add{.name = wire::text::miniMessage::parse(text),
                                                              .progress = progress,
                                                              .color = color,
                                                              .overlay = overlay}}
                .write(buffer);
        });
    } else {
        // Nudged rather than re-added: re-adding restarts the client's own fade, which reads as a
        // flicker every time anything on the bar changes.
        const wire::common::Uuid id = *bossBarId_;
        if (!lastBossText_.has_value() || *lastBossText_ != text) {
            send_(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                BossEventPacket{.id = id,
                                .operation = BossEventPacket::UpdateName{.name = wire::text::miniMessage::parse(text)}}
                    .write(buffer);
            });
        }
        if (!lastBossProgress_.has_value() || *lastBossProgress_ != progress) {
            send_(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                BossEventPacket{.id = id, .operation = BossEventPacket::UpdateProgress{.progress = progress}}.write(
                    buffer);
            });
        }
        if (!lastBossStyle_.has_value() || *lastBossStyle_ != std::pair{color, overlay}) {
            send_(BossEventPacket::kId, [&](wire::io::ByteBuffer& buffer) {
                BossEventPacket{.id = id, .operation = BossEventPacket::UpdateStyle{.color = color, .overlay = overlay}}
                    .write(buffer);
            });
        }
    }
    lastBossText_ = text;
    lastBossProgress_ = progress;
    lastBossStyle_ = std::pair{color, overlay};
}

float PlayerUi::progressOf(const std::optional<std::string>& raw, const PlaceholderContext& context) {
    const std::string text = substitute(raw.value_or("1.0"), context);
    return std::clamp(parseFloat(text).value_or(1.0F), 0.0F, 1.0F);
}

std::pair<std::string, Component> PlayerUi::render(const std::vector<std::string>& lines,
                                                   const PlaceholderContext& context) {
    if (lines.empty()) {
        return {std::string{}, Component::empty()};
    }
    std::string text;
    for (const std::string& line : lines) {
        if (!text.empty()) {
            text += "\n";
        }
        text += substitute(line, context);
    }
    Component component = Component::empty();
    bool first = true;
    for (std::size_t start = 0; start <= text.size();) {
        const std::size_t end = text.find('\n', start);
        const std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (first) {
            component = wire::text::miniMessage::parse(line);
            first = false;
        } else {
            component = component.append(Component::text("\n")).append(wire::text::miniMessage::parse(line));
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return {text, component};
}

std::string PlayerUi::substitute(std::string_view text, const PlaceholderContext& context) {
    return wire::common::placeholder::substitute(
        text, *placeholders_, context,
        [this](std::string_view name,
               const wire::common::placeholder::PlaceholderValue& value) -> std::optional<std::string> {
            if (value.kind == wire::common::placeholder::PlaceholderKind::Unknown) {
                reporter_.report(logger_, "A UI file uses %" + std::string{name} +
                                              "%, which is not a placeholder this server knows.");
                return std::nullopt;
            }
            // Ordinary: a panel that shows something only some players have should go blank for the
            // rest, not show them the name of the placeholder.
            return std::string{};
        });
}

} // namespace basalt::server::runtime::ui
