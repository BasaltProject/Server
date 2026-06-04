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
    bootMethod_ = env->GetStaticMethodID(runtime_, "boot", "(Ljava/lang/String;Ljava/lang/String;)Z");
    shutdownMethod_ = env->GetStaticMethodID(runtime_, "shutdown", "()V");
    taskMethod_ = env->GetStaticMethodID(runtime_, "runTask", "(J)V");
    eventMethod_ = env->GetStaticMethodID(runtime_, "event", "(Ljava/lang/String;IJ[Ljava/lang/Object;Z)Z");
    commandMethod_ =
        env->GetStaticMethodID(runtime_, "command", "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;)Z");
    if (bootMethod_ == nullptr || shutdownMethod_ == nullptr || taskMethod_ == nullptr || eventMethod_ == nullptr ||
        commandMethod_ == nullptr) {
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

void BukkitBridge::shutdown() {
    JNIEnv* env = shim_->env();
    if (env != nullptr && runtime_ != nullptr) {
        env->CallStaticVoidMethod(runtime_, shutdownMethod_);
        if (env->ExceptionCheck() == JNI_TRUE) {
            reportJavaFailure(env, "BukkitRuntime.shutdown");
        }
    }
    tasks_.clear();
    commands_.clear();
    eventHandles_.clear();
    plugins_->events().removeAll(kEventOwner);
}

void BukkitBridge::reportJavaFailure(JNIEnv* env, std::string_view where) {
    log_.error([where] { return "Java failure in " + std::string{where}; });
    env->ExceptionDescribe();
    env->ExceptionClear();
}

void BukkitBridge::runTask(std::int64_t id) {
    JNIEnv* env = shim_->env();
    env->CallStaticVoidMethod(runtime_, taskMethod_, static_cast<jlong>(id));
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "a scheduled task");
    }
}

void BukkitBridge::scheduleAfter(std::int64_t id, std::int32_t delay) {
    tasks_.insert_or_assign(id, services().tasks().after(delay, [this, id] {
                                tasks_.erase(id);
                                runTask(id);
                            }));
}

void BukkitBridge::scheduleEvery(std::int64_t id, std::int32_t delay, std::int32_t period) {
    tasks_.insert_or_assign(id, services().tasks().every(delay, period, [this, id] { runTask(id); }));
}

void BukkitBridge::cancelTask(std::int64_t id) {
    const auto found = tasks_.find(id);
    if (found == tasks_.end()) {
        return;
    }
    plugins::PluginHandle handle = std::move(found->second);
    tasks_.erase(found);
    handle.remove();
}

bool BukkitBridge::deliver(const std::string& event, std::int32_t priority, plugins::PluginEvent& fired) {
    JNIEnv* env = shim_->env();
    const std::vector<std::string>& names = fired.type().fields();
    jobjectArray fields = env->NewObjectArray(static_cast<jsize>(names.size()), env->FindClass("java/lang/Object"),
                                              nullptr);
    for (std::size_t index = 0; index < names.size(); ++index) {
        jobject value = box(env, fired.get(static_cast<std::int32_t>(index)));
        env->SetObjectArrayElement(fields, static_cast<jsize>(index), value);
        if (value != nullptr) {
            env->DeleteLocalRef(value);
        }
    }
    jstring name = toJava(env, event);
    const jboolean cancelled =
        env->CallStaticBooleanMethod(runtime_, eventMethod_, name, static_cast<jint>(priority),
                                     static_cast<jlong>(fired.generation()), fields,
                                     fired.cancelled() ? JNI_TRUE : JNI_FALSE);
    bool result = fired.cancelled();
    if (env->ExceptionCheck() == JNI_TRUE) {
        reportJavaFailure(env, "event " + event);
    } else {
        result = cancelled == JNI_TRUE;
        for (std::size_t index = 0; index < names.size(); ++index) {
            if (!fired.type().isMutable(names[index])) {
                continue;
            }
            jobject value = env->GetObjectArrayElement(fields, static_cast<jsize>(index));
            fired.set(static_cast<std::int32_t>(index), unbox(env, value));
            if (value != nullptr) {
                env->DeleteLocalRef(value);
            }
        }
    }
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(fields);
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
            const bool cancelled = deliver(event, priority, fired);
            if (cancelled && !monitor && fired.type().cancellable()) {
                fired.cancel();
            }
        }));
    return true;
}

bool BukkitBridge::runCommand(const std::string& name, plugins::PluginCommandCall& call) {
    JNIEnv* env = shim_->env();
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

void BukkitBridge::registerCommand(const std::string& plugin, const std::string& name,
                                   const std::string& permission) {
    const std::string label = lowercase(name);
    auto spec = plugins::PluginCommandSpec::parse(label + " [args:text]");
    if (!spec.has_value()) {
        log_.warn([&label] { return "command '" + label + "' cannot be registered"; });
        return;
    }
    if (!permission.empty()) {
        spec->permission = permission;
    }
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
