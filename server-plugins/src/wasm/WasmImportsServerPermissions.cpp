#include "WasmServerSupport.hpp"

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;

constexpr std::int32_t kCheckKind = 1;
constexpr std::int32_t kMetaKind = 2;

void permissionCheck(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> subject = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> node = memory.string(c.i32(2), c.i32(3));
    if (!subject || !node || node->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const ImportScope scope;
    const Subject who = resolveSubject(*state.host, *subject);
    if (!who.known) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const std::int32_t answer = state.host->services().permissions().check(who.uuid, *node);
    c.ret(answer == permissions::kUndefined ? abi::kErrAbsent : (answer == permissions::kTrue ? 1 : 0));
}

void permissionMeta(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const abi::GuestMemory memory = c.memory();
    const std::optional<std::string_view> subject = memory.string(c.i32(0), c.i32(1));
    const std::optional<std::string_view> key = memory.string(c.i32(2), c.i32(3));
    if (!subject || !key || key->empty()) {
        c.ret(abi::kErrInvalid);
        return;
    }
    const ImportScope scope;
    const Subject who = resolveSubject(*state.host, *subject);
    const std::optional<std::string> found =
        who.known ? state.host->services().permissions().meta(who.uuid, *key) : std::nullopt;
    c.ret(found ? memory.put(c.i32(4), c.i32(5), *found) : abi::kErrAbsent);
}

void permissionHas(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> node = c.memory().string(c.i32(1), c.i32(2));
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk || !node || node->empty()) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const ImportScope scope;
    c.ret(player->may(*node) ? 1 : 0);
}

void permissionSet(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> node = c.memory().string(c.i32(1), c.i32(2));
    const std::int32_t mode = c.i32(3);
    std::shared_ptr<PluginPlayerRef> player;
    const std::int32_t status = state.resolvePlayer(c.i32(0), player);
    if (status != abi::kOk || !node || node->empty() || mode < -1 || mode > 1) {
        c.ret(status != abi::kOk ? status : abi::kErrInvalid);
        return;
    }
    const ImportScope scope;
    player->setPermission(state.plugin->manifest.id, *node, mode < 0 ? std::nullopt : std::optional<bool>{mode == 1});
    c.ret(abi::kOk);
}

[[nodiscard]] std::optional<std::string> askProvider(PluginHost& host, const std::shared_ptr<State*>& link,
                                                     std::int32_t token, const Value& query) {
    if (insideImport() || !host.onPluginThread()) {
        return std::nullopt;
    }
    const std::lock_guard<std::recursive_mutex> turn{host.turnMutex()};
    State* live = *link;
    if (live == nullptr || live->depth > 0) {
        return std::nullopt;
    }
    std::string answer;
    Context context;
    context.kind = Context::Kind::Completion;
    context.value = &query;
    context.result = &answer;
    if (!live->run(token, context, "permission provider")) {
        return std::nullopt;
    }
    return answer;
}

[[nodiscard]] Value queryOf(PluginHost& host, std::string_view kind, std::string_view uuid, std::string_view argument) {
    const std::shared_ptr<PluginPlayerRef> player = uuid.empty() ? nullptr : host.services().players().byUuid(uuid);
    return mapOf(
        {{"kind", Value{std::string{kind}}},
         {"name", Value{player == nullptr ? (uuid.empty() ? std::string{"CONSOLE"} : std::string{}) : player->name()}},
         {"uuid", Value{std::string{uuid}}},
         {"console", Value{uuid.empty()}},
         {"node", Value{std::string{argument}}},
         {"world", Value{player == nullptr ? std::string{} : player->world()}}});
}

void permissionProvide(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::int32_t priority = c.i32(0);
    const std::int32_t kinds = c.i32(1);
    const std::int32_t token = c.i32(2);
    if (priority <= 0 || kinds < 1 || kinds > (kCheckKind | kMetaKind)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    PluginHost* host = state.host;
    const std::shared_ptr<State*> link = state.link;
    permissions::Check check;
    permissions::Meta meta;
    if ((kinds & kCheckKind) != 0) {
        check = [host, link, token](std::string_view uuid, std::string_view node) {
            const std::optional<std::string> answer =
                askProvider(*host, link, token, queryOf(*host, "check", uuid, node));
            return !answer || answer->empty() || (*answer)[0] == '-'
                       ? permissions::kUndefined
                       : ((*answer)[0] == '1' ? permissions::kTrue : permissions::kFalse);
        };
    }
    if ((kinds & kMetaKind) != 0) {
        meta = [host, link, token](std::string_view uuid, std::string_view key) -> std::optional<std::string> {
            const std::optional<std::string> answer =
                askProvider(*host, link, token, queryOf(*host, "meta", uuid, key));
            if (!answer || answer->empty() || (*answer)[0] != '=') {
                return std::nullopt;
            }
            return answer->substr(1);
        };
    }
    PluginHandle registered = host->services().permissions().provide("wasm:" + state.plugin->manifest.id, priority,
                                                                     std::move(check), std::move(meta));
    const std::int32_t handle = area(state).providers.add(keep(state, "permissions.provide", std::move(registered)));
    c.ret(handle != 0 ? handle : abi::kErrInvalid);
}

void permissionProviderRemove(void* user, abi::Call& c) {
    Area& held = area(stateOf(user));
    PluginHandle* handle = nullptr;
    if (held.providers.find(c.i32(0), handle) != abi::kOk) {
        c.ret(abi::kErrAbsent);
        return;
    }
    handle->remove();
    static_cast<void>(held.providers.remove(c.i32(0)));
    c.ret(abi::kOk);
}

[[nodiscard]] std::int32_t queryContext(State& state, std::int32_t id, std::string_view kind, Context*& context) {
    const std::int32_t status = state.check(id, Context::Kind::Completion, context);
    if (status != abi::kOk) {
        return status;
    }
    return context->result != nullptr && context->value != nullptr && textOf(context->value->find("kind")) == kind
               ? abi::kOk
               : abi::kErrInvalid;
}

void permissionAnswer(void* user, abi::Call& c) {
    Context* context = nullptr;
    std::int32_t status = queryContext(stateOf(user), c.i32(0), "check", context);
    if (status == abi::kOk && (c.i32(1) < -1 || c.i32(1) > 1)) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        *context->result = c.i32(1) < 0 ? "-" : (c.i32(1) == 1 ? "1" : "0");
    }
    c.ret(status);
}

void permissionAnswerMeta(void* user, abi::Call& c) {
    const std::optional<std::string_view> text = c.memory().string(c.i32(1), c.i32(2));
    Context* context = nullptr;
    std::int32_t status = queryContext(stateOf(user), c.i32(0), "meta", context);
    if (status == abi::kOk && !text) {
        status = abi::kErrInvalid;
    }
    if (status == abi::kOk) {
        *context->result = "=" + std::string{*text};
    }
    c.ret(status);
}

void permissionChanged(void* user, abi::Call& c) {
    const ImportScope scope;
    stateOf(user).host->services().permissions().changed();
    c.ret(abi::kOk);
}
} // namespace

void addServerPermissionsImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"permission_check", permissionCheck, user},
                              {"permission_meta", permissionMeta, user},
                              {"permission_has", permissionHas, user},
                              {"permission_set", permissionSet, user},
                              {"permission_provide", permissionProvide, user},
                              {"permission_provider_remove", permissionProviderRemove, user},
                              {"permission_answer", permissionAnswer, user},
                              {"permission_answer_meta", permissionAnswerMeta, user},
                              {"permission_changed", permissionChanged, user},
                          });
}
} // namespace basalt::server::plugins::wasm
