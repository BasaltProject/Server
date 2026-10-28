#include "BridgeInternal.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

namespace basalt::server::bukkit {
namespace {
std::atomic<BukkitBridge*> gCurrent{nullptr};
thread_local plugins::PluginCommandCall* gCurrentCall = nullptr;

constexpr std::string_view kEventOwner = "bukkit";

struct Boxes {
    jclass booleanClass = nullptr;
    jclass longClass = nullptr;
    jclass doubleClass = nullptr;
    jclass stringClass = nullptr;
    jmethodID booleanOf = nullptr;
    jmethodID longOf = nullptr;
    jmethodID doubleOf = nullptr;
    jmethodID booleanValue = nullptr;
    jmethodID longValue = nullptr;
    jmethodID doubleValue = nullptr;
};

Boxes gBoxes;

[[nodiscard]] std::string lowercase(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

[[nodiscard]] jobject box(JNIEnv* env, const plugins::PluginEventValue& value) {
    if (const bool* flag = std::get_if<bool>(&value)) {
        return env->CallStaticObjectMethod(gBoxes.booleanClass, gBoxes.booleanOf, *flag ? JNI_TRUE : JNI_FALSE);
    }
    if (const std::int64_t* whole = std::get_if<std::int64_t>(&value)) {
        return env->CallStaticObjectMethod(gBoxes.longClass, gBoxes.longOf, static_cast<jlong>(*whole));
    }
    if (const double* real = std::get_if<double>(&value)) {
        return env->CallStaticObjectMethod(gBoxes.doubleClass, gBoxes.doubleOf, static_cast<jdouble>(*real));
    }
    if (const std::string* text = std::get_if<std::string>(&value)) {
        return toJava(env, *text);
    }
    return nullptr;
}

[[nodiscard]] plugins::PluginEventValue unbox(JNIEnv* env, jobject object) {
    if (object == nullptr) {
        return {};
    }
    if (env->IsInstanceOf(object, gBoxes.booleanClass) == JNI_TRUE) {
        return env->CallBooleanMethod(object, gBoxes.booleanValue) == JNI_TRUE;
    }
    if (env->IsInstanceOf(object, gBoxes.longClass) == JNI_TRUE) {
        return static_cast<std::int64_t>(env->CallLongMethod(object, gBoxes.longValue));
    }
    if (env->IsInstanceOf(object, gBoxes.doubleClass) == JNI_TRUE) {
        return env->CallDoubleMethod(object, gBoxes.doubleValue);
    }
    if (env->IsInstanceOf(object, gBoxes.stringClass) == JNI_TRUE) {
        return toStd(env, static_cast<jstring>(object));
    }
    return {};
}

[[nodiscard]] jclass globalClass(JNIEnv* env, const char* name) {
    jclass local = env->FindClass(name);
    if (local == nullptr) {
        env->ExceptionClear();
        return nullptr;
    }
    return static_cast<jclass>(env->NewGlobalRef(local));
}
} // namespace

std::string toStd(JNIEnv* env, jstring text) {
    if (text == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(text, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string out{chars};
    env->ReleaseStringUTFChars(text, chars);
    return out;
}

jstring toJava(JNIEnv* env, std::string_view text) { return env->NewStringUTF(std::string{text}.c_str()); }

BukkitBridge::BukkitBridge(ShimHost& shim, plugins::PluginHost& plugins, BukkitOptions options)
    : shim_(&shim), plugins_(&plugins), options_(std::move(options)) {
    gCurrent.store(this);
}

BukkitBridge::~BukkitBridge() { seal(); }

void BukkitBridge::replyToCommand(const std::string& message) {
    if (gCurrentCall != nullptr) {
        gCurrentCall->reply(message);
    }
}

BukkitBridge* BukkitBridge::live() { return gCurrent.load(); }

JNIEnv* BukkitBridge::liveEnv() {
    BukkitBridge* bridge = gCurrent.load();
    return bridge == nullptr ? nullptr : bridge->env();
}

void BukkitBridge::seal() { gCurrent.store(nullptr); }

BukkitBridge& BukkitBridge::current() {
    BukkitBridge* bridge = gCurrent.load();
    // A Java thread outliving the server must not run against the freed model: it waits here until exit.
    while (bridge == nullptr) {
        std::this_thread::sleep_for(std::chrono::hours{1});
        bridge = gCurrent.load();
    }
    return *bridge;
}

std::expected<void, std::string> BukkitBridge::boot() {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return std::unexpected(std::string{"cannot attach to the VM"});
    }
    JniLocalFrame frame{env};
    if (!frame) return std::unexpected(std::string{"cannot allocate a JNI local frame"});
    gBoxes.booleanClass = globalClass(env, "java/lang/Boolean");
    gBoxes.longClass = globalClass(env, "java/lang/Long");
    gBoxes.doubleClass = globalClass(env, "java/lang/Double");
    gBoxes.stringClass = globalClass(env, "java/lang/String");
    if (gBoxes.booleanClass == nullptr || gBoxes.longClass == nullptr || gBoxes.doubleClass == nullptr ||
        gBoxes.stringClass == nullptr) {
        return std::unexpected(std::string{"java.lang classes are missing"});
    }
    gBoxes.booleanOf = env->GetStaticMethodID(gBoxes.booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");
    gBoxes.longOf = env->GetStaticMethodID(gBoxes.longClass, "valueOf", "(J)Ljava/lang/Long;");
    gBoxes.doubleOf = env->GetStaticMethodID(gBoxes.doubleClass, "valueOf", "(D)Ljava/lang/Double;");
    gBoxes.booleanValue = env->GetMethodID(gBoxes.booleanClass, "booleanValue", "()Z");
    gBoxes.longValue = env->GetMethodID(gBoxes.longClass, "longValue", "()J");
    gBoxes.doubleValue = env->GetMethodID(gBoxes.doubleClass, "doubleValue", "()D");

    jclass natives = env->FindClass("org/basaltmc/bukkit/Natives");
    runtime_ = globalClass(env, "org/basaltmc/bukkit/BukkitRuntime");
    if (natives == nullptr || runtime_ == nullptr) {
        reportJavaFailure(env, "finding the runtime classes (is runtime-jar on the class path?)");
        return std::unexpected(std::string{"org.basaltmc.bukkit runtime classes not found"});
    }
    registerNatives(env, natives);
    if (jclass worldNatives = env->FindClass("org/basaltmc/bukkit/WorldNatives"); worldNatives != nullptr) {
        registerWorldNatives(env, worldNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass inventoryNatives = env->FindClass("org/basaltmc/bukkit/InventoryNatives"); inventoryNatives != nullptr) {
        registerInventoryNatives(env, inventoryNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass entityNatives = env->FindClass("org/basaltmc/bukkit/EntityNatives"); entityNatives != nullptr) {
        registerEntityNatives(env, entityNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass miscNatives = env->FindClass("org/basaltmc/bukkit/MiscNatives"); miscNatives != nullptr) {
        registerMiscNatives(env, miscNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass playerNatives = env->FindClass("org/basaltmc/bukkit/PlayerNatives"); playerNatives != nullptr) {
        registerPlayerNatives(env, playerNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass envNatives = env->FindClass("org/basaltmc/bukkit/WorldEnvNatives"); envNatives != nullptr) {
        registerWorldEnvNatives(env, envNatives);
        registerWorldEnvMoreNatives(env, envNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass leftoverNatives = env->FindClass("org/basaltmc/bukkit/LeftoverNatives"); leftoverNatives != nullptr) {
        registerLeftoverNatives(env, leftoverNatives);
    } else {
        env->ExceptionClear();
    }
    if (jclass restNatives = env->FindClass("org/basaltmc/bukkit/RestNatives"); restNatives != nullptr) {
        registerRestNatives(env, restNatives);
    } else {
        env->ExceptionClear();
    }
    bootMethod_ = env->GetStaticMethodID(runtime_, "boot", "(Ljava/lang/String;Ljava/lang/String;)Z");
    enableMethod_ = env->GetStaticMethodID(runtime_, "enable", "(Ljava/lang/String;)Z");
    shutdownMethod_ = env->GetStaticMethodID(runtime_, "shutdown", "()Z");
    taskMethod_ = env->GetStaticMethodID(runtime_, "runTask", "(J)V");
    eventMethod_ = env->GetStaticMethodID(runtime_, "event", "(Ljava/lang/String;IJ[Ljava/lang/Object;Z)Z");
    commandMethod_ = env->GetStaticMethodID(runtime_, "command",
                                            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
                                            "String;DDDLjava/lang/String;Ljava/lang/String;)Z");
    completionMethod_ = env->GetStaticMethodID(runtime_, "completeCommand",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DDDLjava/lang/String;"
        "Ljava/lang/String;)[Ljava/lang/String;");
    channelMethod_ = env->GetStaticMethodID(runtime_, "channelMessage", "(Ljava/lang/String;Ljava/lang/String;[B)V");
    listMethod_ = env->GetStaticMethodID(runtime_, "pluginList", "()[Ljava/lang/String;");
    if (channelMethod_ == nullptr || enableMethod_ == nullptr || bootMethod_ == nullptr || shutdownMethod_ == nullptr ||
        taskMethod_ == nullptr || eventMethod_ == nullptr || commandMethod_ == nullptr || completionMethod_ == nullptr ||
        listMethod_ == nullptr) {
        reportJavaFailure(env, "resolving BukkitRuntime entry points");
        return std::unexpected(std::string{"BukkitRuntime entry points not found"});
    }
    jstring plugins = toJava(env, options_.pluginsFolder);
    jstring data = toJava(env, options_.dataFolder);
    const jboolean ok = env->CallStaticBooleanMethod(runtime_, bootMethod_, plugins, data);
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "BukkitRuntime.boot");
        return std::unexpected(std::string{"BukkitRuntime.boot threw"});
    }
    if (ok != JNI_TRUE) {
        return std::unexpected(std::string{"BukkitRuntime.boot refused to start"});
    }
    return {};
}

bool BukkitBridge::enable() {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return false;
    }
    JniLocalFrame frame{env};
    if (!frame) { return false; }
    jstring plugins = toJava(env, options_.pluginsFolder);
    const jboolean ok = env->CallStaticBooleanMethod(runtime_, enableMethod_, plugins);
    env->DeleteLocalRef(plugins);
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "BukkitRuntime.enable");
        return false;
    }
    return ok == JNI_TRUE;
}

std::vector<BukkitHost::PluginState> BukkitBridge::javaPlugins() {
    std::vector<BukkitHost::PluginState> found;
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return found;
    }
    JniLocalFrame frame{env};
    if (!frame) { return found; }
    auto* rows = static_cast<jobjectArray>(env->CallStaticObjectMethod(runtime_, listMethod_));
    if (env->ExceptionCheck() == JNI_TRUE || rows == nullptr) {
        env->ExceptionClear();
        return found;
    }
    const jsize count = env->GetArrayLength(rows);
    for (jsize index = 0; index < count; ++index) {
        auto* row = static_cast<jstring>(env->GetObjectArrayElement(rows, index));
        const std::string line = toStd(env, row);
        env->DeleteLocalRef(row);
        const std::size_t first = line.find('\t');
        const std::size_t second = line.find('\t', first + 1);
        if (first == std::string::npos || second == std::string::npos) {
            continue;
        }
        found.push_back(
            {line.substr(0, first), line.substr(first + 1, second - first - 1), line.substr(second + 1) == "1"});
    }
    env->DeleteLocalRef(rows);
    return found;
}

bool BukkitBridge::shutdown() {
    stopping_.store(true);
    JNIEnv* env = shim_->env();
    bool clean = false;
    if (env != nullptr && runtime_ != nullptr && shutdownMethod_ != nullptr) {
        JniLocalFrame frame{env};
        if (!frame) return false;
        clean = env->CallStaticBooleanMethod(runtime_, shutdownMethod_) == JNI_TRUE;
        if (env->ExceptionCheck() == JNI_TRUE) {
            reportJavaFailure(env, "BukkitRuntime.shutdown");
            clean = false;
        }
    }
    tasks_.clear();
    channels_.clear();
    commands_.clear();
    eventHandles_.clear();
    plugins_->events().removeAll(kEventOwner);
    return clean;
}

void BukkitBridge::reportJavaFailure(JNIEnv* env, std::string_view where) {
    jthrowable failure = env->ExceptionOccurred();
    env->ExceptionClear();
    std::string trace;
    if (failure != nullptr && runtime_ != nullptr) {
        jmethodID describe = env->GetStaticMethodID(runtime_, "describeFailure", "(Ljava/lang/Throwable;)Ljava/lang/String;");
        if (describe != nullptr) {
            jstring written = static_cast<jstring>(env->CallStaticObjectMethod(runtime_, describe, failure));
            if (env->ExceptionCheck() != JNI_TRUE && written != nullptr) {
                trace = toStd(env, written);
            }
            if (written != nullptr) env->DeleteLocalRef(written);
        }
        // A formatting failure must not replace or leave pending the original.
        if (env->ExceptionCheck() == JNI_TRUE) env->ExceptionClear();
    }
    log_.error([where, &trace] {
        return "Java failure in " + std::string{where} + (trace.empty() ? "" : "\n" + trace);
    });
    if (failure != nullptr) {
        if (trace.empty()) {
            env->Throw(failure);
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        env->DeleteLocalRef(failure);
    }
}

void BukkitBridge::runTask(std::int64_t id) {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return;
    }
    JniLocalFrame frame{env};
    if (!frame) { return; }
    env->CallStaticVoidMethod(runtime_, taskMethod_, static_cast<jlong>(id));
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "a scheduled task");
    }
}

void BukkitBridge::scheduleAfter(std::int64_t id, std::int32_t delay) {
    const std::lock_guard<std::mutex> lock{tasksMutex_};
    tasks_.insert_or_assign(id, plugins::PluginHandle{});
    plugins::PluginHandle handle = services().tasks().after(delay, [this, id] {
        {
            const std::lock_guard<std::mutex> inner{tasksMutex_};
            tasks_.erase(id);
        }
        runTask(id);
    });
    tasks_.insert_or_assign(id, std::move(handle));
}

void BukkitBridge::scheduleEvery(std::int64_t id, std::int32_t delay, std::int32_t period) {
    plugins::PluginHandle handle = services().tasks().every(delay, period, [this, id] { runTask(id); });
    const std::lock_guard<std::mutex> lock{tasksMutex_};
    tasks_.insert_or_assign(id, std::move(handle));
}

void BukkitBridge::cancelTask(std::int64_t id) {
    plugins::PluginHandle handle;
    {
        const std::lock_guard<std::mutex> lock{tasksMutex_};
        const auto found = tasks_.find(id);
        if (found == tasks_.end()) {
            return;
        }
        handle = std::move(found->second);
        tasks_.erase(found);
    }
    handle.remove();
}

bool BukkitBridge::deliver(const std::string& event, std::int32_t priority, Snapshot& snapshot) {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return false;
    }
    JniLocalFrame frame{env};
    if (!frame) { return snapshot.cancelled; }
    const std::vector<std::string>& names = snapshot.type->fields();
    jclass objectClass = env->FindClass("java/lang/Object");
    jobjectArray fields = env->NewObjectArray(static_cast<jsize>(names.size()), objectClass, nullptr);
    env->DeleteLocalRef(objectClass);
    for (std::size_t index = 0; index < names.size(); ++index) {
        jobject value = box(env, snapshot.values[index]);
        env->SetObjectArrayElement(fields, static_cast<jsize>(index), value);
        if (value != nullptr) {
            env->DeleteLocalRef(value);
        }
    }
    jstring name = toJava(env, event);
    const jboolean cancelled = env->CallStaticBooleanMethod(runtime_, eventMethod_, name, static_cast<jint>(priority),
                                                            static_cast<jlong>(snapshot.generation), fields,
                                                            snapshot.cancelled ? JNI_TRUE : JNI_FALSE);
    bool result = snapshot.cancelled;
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "event " + event);
    } else {
        result = cancelled == JNI_TRUE;
        for (std::size_t index = 0; index < names.size(); ++index) {
            if (!snapshot.type->isMutable(names[index])) {
                continue;
            }
            jobject value = env->GetObjectArrayElement(fields, static_cast<jsize>(index));
            snapshot.values[index] = unbox(env, value);
            if (value != nullptr) {
                env->DeleteLocalRef(value);
            }
        }
    }
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(fields);
    snapshot.cancelled = result;
    return result;
}

const plugins::PluginEventType* BukkitBridge::typeNamed(const std::string& name) const {
    if (const plugins::PluginEventType* declared = plugins_->events().typeOf(name); declared != nullptr) {
        return declared;
    }
    for (const plugins::PluginEventType* known : plugins::pluginEventTypes::all()) {
        if (known->name() == name) {
            return known;
        }
    }
    return nullptr;
}

bool BukkitBridge::subscribe(const std::string& event, std::int32_t priority) {
    const plugins::PluginEventType* type = typeNamed(event);
    if (type == nullptr) {
        return false;
    }
    if (!subscribed_.emplace(event, priority).second) {
        return true;
    }
    constexpr std::int32_t kMonitor = 5;
    const bool monitor = priority == kMonitor;
    const auto level = static_cast<plugins::PluginEventPriority>(monitor ? 4 : priority);
    eventHandles_.push_back(plugins_->events().add(
        *type, std::string{kEventOwner}, 0, level, monitor, true, {},
        [this, event, priority, monitor](plugins::PluginEvent& fired) {
            Snapshot snapshot{.type = &fired.type(), .generation = fired.generation(), .cancelled = fired.cancelled()};
            for (std::size_t index = 0; index < fired.type().fields().size(); ++index) {
                snapshot.values.push_back(fired.get(static_cast<std::int32_t>(index)));
            }
            if (!plugins_->onPluginThread()) {
                if (event == "player.login" && !monitor) {
                    auto shared = std::make_shared<Snapshot>(std::move(snapshot));
                    auto done = std::make_shared<std::promise<void>>();
                    std::future<void> finished = done->get_future();
                    static_cast<void>(services().tasks().after(0, [this, event, priority, shared, done] {
                        shared->cancelled = deliver(event, priority, *shared);
                        done->set_value();
                    }));
                    if (finished.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
                        return;
                    }
                    for (std::size_t index = 0; index < shared->values.size(); ++index) {
                        if (fired.type().isMutable(fired.type().fields()[index])) {
                            fired.set(static_cast<std::int32_t>(index), shared->values[index]);
                        }
                    }
                    if (shared->cancelled) {
                        fired.cancel();
                    } else if (fired.cancelled()) {
                        fired.restore();
                    }
                    return;
                }
                if (monitor || !fired.type().cancellable()) {
                    static_cast<void>(
                        services().tasks().after(0, [this, event, priority, moved = std::move(snapshot)]() mutable {
                            deliver(event, priority, moved);
                        }));
                } else if (!warnedAboutThread_) {
                    warnedAboutThread_ = true;
                    log_.warn([&event] {
                        return event + " was fired from a thread plugins do not run on, so no Bukkit listener "
                                       "could change it";
                    });
                }
                return;
            }
            const bool cancelled = deliver(event, priority, snapshot);
            for (std::size_t index = 0; index < snapshot.values.size(); ++index) {
                if (fired.type().isMutable(fired.type().fields()[index])) {
                    fired.set(static_cast<std::int32_t>(index), snapshot.values[index]);
                }
            }
            if (!monitor && fired.type().cancellable()) {
                if (cancelled) {
                    fired.cancel();
                } else if (fired.cancelled()) {
                    fired.restore();
                }
            }
        }));
    return true;
}

bool BukkitBridge::runCommand(const std::string& name, plugins::PluginCommandCall& call) {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return false;
    }
    JniLocalFrame frame{env};
    if (!frame) { return false; }
    const plugins::PluginCommandCall::Argument raw = call.argument("args");
    const std::string* line = std::get_if<std::string>(&raw);
    jstring command = toJava(env, name);
    jstring sender = toJava(env, call.source());
    jstring args = toJava(env, line == nullptr ? std::string_view{} : std::string_view{*line});
    jstring kind = toJava(env, call.kind());
    jstring world = toJava(env, call.world());
    jstring caller = toJava(env, call.caller());
    const std::array<double, 3> at = call.position();
    gCurrentCall = &call;
    const jboolean handled = env->CallStaticBooleanMethod(runtime_, commandMethod_, command, sender, kind, world, at[0],
                                                          at[1], at[2], caller, args);
    gCurrentCall = nullptr;
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "command " + name);
    }
    env->DeleteLocalRef(command);
    env->DeleteLocalRef(sender);
    env->DeleteLocalRef(args);
    env->DeleteLocalRef(kind);
    env->DeleteLocalRef(world);
    env->DeleteLocalRef(caller);
    return handled == JNI_TRUE;
}

std::vector<std::string> BukkitBridge::completeCommand(const std::string& name, plugins::PluginCommandCall& call,
                                                      std::string_view input) {
    if (stopping()) return {};
    JNIEnv* env = shim_->env();
    if (env == nullptr) return {};
    JniLocalFrame frame{env};
    if (!frame) { return {}; }
    jstring command = toJava(env, name);
    jstring sender = toJava(env, call.source());
    jstring args = toJava(env, input);
    jstring kind = toJava(env, call.kind());
    jstring world = toJava(env, call.world());
    jstring caller = toJava(env, call.caller());
    const std::array<double, 3> at = call.position();
    plugins::PluginCommandCall* previous = gCurrentCall;
    gCurrentCall = &call;
    jobjectArray found = static_cast<jobjectArray>(env->CallStaticObjectMethod(runtime_, completionMethod_,
        command, sender, kind, world, at[0], at[1], at[2], caller, args));
    gCurrentCall = previous;
    std::vector<std::string> result;
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "completion for " + name);
    } else if (found != nullptr) {
        // Native Brigadier replaces the greedy args node; Bukkit replaces only
        // its final token. Retain the already typed argument prefix.
        const std::size_t space = input.find_last_of(' ');
        const std::string prefix{space == std::string_view::npos ? std::string_view{} : input.substr(0, space + 1)};
        const jsize count = env->GetArrayLength(found);
        for (jsize index = 0; index < count; ++index) {
            jstring value = static_cast<jstring>(env->GetObjectArrayElement(found, index));
            if (value != nullptr) {
                result.push_back(prefix + toStd(env, value));
                env->DeleteLocalRef(value);
            }
        }
    }
    if (found != nullptr) env->DeleteLocalRef(found);
    for (jstring value : {command, sender, args, kind, world, caller}) env->DeleteLocalRef(value);
    return result;
}

void BukkitBridge::registerCommand(const std::string& plugin, const std::string& name, const std::string& permission) {
    const std::string label = lowercase(name);
    auto spec = plugins::PluginCommandSpec::parse(label + " [args:text]");
    if (!spec.has_value()) {
        log_.warn([&label] { return "command '" + label + "' cannot be registered"; });
        return;
    }
    spec->permission = permission;
    commands_.emplace(plugin, services().commands().registerCompletingCommand(
                                  "bukkit." + lowercase(plugin), *spec,
                                  [this, label](plugins::PluginCommandCall& call) { runCommand(label, call); },
                                  [this, label](plugins::PluginCommandCall& call, std::string_view input) {
                                      return completeCommand(label, call, input);
                                  }));
}

void BukkitBridge::unregisterCommands(const std::string& plugin) {
    const auto [first, last] = commands_.equal_range(plugin);
    for (auto entry = first; entry != last; ++entry) {
        entry->second.remove();
    }
    commands_.erase(first, last);
}
} // namespace basalt::server::bukkit

namespace basalt::server::bukkit {
void BukkitBridge::listenChannel(const std::string& channel) {
    if (channels_.contains(channel)) {
        return;
    }
    channels_.emplace(channel, plugins_->listenNative(channel, [this](const std::string& name, const std::string& uuid,
                                                                      std::span<const std::uint8_t> bytes) {
        JNIEnv* env = shim_->env();
        if (env == nullptr) {
            return;
        }
        JniLocalFrame frame{env};
        if (!frame) return;
        jbyteArray payload = env->NewByteArray(static_cast<jsize>(bytes.size()));
        env->SetByteArrayRegion(payload, 0, static_cast<jsize>(bytes.size()),
                                reinterpret_cast<const jbyte*>(bytes.data()));
        jstring channelName = toJava(env, name);
        jstring player = toJava(env, uuid);
        env->CallStaticVoidMethod(runtime_, channelMethod_, channelName, player, payload);
        if (env->ExceptionCheck() == JNI_TRUE) {
            reportJavaFailure(env, "plugin message on " + name);
        }
        env->DeleteLocalRef(payload);
        env->DeleteLocalRef(channelName);
        env->DeleteLocalRef(player);
    }));
}
} // namespace basalt::server::bukkit
