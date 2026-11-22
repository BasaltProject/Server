#include "WasmItemState.hpp"

#include <basalt/server/plugins/PluginServices.hpp>
#include <basalt/wire/common/json/Json.hpp>
#include <basalt/wire/text/ComponentJson.hpp>
#include <basalt/wire/text/ComponentNbt.hpp>
#include <basalt/wire/text/MiniMessage.hpp>

#include <charconv>
#include <map>

namespace basalt::server::plugins::wasm {
namespace {
using namespace items;

struct XMenu {
    std::string title;
    std::int32_t rows = 3;
    std::string kind = "chest";
    std::map<std::int32_t, PluginItem> items;
    std::map<std::int32_t, std::int32_t> tokens;
    std::map<std::string, PluginHandle> opened;
};

struct MenuState {
    HandleTable<XMenu> menus;
    bool cleanup = false;
};

[[nodiscard]] MenuState& menusOf(State& state) { return state.areaState<MenuState>("items_menus"); }

[[nodiscard]] PluginInventories& inventories(State& state) { return state.host->services().inventories(); }

[[nodiscard]] std::optional<std::string_view> textArg(abi::Call& c, std::size_t at) {
    return c.memory().string(c.i32(at), c.i32(at + 1));
}

void recipeAdd(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> id = textArg(c, 0);
    const std::optional<std::string_view> json = textArg(c, 2);
    const bool added = id && json && !id->empty() && inventories(state).addRecipeJson(qualified(*id), *json);
    c.ret(added ? abi::kOk : abi::kErrInvalid);
}

void recipeRemove(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = textArg(c, 0);
    if (!id || id->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(inventories(stateOf(user)).removeRecipe(qualified(*id)) ? abi::kOk : abi::kErrAbsent);
}

void recipeHas(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = textArg(c, 0);
    if (!id || id->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    c.ret(inventories(stateOf(user)).recipeData(qualified(*id)).has_value() ? 1 : 0);
}

void recipeIds(void* user, abi::Call& c) {
    c.ret(putValue(c.memory(), c.i32(0), c.i32(1), strings(inventories(stateOf(user)).recipeIds())));
}

void recipeClear(void* user, abi::Call& c) {
    inventories(stateOf(user)).clearRecipes();
    c.ret(abi::kOk);
}

void recipeReset(void* user, abi::Call& c) {
    inventories(stateOf(user)).resetRecipes();
    c.ret(abi::kOk);
}

void recipeResetBrewing(void* user, abi::Call& c) {
    inventories(stateOf(user)).resetBrewingRecipes();
    c.ret(abi::kOk);
}

void recipeGet(void* user, abi::Call& c) {
    const std::optional<std::string_view> id = textArg(c, 0);
    if (!id || id->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const std::optional<std::vector<std::uint8_t>> bytes = inventories(stateOf(user)).recipeData(qualified(*id));
    const std::optional<NbtCompound> compound = bytes.has_value() ? compoundFromBytes(*bytes) : std::nullopt;
    c.ret(compound.has_value() ? putValue(c.memory(), c.i32(2), c.i32(3), nbtToValue(NbtTag::ofCompound(*compound)))
                               : abi::kErrAbsent);
}

/// A station input stack: null is an empty cell, anything else must be an item value.
[[nodiscard]] std::optional<NbtCompound> cellOf(State& state, const Value* value) {
    if (value == nullptr || value->isNull()) {
        return NbtCompound{};
    }
    const std::optional<PluginItem> item =
        fromValue(*value, [&state](const std::string& id) { return knownItem(state, id); });
    return item.has_value() ? std::optional<NbtCompound>{compoundOf(*item)} : std::nullopt;
}

[[nodiscard]] std::int32_t station(State& state, std::string_view kind, const Value& input, Value& answer) {
    if (input.asMap() == nullptr) {
        return abi::kErrInvalid;
    }
    PluginInventories& service = inventories(state);
    std::optional<std::vector<std::uint8_t>> made;
    if (kind == "crafting") {
        const Value* width = input.find("width");
        const Value* height = input.find("height");
        const Value* cells = input.find("items");
        if (width == nullptr || height == nullptr || width->asInt() == nullptr || height->asInt() == nullptr ||
            cells == nullptr || cells->asList() == nullptr) {
            return abi::kErrInvalid;
        }
        wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
        for (const Value& cell : *cells->asList()) {
            const std::optional<NbtCompound> compound = cellOf(state, &cell);
            if (!compound.has_value()) {
                return abi::kErrInvalid;
            }
            static_cast<void>(list.push(NbtTag::ofCompound(*compound)));
        }
        NbtCompound root;
        root.set("items", NbtTag::ofList(std::move(list)));
        made = service.craft(static_cast<std::int32_t>(*width->asInt()), static_cast<std::int32_t>(*height->asInt()),
                             anonymous(std::move(root)));
    } else if (kind == "anvil" || kind == "smithing") {
        std::array<std::optional<NbtCompound>, 3> stacks;
        const std::array<std::string_view, 3> keys =
            kind == "anvil" ? std::array<std::string_view, 3>{"left", "right", ""}
                            : std::array<std::string_view, 3>{"template", "base", "addition"};
        for (std::size_t at = 0; at < (kind == "anvil" ? 2U : 3U); ++at) {
            stacks[at] = cellOf(state, input.find(keys[at]));
            if (!stacks[at].has_value()) {
                return abi::kErrInvalid;
            }
        }
        if (kind == "anvil") {
            const Value* name = input.find("name");
            made =
                service.anvil(anonymous(*stacks[0]), anonymous(*stacks[1]),
                              name != nullptr && name->asString() != nullptr ? *name->asString() : std::string_view{});
        } else {
            made = service.smithing(anonymous(*stacks[0]), anonymous(*stacks[1]), anonymous(*stacks[2]));
        }
    } else {
        return abi::kErrInvalid;
    }
    const std::optional<NbtCompound> compound = made.has_value() ? compoundFromBytes(*made) : std::nullopt;
    if (!compound.has_value()) {
        return abi::kErrAbsent;
    }
    answer = stationValue(*compound);
    return abi::kOk;
}

void recipeStation(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> kind = textArg(c, 0);
    const std::expected<Value, std::int32_t> input = readValue(c.memory(), c.i32(2), c.i32(3));
    Value answer;
    const std::int32_t status = !kind || !input.has_value() ? abi::kErrInvalid : station(state, *kind, *input, answer);
    c.ret(status != abi::kOk ? status : putValue(c.memory(), c.i32(4), c.i32(5), answer));
}

void bookGet(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> known = player->invoke("known_recipes", {});
    const auto* ids = known.has_value() ? std::get_if<std::vector<std::string>>(&*known) : nullptr;
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), strings(ids != nullptr ? *ids : std::vector<std::string>{})));
}

void bookSet(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::expected<Value, std::int32_t> ids = readValue(c.memory(), c.i32(1), c.i32(2));
    std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!ids.has_value() || ids->asList() == nullptr)) {
        status = abi::kErrInvalid;
    }
    std::vector<std::string> names;
    for (std::size_t at = 0; status == abi::kOk && at < ids->asList()->size(); ++at) {
        const std::string* name = (*ids->asList())[at].asString();
        if (name == nullptr) {
            status = abi::kErrInvalid;
        } else {
            names.push_back(qualified(*name));
        }
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> changed =
        player->invoke(c.i32(3) != 0 ? "discover_recipes" : "undiscover_recipes", {std::move(names)});
    const auto* count = changed.has_value() ? std::get_if<std::int64_t>(&*changed) : nullptr;
    c.ret(count == nullptr ? 0 : static_cast<std::int32_t>(*count));
}

void bookHas(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::optional<std::string_view> id = textArg(c, 1);
    std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!id || id->empty())) {
        status = abi::kErrInvalid;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> has = player->invoke("has_recipe", {qualified(*id)});
    const auto* flag = has.has_value() ? std::get_if<bool>(&*has) : nullptr;
    c.ret(flag != nullptr && *flag ? 1 : 0);
}

void bookResend(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    const std::optional<PluginEntityValue> sent = player->invoke("resend_recipes", {});
    const auto* flag = sent.has_value() ? std::get_if<bool>(&*sent) : nullptr;
    c.ret(flag != nullptr && *flag ? abi::kOk : abi::kErrAbsent);
}

[[nodiscard]] std::string titleJson(std::string_view miniMessage) {
    return wire::common::writeJson(wire::text::componentJson::serialize(wire::text::miniMessage::parse(miniMessage)));
}

void merchantOpen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    const Value* offers =
        status == abi::kOk && spec.has_value() && spec->asMap() != nullptr ? spec->find("offers") : nullptr;
    if (status == abi::kOk && (offers == nullptr || offers->asList() == nullptr)) {
        status = abi::kErrInvalid;
    }
    wire::nbt::NbtList list{wire::nbt::NbtType::Compound};
    for (std::size_t at = 0; status == abi::kOk && at < offers->asList()->size(); ++at) {
        const Value& offer = (*offers->asList())[at];
        const std::optional<NbtCompound> buy = cellOf(state, offer.find("buy"));
        const std::optional<NbtCompound> second = cellOf(state, offer.find("buy2"));
        const std::optional<NbtCompound> sell = cellOf(state, offer.find("sell"));
        if (!buy.has_value() || !second.has_value() || !sell.has_value() || buy->empty() || sell->empty()) {
            status = abi::kErrInvalid;
            break;
        }
        NbtCompound entry;
        entry.set("buy", NbtTag::ofCompound(*buy));
        entry.set("buy2", NbtTag::ofCompound(*second));
        entry.set("sell", NbtTag::ofCompound(*sell));
        for (const char* key : {"uses", "max_uses", "xp", "demand", "special_price"}) {
            if (const Value* number = offer.find(key); number != nullptr && number->asInt() != nullptr) {
                entry.putInt(key, static_cast<std::int32_t>(*number->asInt()));
            }
        }
        if (const Value* multiplier = offer.find("price_multiplier"); multiplier != nullptr) {
            const double* real = multiplier->asFloat();
            entry.putFloat("price_multiplier", real != nullptr ? static_cast<float>(*real) : 0.0F);
        }
        static_cast<void>(list.push(NbtTag::ofCompound(std::move(entry))));
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    NbtCompound root;
    root.set("offers", NbtTag::ofList(std::move(list)));
    const Value* title = spec->find("title");
    const std::optional<PluginEntityValue> opened = player->invoke(
        "open_merchant",
        {titleJson(title != nullptr && title->asString() != nullptr ? *title->asString() : std::string_view{}),
         wire::nbt::snbt::stringify(NbtTag::ofCompound(std::move(root)))});
    const auto* flag = opened.has_value() ? std::get_if<bool>(&*opened) : nullptr;
    c.ret(flag != nullptr && *flag ? abi::kOk : abi::kErrAbsent);
}

[[nodiscard]] NbtTag componentOf(std::string_view miniMessage) {
    return wire::text::componentNbt::serialize(wire::text::miniMessage::parse(miniMessage));
}

[[nodiscard]] std::string textOr(const Value& map, std::string_view key, std::string fallback) {
    const Value* found = map.find(key);
    return found != nullptr && found->asString() != nullptr ? *found->asString() : std::move(fallback);
}

void windowOpen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    std::shared_ptr<PluginPlayerRef> player;
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!spec.has_value() || spec->asMap() == nullptr)) {
        status = abi::kErrInvalid;
    }
    PluginWindow* window = status == abi::kOk ? player->window() : nullptr;
    if (status == abi::kOk && window == nullptr) {
        status = abi::kErrAbsent;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    PluginWindowSpec windowSpec;
    windowSpec.kind = textOr(*spec, "kind", "chest");
    windowSpec.title = componentOf(textOr(*spec, "title", ""));
    if (const Value* size = spec->find("size"); size != nullptr && size->asInt() != nullptr) {
        windowSpec.size = static_cast<std::int32_t>(*size->asInt());
    }
    if (const Value* editable = spec->find("editable"); editable != nullptr && editable->asBool() != nullptr) {
        windowSpec.editable = *editable->asBool();
    }
    if (const Value* items = spec->find("items"); items != nullptr && items->asList() != nullptr) {
        for (const Value& entry : *items->asList()) {
            const std::optional<NbtCompound> cell = cellOf(state, &entry);
            if (!cell.has_value()) {
                c.ret(abi::kErrInvalid);
                return;
            }
            windowSpec.items.push_back(cell->empty() ? Slot{} : Slot{itemOfCompound(*cell)});
        }
    }
    const std::int32_t container = window->openWindow(windowSpec);
    c.ret(container > 0 ? container : abi::kErrInvalid);
}

void windowInfo(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    PluginWindow* window = status == abi::kOk ? player->window() : nullptr;
    if (status != abi::kOk || window == nullptr || window->windowInfo().containerId == 0) {
        c.ret(status != abi::kOk ? status : abi::kErrAbsent);
        return;
    }
    const PluginWindowInfo info = window->windowInfo();
    Value::Map map{{"container_id", Value{static_cast<std::int64_t>(info.containerId)}},
                   {"kind", Value{info.kind}},
                   {"slots", Value{static_cast<std::int64_t>(info.slots)}},
                   {"plugin", Value{info.plugin}},
                   {"world", Value{info.world}}};
    if (info.block.has_value()) {
        map.push_back({"block", Value{Value::Map{{"x", Value{static_cast<std::int64_t>((*info.block)[0])}},
                                                 {"y", Value{static_cast<std::int64_t>((*info.block)[1])}},
                                                 {"z", Value{static_cast<std::int64_t>((*info.block)[2])}}}}});
    }
    c.ret(putValue(c.memory(), c.i32(1), c.i32(2), Value{std::move(map)}));
}

void windowClose(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    PluginWindow* window = status == abi::kOk ? player->window() : nullptr;
    if (status != abi::kOk || window == nullptr || window->windowInfo().containerId == 0) {
        c.ret(status != abi::kOk ? status : abi::kErrAbsent);
        return;
    }
    window->closeWindow();
    c.ret(abi::kOk);
}

void windowUpdate(void* user, abi::Call& c) {
    std::shared_ptr<PluginPlayerRef> player;
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(1), c.i32(2));
    std::int32_t status = stateOf(user).resolvePlayer(c.i32(0), player);
    if (status == abi::kOk && (!spec.has_value() || spec->asMap() == nullptr)) {
        status = abi::kErrInvalid;
    }
    PluginWindow* window = status == abi::kOk ? player->window() : nullptr;
    if (status == abi::kOk && (window == nullptr || window->windowInfo().containerId == 0)) {
        status = abi::kErrAbsent;
    }
    if (status != abi::kOk) {
        c.ret(status);
        return;
    }
    if (const Value* title = spec->find("title"); title != nullptr && title->asString() != nullptr) {
        static_cast<void>(player->invoke("window_title", {titleJson(*title->asString())}));
    }
    const Value* props = spec->find("properties");
    for (const MapEntry& entry : props != nullptr && props->asMap() != nullptr ? *props->asMap() : Value::Map{}) {
        std::int32_t id = 0;
        const auto parsed = std::from_chars(entry.key.data(), entry.key.data() + entry.key.size(), id);
        if (entry.value.asInt() == nullptr || parsed.ec != std::errc{} ||
            !window->setWindowProperty(id, static_cast<std::int32_t>(*entry.value.asInt()))) {
            status = abi::kErrAbsent;
        }
    }
    c.ret(status);
}

[[nodiscard]] std::optional<std::int32_t> chestSlots(const XMenu& menu) {
    return menu.kind == "chest" ? std::optional<std::int32_t>{menu.rows * PluginMenus::kColumns} : std::nullopt;
}

void menuCreate(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::expected<Value, std::int32_t> spec = readValue(c.memory(), c.i32(0), c.i32(1));
    if (!spec.has_value() || spec->asMap() == nullptr) {
        c.ret(abi::kErrInvalid);
        return;
    }
    XMenu menu;
    menu.title = textOr(*spec, "title", "");
    menu.kind = textOr(*spec, "kind", "chest");
    const Value* rows = spec->find("rows");
    menu.rows = rows != nullptr && rows->asInt() != nullptr ? static_cast<std::int32_t>(*rows->asInt()) : 3;
    if ((rows != nullptr && !rows->isNull() && rows->asInt() == nullptr) || menu.rows < 1 ||
        menu.rows > PluginMenus::kMaxRows || menu.kind.empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    MenuState& held = menusOf(state);
    if (!held.cleanup) {
        held.cleanup = true;
        State* owner = &state;
        static_cast<void>(state.plugin->registrations.add("container menus", [owner]() {
            menusOf(*owner).menus.forEach([](std::int32_t, XMenu& menu) {
                for (auto& [uuid, handle] : menu.opened) {
                    handle.remove();
                }
            });
        }));
    }
    const std::int32_t handle = held.menus.add(std::move(menu));
    c.ret(handle != 0 ? handle : abi::kErrInvalid);
}

void redraw(State& state, XMenu& menu) {
    for (const auto& [uuid, handle] : menu.opened) {
        const std::shared_ptr<PluginPlayerRef> player = state.host->services().players().byUuid(uuid);
        if (player != nullptr && chestSlots(menu).has_value()) {
            state.host->services().menus().refresh(*player, menu.items);
        }
    }
}

void menuSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    XMenu* menu = nullptr;
    const std::int32_t found = menusOf(state).menus.find(c.i32(0), menu);
    const std::expected<Slot, std::int32_t> item = readStack(state, c.memory(), c.i32(2), c.i32(3));
    if (found != abi::kOk || !item.has_value() || c.i32(1) < 0 || c.i32(1) >= chestSlots(*menu).value_or(54)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    if (item->has_value()) {
        menu->items.insert_or_assign(c.i32(1), **item);
        if (c.i32(4) != 0) {
            menu->tokens.insert_or_assign(c.i32(1), c.i32(4));
        } else {
            menu->tokens.erase(c.i32(1));
        }
    } else {
        menu->items.erase(c.i32(1));
        menu->tokens.erase(c.i32(1));
    }
    redraw(state, *menu);
    c.ret(abi::kOk);
}

void menuClear(void* user, abi::Call& c) {
    State& state = stateOf(user);
    XMenu* menu = nullptr;
    if (menusOf(state).menus.find(c.i32(0), menu) != abi::kOk) {
        c.ret(abi::kErrInvalid);
        return;
    }
    menu->items.clear();
    menu->tokens.clear();
    redraw(state, *menu);
    c.ret(abi::kOk);
}

void clicked(PluginHost& host, const std::shared_ptr<State*>& link, std::int32_t handle,
             const std::shared_ptr<PluginPlayerRef>& player, std::int32_t slot) {
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* state = *link;
    if (state == nullptr) {
        return;
    }
    XMenu* menu = nullptr;
    if (menusOf(*state).menus.find(handle, menu) != abi::kOk || !menu->tokens.contains(slot)) {
        return;
    }
    const std::int32_t token = menu->tokens.at(slot);
    const std::int32_t playerHandle = state->playerHandle(player);
    const Value info{Value::Map{{"player", Value{static_cast<std::int64_t>(playerHandle)}},
                                {"slot", Value{static_cast<std::int64_t>(slot)}},
                                {"button", Value{0}},
                                {"mode", Value{"pickup"}}}};
    std::string result;
    Context context;
    context.kind = Context::Kind::MenuClick;
    context.player = playerHandle;
    context.value = &info;
    context.result = &result;
    static_cast<void>(state->run(token, context, "container menu click"));
    if (result.empty() || menusOf(*state).menus.find(handle, menu) != abi::kOk) {
        return;
    }
    const std::expected<Value, std::string> decoded =
        wire::wasm::decodeValue({reinterpret_cast<const std::uint8_t*>(result.data()), result.size()});
    const std::optional<PluginItem> replaced =
        decoded.has_value() && !decoded->isNull()
            ? fromValue(*decoded, [state](const std::string& id) { return knownItem(*state, id); })
            : std::nullopt;
    if (replaced.has_value()) {
        menu->items.insert_or_assign(slot, *replaced);
    } else {
        menu->items.erase(slot);
    }
    redraw(*state, *menu);
}

void menuOpen(void* user, abi::Call& c) {
    State& state = stateOf(user);
    XMenu* menu = nullptr;
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t found = menusOf(state).menus.find(c.i32(0), menu);
    const std::int32_t status = state.resolvePlayer(c.i32(1), player);
    if (found != abi::kOk || status != abi::kOk) {
        c.ret(found != abi::kOk ? abi::kErrInvalid : status);
        return;
    }
    const std::string uuid = player->uuid();
    if (const auto opened = menu->opened.find(uuid); opened != menu->opened.end()) {
        opened->second.remove();
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    const std::int32_t handle = c.i32(0);
    if (chestSlots(*menu).has_value()) {
        PluginHandle opened = host->services().menus().open(
            *player, menu->title, menu->rows, menu->items,
            [host, link, handle, player](std::int32_t slot) { clicked(*host, link, handle, player, slot); });
        menu->opened.insert_or_assign(uuid, std::move(opened));
        c.ret(abi::kOk);
        return;
    }
    PluginWindow* window = player->window();
    if (window == nullptr) {
        c.ret(abi::kErrAbsent);
        return;
    }
    PluginWindowSpec spec;
    spec.kind = menu->kind;
    spec.title = componentOf(menu->title);
    spec.editable = false;
    for (const auto& [slot, item] : menu->items) {
        if (static_cast<std::size_t>(slot) >= spec.items.size()) {
            spec.items.resize(static_cast<std::size_t>(slot) + 1);
        }
        spec.items[static_cast<std::size_t>(slot)] = item;
    }
    if (window->openWindow(spec) <= 0) {
        c.ret(abi::kErrInvalid);
        return;
    }
    menu->opened.insert_or_assign(uuid, PluginHandle{[window] { window->closeWindow(); }});
    c.ret(abi::kOk);
}

void menuClose(void* user, abi::Call& c) {
    State& state = stateOf(user);
    XMenu* menu = nullptr;
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t found = menusOf(state).menus.find(c.i32(0), menu);
    const std::int32_t status = state.resolvePlayer(c.i32(1), player);
    if (found != abi::kOk || status != abi::kOk) {
        c.ret(found != abi::kOk ? abi::kErrInvalid : status);
        return;
    }
    const auto opened = menu->opened.find(player->uuid());
    if (opened == menu->opened.end()) {
        c.ret(abi::kErrAbsent);
        return;
    }
    if (chestSlots(*menu).has_value()) {
        state.host->services().menus().close(*player);
    }
    opened->second.remove();
    menu->opened.erase(opened);
    c.ret(abi::kOk);
}

[[nodiscard]] std::int32_t clickContext(State& state, std::int32_t id, Context*& context) {
    const std::int32_t status = state.check(id, Context::Kind::MenuClick, context);
    return status == abi::kOk && context->value == nullptr ? abi::kErrInvalid : status;
}

void clickInfo(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = clickContext(stateOf(user), c.i32(0), context);
    c.ret(status != abi::kOk ? status : putValue(c.memory(), c.i32(1), c.i32(2), *context->value));
}

void clickResult(void* user, abi::Call& c) {
    State& state = stateOf(user);
    Context* context = nullptr;
    const std::expected<Slot, std::int32_t> item = readStack(state, c.memory(), c.i32(1), c.i32(2));
    std::int32_t status = clickContext(state, c.i32(0), context);
    if (status == abi::kOk && (!item.has_value() || context->result == nullptr)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        context->result->clear();
        static_cast<void>(wire::wasm::encodeValue(toValue(*item), *context->result));
    }
    c.ret(status);
}

void clickCancel(void* user, abi::Call& c) {
    Context* context = nullptr;
    const std::int32_t status = clickContext(stateOf(user), c.i32(0), context);
    c.ret(status != abi::kOk ? status : (c.i32(1) != 0 ? abi::kOk : abi::kErrDenied));
}
} // namespace

void addItemsExtraImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"recipe_add", recipeAdd, user},
                              {"recipe_remove", recipeRemove, user},
                              {"recipe_has", recipeHas, user},
                              {"recipe_ids", recipeIds, user},
                              {"recipe_clear", recipeClear, user},
                              {"recipe_reset", recipeReset, user},
                              {"recipe_reset_brewing", recipeResetBrewing, user},
                              {"recipe_get", recipeGet, user},
                              {"recipe_station", recipeStation, user},
                              {"recipe_book_get", bookGet, user},
                              {"recipe_book_set", bookSet, user},
                              {"recipe_book_has", bookHas, user},
                              {"recipe_book_resend", bookResend, user},
                              {"merchant_open", merchantOpen, user},
                              {"window_open", windowOpen, user},
                              {"window_info", windowInfo, user},
                              {"window_close", windowClose, user},
                              {"window_update", windowUpdate, user},
                              {"container_menu_create", menuCreate, user},
                              {"container_menu_set", menuSet, user},
                              {"container_menu_clear", menuClear, user},
                              {"container_menu_open", menuOpen, user},
                              {"container_menu_close", menuClose, user},
                              {"container_menu_click_info", clickInfo, user},
                              {"container_menu_click_result", clickResult, user},
                              {"container_menu_click_cancel", clickCancel, user},
                          });
}
} // namespace basalt::server::plugins::wasm
