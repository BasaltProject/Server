#include <basalt/server/plugins/PluginMenuObject.hpp>
#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;
} // namespace

std::shared_ptr<LuaMenu> LuaMenu::create(PluginMenus& menus, PluginPlayers& players, std::string title,
                                         std::int32_t rows, Call call) {
    return std::shared_ptr<LuaMenu>{new LuaMenu{menus, players, std::move(title), rows, std::move(call)}};
}

void LuaMenu::redraw() {
    for (const auto& [uuid, handle] : opened_) {
        const std::shared_ptr<PluginPlayerRef> player = players_->byUuid(uuid);
        if (player != nullptr) {
            menus_->refresh(*player, items_);
        }
    }
}

void LuaMenu::closeAll() {
    for (auto& [uuid, handle] : opened_) {
        const std::shared_ptr<PluginPlayerRef> player = players_->byUuid(uuid);
        if (player != nullptr) {
            menus_->close(*player);
        }
        handle.remove();
    }
    opened_.clear();
}

void LuaMenu::push(LuaArgs& lua) {
    const std::shared_ptr<LuaMenu> self = shared_from_this();
    PluginObject object{"menu"};
    object.reads("title", [self] { return PluginValue{self->title_}; })
        .reads("rows", [self] { return PluginValue{static_cast<std::int64_t>(self->rows_)}; })
        .reads("size", [self] { return PluginValue{static_cast<std::int64_t>(self->rows_ * PluginMenus::kColumns)}; })
        .method("set",
                [self](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const auto slot = static_cast<std::int32_t>(args.checkInteger(base));
                    const std::optional<PluginItem> item = itemFrom(args, base + 1);
                    if (!item.has_value()) {
                        args.raise("menu:set(slot, item) needs an item");
                    }
                    if (slot < 0 || slot >= self->rows_ * PluginMenus::kColumns) {
                        args.raise("slot " + std::to_string(slot) + " is outside a menu of " +
                                   std::to_string(self->rows_) + " rows");
                    }
                    self->items_.insert_or_assign(slot, *item);
                    if (args.isFunction(base + 2)) {
                        self->handlers_.insert_or_assign(slot, wire::lua::LuaRef::of(args.state(), base + 2));
                    } else {
                        self->handlers_.erase(slot);
                    }
                    self->redraw();
                    return 0;
                })
        .method("clear",
                [self](LuaArgs& args) {
                    static_cast<void>(args);
                    self->items_.clear();
                    self->handlers_.clear();
                    self->redraw();
                    return 0;
                })
        .method("open",
                [self](LuaArgs& args) {
                    const std::shared_ptr<PluginPlayerRef> player =
                        playerFrom(args, firstArgument(args), *self->players_);
                    if (player == nullptr) {
                        args.raise("menu:open(player) needs a player");
                    }
                    const std::string uuid = player->uuid();
                    if (const auto opened = self->opened_.find(uuid); opened != self->opened_.end()) {
                        opened->second.remove();
                    }
                    self->opened_.insert_or_assign(
                        uuid, self->menus_->open(*player, self->title_, self->rows_, self->items_,
                                                 [self, player](std::int32_t slot) {
                                                     const auto handler = self->handlers_.find(slot);
                                                     if (handler != self->handlers_.end() && self->call_) {
                                                         self->call_(handler->second, *player, slot);
                                                     }
                                                 }));
                    return 0;
                })
        .method("close", [self](LuaArgs& args) {
            const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, firstArgument(args), *self->players_);
            if (player == nullptr) {
                args.raise("menu:close(player) needs a player");
            }
            self->menus_->close(*player);
            const auto opened = self->opened_.find(player->uuid());
            if (opened != self->opened_.end()) {
                opened->second.remove();
                self->opened_.erase(opened);
            }
            return 0;
        });
    std::move(object).push(lua);
}
} // namespace basalt::server::plugins
