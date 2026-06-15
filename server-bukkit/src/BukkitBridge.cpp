#include "BridgeInternal.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>

#include <algorithm>
#include <cctype>

namespace basalt::server::bukkit {
namespace {
BukkitBridge* gCurrent = nullptr;

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
    gCurrent = this;
}

BukkitBridge& BukkitBridge::current() { return *gCurrent; }

std::expected<void, std::string> BukkitBridge::boot() {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return std::unexpected(std::string{"cannot attach to the VM"});
    }
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
    bootMethod_ = env->GetStaticMethodID(runtime_, "boot", "(Ljava/lang/String;Ljava/lang/String;)Z");
    enableMethod_ = env->GetStaticMethodID(runtime_, "enable", "(Ljava/lang/String;)Z");
    shutdownMethod_ = env->GetStaticMethodID(runtime_, "shutdown", "()Z");
    taskMethod_ = env->GetStaticMethodID(runtime_, "runTask", "(J)V");
    eventMethod_ = env->GetStaticMethodID(runtime_, "event", "(Ljava/lang/String;IJ[Ljava/lang/Object;Z)Z");
    commandMethod_ =
        env->GetStaticMethodID(runtime_, "command", "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;)Z");
    channelMethod_ = env->GetStaticMethodID(runtime_, "channelMessage", "(Ljava/lang/String;Ljava/lang/String;[B)V");
    listMethod_ = env->GetStaticMethodID(runtime_, "pluginList", "()[Ljava/lang/String;");
    if (channelMethod_ == nullptr || enableMethod_ == nullptr || bootMethod_ == nullptr || shutdownMethod_ == nullptr ||
        taskMethod_ == nullptr || eventMethod_ == nullptr || commandMethod_ == nullptr || listMethod_ == nullptr) {
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
    if (env != nullptr && runtime_ != nullptr) {
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
    log_.error([where] { return "Java failure in " + std::string{where}; });
    env->ExceptionDescribe();
    env->ExceptionClear();
}

void BukkitBridge::runTask(std::int64_t id) {
    JNIEnv* env = shim_->env();
    if (env == nullptr) {
        return;
    }
    env->CallStaticVoidMethod(runtime_, taskMethod_, static_cast<jlong>(id));
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "a scheduled task");
    }
}

void BukkitBridge::scheduleAfter(std::int64_t id, std::int32_t delay) {
    plugins::PluginHandle handle = services().tasks().after(delay, [this, id] {
        {
            const std::lock_guard<std::mutex> lock{tasksMutex_};
            tasks_.erase(id);
        }
        runTask(id);
    });
    const std::lock_guard<std::mutex> lock{tasksMutex_};
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
    const std::vector<std::string>& names = snapshot.type->fields();
    jobjectArray fields =
        env->NewObjectArray(static_cast<jsize>(names.size()), env->FindClass("java/lang/Object"), nullptr);
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
    const plugins::PluginCommandCall::Argument raw = call.argument("args");
    const std::string* line = std::get_if<std::string>(&raw);
    jstring command = toJava(env, name);
    jstring sender = toJava(env, call.source());
    jstring args = toJava(env, line == nullptr ? std::string_view{} : std::string_view{*line});
    const jboolean handled = env->CallStaticBooleanMethod(runtime_, commandMethod_, command, sender,
                                                          call.isConsole() ? JNI_TRUE : JNI_FALSE, args);
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "command " + name);
    }
    env->DeleteLocalRef(command);
    env->DeleteLocalRef(sender);
    env->DeleteLocalRef(args);
    return handled == JNI_TRUE;
}

void BukkitBridge::registerCommand(const std::string& plugin, const std::string& name, const std::string& permission) {
    const std::string label = lowercase(name);
    auto spec = plugins::PluginCommandSpec::parse(label + " [args:text]");
    if (!spec.has_value()) {
        log_.warn([&label] { return "command '" + label + "' cannot be registered"; });
        return;
    }
    spec->permission = permission;
    commands_.emplace(plugin, services().commands().registerCommand(
                                  "bukkit." + lowercase(plugin), *spec,
                                  [this, label](plugins::PluginCommandCall& call) { runCommand(label, call); }));
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
