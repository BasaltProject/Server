#include <basalt/server/plugins/PluginFormObject.hpp>
#include <basalt/server/plugins/PluginObject.hpp>
#include <basalt/server/plugins/PluginObjects.hpp>

#include <utility>

namespace basalt::server::plugins {
namespace {
using wire::lua::LuaArgs;

[[nodiscard]] std::optional<std::string> textField(LuaArgs& lua, const std::int32_t table, const std::string_view key) {
    lua.pushField(table, key);
    std::optional<std::string> value;
    if (lua.isString(-1) || lua.isNumber(-1)) {
        value = lua.toText(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] std::optional<double> numberField(LuaArgs& lua, const std::int32_t table, const std::string_view key) {
    lua.pushField(table, key);
    std::optional<double> value;
    if (lua.isNumber(-1)) {
        value = lua.toNumber(-1);
    }
    lua.pop(1);
    return value;
}

[[nodiscard]] std::optional<bool> booleanField(LuaArgs& lua, const std::int32_t table, const std::string_view key) {
    lua.pushField(table, key);
    std::optional<bool> value;
    if (lua.isBoolean(-1)) {
        value = lua.toBoolean(-1);
    }
    lua.pop(1);
    return value;
}

/// The strings of the list under `key`, or of the one string there.
[[nodiscard]] std::vector<std::string> listField(LuaArgs& lua, const std::int32_t table, const std::string_view key,
                                                 const std::function<std::string(const std::string&)>& resolve,
                                                 const bool resolveEach) {
    std::vector<std::string> out;
    lua.pushField(table, key);
    const std::int32_t at = lua.top();
    if (lua.isString(at)) {
        out.push_back(resolveEach ? resolve(lua.toText(at)) : lua.toText(at));
    } else if (lua.isTable(at)) {
        const std::int64_t count = lua.length(at);
        for (std::int64_t i = 1; i <= count; ++i) {
            lua.pushIndex(at, i);
            if (lua.isString(-1) || lua.isNumber(-1)) {
                out.push_back(resolveEach ? resolve(lua.toText(-1)) : lua.toText(-1));
            }
            lua.pop(1);
        }
    }
    lua.pop(1);
    return out;
}
} // namespace

PluginForm readForm(LuaArgs& lua, const std::int32_t index,
                    const std::function<std::string(const std::string&)>& resolve) {
    if (!lua.isTable(index)) {
        lua.raise("form{ title = ..., fields = {...}, buttons = {...} } takes a table");
    }
    PluginForm form;
    form.title = resolve(textField(lua, index, "title").value_or(""));
    form.body = listField(lua, index, "body", resolve, true);
    form.buttons = listField(lua, index, "buttons", resolve, true);
    if (form.buttons.empty()) {
        form.buttons.push_back(resolve("OK"));
    }
    lua.pushField(index, "fields");
    const std::int32_t list = lua.top();
    if (lua.isTable(list)) {
        const std::int64_t count = lua.length(list);
        for (std::int64_t i = 1; i <= count; ++i) {
            lua.pushIndex(list, i);
            const std::int32_t one = lua.top();
            if (!lua.isTable(one)) {
                lua.raise("field " + std::to_string(i) + " of the form is not a table");
            }
            PluginFormField field;
            field.key = textField(lua, one, "key").value_or("");
            if (field.key.empty()) {
                lua.raise("field " + std::to_string(i) + " of the form needs a key");
            }
            field.label = resolve(textField(lua, one, "label").value_or(field.key));
            const std::string kind = textField(lua, one, "type").value_or("text");
            if (kind == "text") {
                field.kind = PluginFormField::Kind::Text;
                field.initialText = textField(lua, one, "initial").value_or("");
            } else if (kind == "toggle") {
                field.kind = PluginFormField::Kind::Toggle;
                field.initialToggle = booleanField(lua, one, "initial").value_or(false);
            } else if (kind == "slider") {
                field.kind = PluginFormField::Kind::Slider;
                field.min = numberField(lua, one, "min").value_or(0.0);
                field.max = numberField(lua, one, "max").value_or(1.0);
                field.step = numberField(lua, one, "step").value_or(0.0);
                field.initialNumber = numberField(lua, one, "initial").value_or((field.min + field.max) / 2.0);
                if (field.max < field.min) {
                    lua.raise("slider '" + field.key + "' ends before it starts");
                }
            } else if (kind == "choice") {
                field.kind = PluginFormField::Kind::Choice;
                field.options = listField(lua, one, "options", resolve, false);
                if (field.options.empty()) {
                    lua.raise("choice '" + field.key + "' has no options");
                }
                field.initialOption = textField(lua, one, "initial").value_or(field.options.front());
            } else {
                lua.raise("field '" + field.key + "' has type '" + kind + "'; use text, toggle, slider or choice");
            }
            lua.pop(1);
            form.fields.push_back(std::move(field));
        }
    }
    lua.pop(1);
    return form;
}

std::shared_ptr<LuaForm> LuaForm::create(PluginMenus& menus, PluginPlayers& players, PluginForm form, Call call) {
    return std::shared_ptr<LuaForm>{new LuaForm{menus, players, std::move(form), std::move(call)}};
}

void LuaForm::closeAll() {
    for (auto& [uuid, handle] : opened_) {
        const std::shared_ptr<PluginPlayerRef> player = players_->byUuid(uuid);
        if (player != nullptr) {
            menus_->close(*player);
        }
        handle.remove();
    }
    opened_.clear();
    handlers_.clear();
}

void LuaForm::push(LuaArgs& lua) {
    const std::shared_ptr<LuaForm> self = shared_from_this();
    PluginObject object{"form"};
    object.reads("title", [self] { return PluginValue{self->form_.title}; })
        .method("open",
                [self](LuaArgs& args) {
                    const std::int32_t base = firstArgument(args);
                    const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, base, *self->players_);
                    if (player == nullptr) {
                        args.raise("form:open(player, handler) needs a player");
                    }
                    if (!args.isFunction(base + 1)) {
                        args.raise("form:open(player, handler) needs a function to hear the answer");
                    }
                    const std::string uuid = player->uuid();
                    if (const auto opened = self->opened_.find(uuid); opened != self->opened_.end()) {
                        opened->second.remove();
                    }
                    self->handlers_.insert_or_assign(
                        uuid, std::make_shared<wire::lua::LuaRef>(wire::lua::LuaRef::of(args.state(), base + 1)));
                    const std::weak_ptr<LuaForm> weak = self;
                    self->opened_.insert_or_assign(
                        uuid,
                        self->menus_->openForm(
                            *player, self->form_,
                            [weak, player, uuid](std::size_t button, const std::map<std::string, std::string>& values) {
                                const std::shared_ptr<LuaForm> form = weak.lock();
                                if (form == nullptr || !form->call_) {
                                    return;
                                }
                                const auto handler = form->handlers_.find(uuid);
                                if (handler != form->handlers_.end()) {
                                    form->call_(*handler->second, *player, form->form_, button, values);
                                }
                            }));
                    return 0;
                })
        .method("close", [self](LuaArgs& args) {
            const std::shared_ptr<PluginPlayerRef> player = playerFrom(args, firstArgument(args), *self->players_);
            if (player == nullptr) {
                args.raise("form:close(player) needs a player");
            }
            self->menus_->close(*player);
            const auto opened = self->opened_.find(player->uuid());
            if (opened != self->opened_.end()) {
                opened->second.remove();
                self->opened_.erase(opened);
            }
            self->handlers_.erase(player->uuid());
            return 0;
        });
    std::move(object).push(lua);
}
} // namespace basalt::server::plugins
