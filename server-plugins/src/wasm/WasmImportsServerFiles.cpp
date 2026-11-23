#include "../PluginFilePath.hpp"
#include "WasmServerSupport.hpp"

#include <basalt/server/plugins/PluginFiles.hpp>
#include <basalt/wire/common/ByteView.hpp>

#include <algorithm>
#include <set>

namespace basalt::server::plugins::wasm {
namespace {
using namespace server;
namespace files = fileService;

struct Resolved {
    std::int32_t status = abi::kOk;
    std::string relative;
    std::string live;
};

[[nodiscard]] bool grants(const std::vector<std::string>& granted, std::string_view relative) {
    return std::ranges::any_of(granted, [relative](std::string_view entry) {
        while (entry.ends_with('/')) {
            entry.remove_suffix(1);
        }
        if (entry.empty() || entry == "." || entry == "*") {
            return true;
        }
        return relative == entry || (relative.starts_with(entry) && relative[entry.size()] == '/');
    });
}

[[nodiscard]] std::string normalized(std::string_view path) {
    std::string out{path};
    while (out.starts_with("./")) {
        out.erase(0, 2);
    }
    while (out.size() > 1 && out.ends_with('/')) {
        out.pop_back();
    }
    return out == "." ? std::string{} : out;
}

[[nodiscard]] Resolved resolve(State& state, abi::Call& c, std::size_t pathIndex) {
    Resolved out;
    const std::optional<std::string_view> path = c.memory().string(c.i32(pathIndex), c.i32(pathIndex + 1));
    if (!state.capable("files")) {
        out.status = abi::kErrDenied;
        return out;
    }
    if (!path) {
        out.status = abi::kErrInvalid;
        return out;
    }
    out.relative = normalized(*path);
    const std::optional<std::string> live = pluginApi::pluginFilePath(state.plugin->folder, out.relative);
    if (!live) {
        out.status = abi::kErrInvalid;
        return out;
    }
    if (!grants(state.plugin->manifest.capabilities.files, out.relative)) {
        out.status = abi::kErrDenied;
        return out;
    }
    out.live = *live;
    return out;
}

void filesRead(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const Resolved at = resolve(state, c, 0);
    if (at.status != abi::kOk) {
        c.ret(at.status);
        return;
    }
    std::optional<std::vector<std::uint8_t>> bytes = files::read(at.live);
    if (!bytes) {
        bytes = state.plugin->pkg->read(std::string{wire::plugins::packageLayout::kResources} + at.relative);
    }
    if (!bytes) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(c.memory().put(c.i32(2), c.i32(3), wire::common::asText(*bytes)));
}

void filesWrite(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const Resolved at = resolve(state, c, 0);
    const std::optional<std::string_view> data = c.memory().string(c.i32(2), c.i32(3));
    if (at.status != abi::kOk || !data || at.relative.empty()) {
        c.ret(at.status != abi::kOk ? at.status : abi::kErrInvalid);
        return;
    }
    const std::size_t slash = at.live.rfind('/');
    if (slash != std::string::npos) {
        static_cast<void>(files::makeDirectories(at.live.substr(0, slash)));
    }
    const std::span<const std::uint8_t> raw = wire::common::asBytes(*data);
    c.ret(files::write(at.live, std::vector<std::uint8_t>{raw.begin(), raw.end()}) ? abi::kOk : abi::kErrAbsent);
}

void filesExists(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const Resolved at = resolve(state, c, 0);
    if (at.status != abi::kOk) {
        c.ret(at.status);
        return;
    }
    c.ret(files::stat(at.live) ||
                  state.plugin->pkg->exists(std::string{wire::plugins::packageLayout::kResources} + at.relative)
              ? 1
              : 0);
}

void filesList(void* user, abi::Call& c) {
    State& state = stateOf(user);
    const std::optional<std::string_view> given = c.memory().string(c.i32(0), c.i32(1));
    if (!state.capable("files") || !given) {
        c.ret(!state.capable("files") ? abi::kErrDenied : abi::kErrInvalid);
        return;
    }
    const std::string prefix = normalized(*given);
    if (!pluginApi::pluginFilePath(state.plugin->folder, prefix)) {
        c.ret(abi::kErrInvalid);
        return;
    }
    std::set<std::string> names;
    for (const auto& [relative, path] : state.plugin->files) {
        if (relative.starts_with(prefix) && grants(state.plugin->manifest.capabilities.files, relative)) {
            names.insert(relative);
        }
    }
    for (const std::string& entry : files::walk(state.plugin->folder)) {
        const std::string relative = entry.substr(2);
        if (entry.front() == 'f' && relative.starts_with(prefix) &&
            grants(state.plugin->manifest.capabilities.files, relative)) {
            names.insert(relative);
        }
    }
    c.ret(returnValue(c, 2, listOfText({names.begin(), names.end()})));
}

void filesStat(void* user, abi::Call& c) {
    const Resolved at = resolve(stateOf(user), c, 0);
    if (at.status != abi::kOk) {
        c.ret(at.status);
        return;
    }
    const std::optional<files::Stat> found = files::stat(at.live);
    if (!found) {
        c.ret(abi::kErrAbsent);
        return;
    }
    c.ret(returnValue(c, 2,
                      mapOf({{"kind", Value{found->kind == files::Kind::File        ? "file"
                                            : found->kind == files::Kind::Directory ? "directory"
                                                                                    : "other"}},
                             {"size", Value{found->size}},
                             {"modified", Value{found->modifiedMillis}}})));
}

void filesWalk(void* user, abi::Call& c) {
    const Resolved at = resolve(stateOf(user), c, 0);
    if (at.status != abi::kOk) {
        c.ret(at.status);
        return;
    }
    const std::optional<files::Stat> found = files::stat(at.live);
    if (!found || found->kind != files::Kind::Directory) {
        c.ret(abi::kErrAbsent);
        return;
    }
    std::vector<std::string> paths;
    for (const std::string& entry : files::walk(at.live)) {
        paths.push_back(entry.substr(2) + (entry.front() == 'd' ? "/" : ""));
    }
    c.ret(returnValue(c, 2, listOfText(paths)));
}

void filesDelete(void* user, abi::Call& c) {
    const Resolved at = resolve(stateOf(user), c, 0);
    if (at.status != abi::kOk || at.relative.empty()) {
        c.ret(at.status != abi::kOk ? at.status : abi::kErrInvalid);
        return;
    }
    if (!files::stat(at.live)) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const bool done = c.i32(2) != 0 ? files::removeTree(at.live) : files::remove(at.live);
    c.ret(done ? abi::kOk : abi::kErrInvalid);
}

void filesCapabilities(void* user, abi::Call& c) {
    const Resolved at = resolve(stateOf(user), c, 0);
    if (at.status != abi::kOk) {
        c.ret(at.status);
        return;
    }
    const std::optional<files::Stat> found = files::stat(at.live);
    if (!found || found->kind != files::Kind::Directory) {
        c.ret(abi::kErrAbsent);
        return;
    }
    const files::Capabilities caps = files::capabilities(at.live);
    c.ret(returnValue(c, 2, mapOf({{"atomic_move", Value{caps.atomicMove}}, {"hard_links", Value{caps.hardLinks}}})));
}
} // namespace

void addServerFilesImports(State& state, std::vector<abi::HostImport>& out) {
    void* user = &state;
    out.insert(out.end(), {
                              {"files_read", filesRead, user},
                              {"files_write", filesWrite, user},
                              {"files_exists", filesExists, user},
                              {"files_list", filesList, user},
                              {"files_stat", filesStat, user},
                              {"files_walk", filesWalk, user},
                              {"files_delete", filesDelete, user},
                              {"files_capabilities", filesCapabilities, user},
                          });
}
} // namespace basalt::server::plugins::wasm
