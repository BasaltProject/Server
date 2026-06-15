#include "BridgeInternal.hpp"

#include <basalt/server/plugins/PluginWindow.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/Snbt.hpp>

#include <array>
#include <chrono>
#include <exception>
#include <future>

namespace basalt::server::bukkit {
namespace {
using plugins::PluginItem;
using wire::nbt::NbtCompound;
using wire::nbt::NbtTag;

[[nodiscard]] plugins::PluginInventories& inventories() { return BukkitBridge::current().services().inventories(); }

template <typename Result, typename Body>
[[nodiscard]] Result onServerThread(Result fallback, Body&& body) {
    const auto run = [&fallback, &body]() -> Result {
        try {
            return body();
        } catch (const std::exception& error) {
            BukkitBridge::current().log().warn([&error] { return std::string{"item call failed: "} + error.what(); });
            return fallback;
        }
    };
    BukkitBridge& bridge = BukkitBridge::current();
    if (bridge.plugins().onPluginThread()) {
        return run();
    }
    std::promise<Result> done;
    std::future<Result> result = done.get_future();
    static_cast<void>(bridge.services().tasks().after(0, [&done, &run] { done.set_value(run()); }));
    while (result.wait_for(std::chrono::milliseconds{50}) != std::future_status::ready) {
        if (bridge.stopping()) {
            return fallback;
        }
    }
    return result.get();
}

[[nodiscard]] std::vector<std::uint8_t> bytesOf(JNIEnv* env, jbyteArray array) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(env->GetArrayLength(array)));
    env->GetByteArrayRegion(array, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<jbyte*>(bytes.data()));
    return bytes;
}

[[nodiscard]] jbyteArray javaBytes(JNIEnv* env, const std::vector<std::uint8_t>& bytes) {
    jbyteArray array = env->NewByteArray(static_cast<jsize>(bytes.size()));
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<const jbyte*>(bytes.data()));
    return array;
}

[[nodiscard]] std::vector<std::uint8_t> encode(const PluginItem& item) {
    NbtCompound root;
    root.putString("id", item.id);
    root.putInt("count", item.count);
    root.set("components", NbtTag::ofCompound(item.components));
    return wire::nbt::nbtIO::writeAnonymous(NbtTag::ofCompound(std::move(root)));
}

[[nodiscard]] std::optional<PluginItem> decode(std::span<const std::uint8_t> bytes) {
    const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read = wire::nbt::nbtIO::readAnonymous(bytes);
    if (!read.has_value()) {
        return std::nullopt;
    }
    const NbtCompound* root = read->tag.get<NbtCompound>();
    if (root == nullptr || !root->contains("id")) {
        return std::nullopt;
    }
    PluginItem item;
    item.id = std::string{root->getString("id")};
    item.count = root->getInt("count", 1);
    if (const NbtCompound* components = root->getCompound("components"); components != nullptr) {
        item.components = *components;
    }
    return item;
}

jbyteArray JNICALL nativePlayerItem(JNIEnv* env, jclass, jstring uuid, jint slot) {
    const std::string who = toStd(env, uuid);
    const std::optional<std::vector<std::uint8_t>> bytes =
        onServerThread<std::optional<std::vector<std::uint8_t>>>(std::nullopt, [&] {
            const auto player = BukkitBridge::current().services().players().byUuid(who);
            if (player == nullptr) {
                return std::optional<std::vector<std::uint8_t>>{};
            }
            const std::optional<PluginItem> item = player->inventory().at(slot);
            return item.has_value() ? std::optional{encode(*item)} : std::nullopt;
        });
    return bytes.has_value() ? javaBytes(env, *bytes) : nullptr;
}

void JNICALL nativeSetPlayerItem(JNIEnv* env, jclass, jstring uuid, jint slot, jbyteArray item) {
    const std::string who = toStd(env, uuid);
    const std::optional<PluginItem> built = item == nullptr ? std::nullopt : decode(bytesOf(env, item));
    onServerThread<bool>(false, [&] {
        if (const auto player = BukkitBridge::current().services().players().byUuid(who); player != nullptr) {
            player->inventory().set(slot, built);
        }
        return true;
    });
}

[[nodiscard]] plugins::PluginWindow* windowOf(const std::string& uuid) {
    const auto player = BukkitBridge::current().services().players().byUuid(uuid);
    return player == nullptr ? nullptr : player->window();
}

jint JNICALL nativeWindowOpen(JNIEnv* env, jclass, jstring uuid, jstring kind, jint size, jbyteArray title,
                              jobjectArray items, jboolean editable) {
    plugins::PluginWindowSpec spec;
    spec.kind = toStd(env, kind);
    spec.size = size;
    spec.editable = editable == JNI_TRUE;
    if (title != nullptr) {
        const std::expected<wire::nbt::AnonymousTag, wire::nbt::NbtError> read =
            wire::nbt::nbtIO::readAnonymous(bytesOf(env, title));
        if (read.has_value()) {
            if (const NbtCompound* wrapper = read->tag.get<NbtCompound>(); wrapper != nullptr) {
                if (const NbtTag* value = wrapper->find("value"); value != nullptr) {
                    spec.title = *value;
                }
            }
        }
    }
    const jsize count = items == nullptr ? 0 : env->GetArrayLength(items);
    for (jsize at = 0; at < count; ++at) {
        auto element = static_cast<jbyteArray>(env->GetObjectArrayElement(items, at));
        spec.items.push_back(element == nullptr ? std::nullopt : decode(bytesOf(env, element)));
        if (element != nullptr) {
            env->DeleteLocalRef(element);
        }
    }
    const std::string who = toStd(env, uuid);
    return onServerThread<jint>(0, [&] {
        plugins::PluginWindow* window = windowOf(who);
        return window == nullptr ? 0 : window->openWindow(spec);
    });
}

void JNICALL nativeWindowClose(JNIEnv* env, jclass, jstring uuid) {
    const std::string who = toStd(env, uuid);
    onServerThread<bool>(false, [&] {
        if (plugins::PluginWindow* window = windowOf(who); window != nullptr) {
            window->closeWindow();
        }
        return true;
    });
}

jobjectArray JNICALL nativeWindowInfo(JNIEnv* env, jclass, jstring uuid) {
    const std::string who = toStd(env, uuid);
    const std::optional<plugins::PluginWindowInfo> info =
        onServerThread<std::optional<plugins::PluginWindowInfo>>(std::nullopt, [&] {
            plugins::PluginWindow* window = windowOf(who);
            return window == nullptr ? std::optional<plugins::PluginWindowInfo>{} : std::optional{window->windowInfo()};
        });
    if (!info.has_value()) {
        return nullptr;
    }
    std::vector<std::string> flat{std::to_string(info->containerId), info->kind, std::to_string(info->slots),
                                  info->plugin ? "1" : "0"};
    if (info->block.has_value()) {
        flat.push_back(info->world);
        for (const std::int32_t coordinate : *info->block) {
            flat.push_back(std::to_string(coordinate));
        }
    }
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(flat.size()), string, nullptr);
    for (std::size_t index = 0; index < flat.size(); ++index) {
        jstring text = toJava(env, flat[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

jbyteArray JNICALL nativeWindowItem(JNIEnv* env, jclass, jstring uuid, jint slot) {
    const std::string who = toStd(env, uuid);
    const std::optional<std::vector<std::uint8_t>> bytes =
        onServerThread<std::optional<std::vector<std::uint8_t>>>(std::nullopt, [&] {
            plugins::PluginWindow* window = windowOf(who);
            const std::optional<PluginItem> item = window == nullptr ? std::nullopt : window->windowItem(slot);
            return item.has_value() ? std::optional{encode(*item)} : std::nullopt;
        });
    return bytes.has_value() ? javaBytes(env, *bytes) : nullptr;
}

void JNICALL nativeSetWindowItem(JNIEnv* env, jclass, jstring uuid, jint slot, jbyteArray item) {
    const std::string who = toStd(env, uuid);
    const std::optional<PluginItem> built = item == nullptr ? std::nullopt : decode(bytesOf(env, item));
    onServerThread<bool>(false, [&] {
        if (plugins::PluginWindow* window = windowOf(who); window != nullptr) {
            window->setWindowItem(slot, built);
        }
        return true;
    });
}

jbyteArray JNICALL nativeCursorItem(JNIEnv* env, jclass, jstring uuid) {
    const std::string who = toStd(env, uuid);
    const std::optional<std::vector<std::uint8_t>> bytes =
        onServerThread<std::optional<std::vector<std::uint8_t>>>(std::nullopt, [&] {
            plugins::PluginWindow* window = windowOf(who);
            const std::optional<PluginItem> item = window == nullptr ? std::nullopt : window->cursorItem();
            return item.has_value() ? std::optional{encode(*item)} : std::nullopt;
        });
    return bytes.has_value() ? javaBytes(env, *bytes) : nullptr;
}

void JNICALL nativeSetCursorItem(JNIEnv* env, jclass, jstring uuid, jbyteArray item) {
    const std::string who = toStd(env, uuid);
    const std::optional<PluginItem> built = item == nullptr ? std::nullopt : decode(bytesOf(env, item));
    onServerThread<bool>(false, [&] {
        if (plugins::PluginWindow* window = windowOf(who); window != nullptr) {
            window->setCursorItem(built);
        }
        return true;
    });
}

void JNICALL nativeWindowResync(JNIEnv* env, jclass, jstring uuid) {
    const std::string who = toStd(env, uuid);
    onServerThread<bool>(false, [&] {
        if (plugins::PluginWindow* window = windowOf(who); window != nullptr) {
            window->resyncWindow();
        }
        return true;
    });
}

void JNICALL nativeSetHeldSlot(JNIEnv* env, jclass, jstring uuid, jint slot) {
    const std::string who = toStd(env, uuid);
    onServerThread<bool>(false, [&] {
        if (const auto player = BukkitBridge::current().services().players().byUuid(who); player != nullptr) {
            player->inventory().setHeldSlot(slot);
        }
        return true;
    });
}

jbyteArray JNICALL nativeItemData(JNIEnv* env, jclass, jstring id) {
    const std::optional<std::vector<std::uint8_t>> data = inventories().itemData(toStd(env, id));
    return data.has_value() ? javaBytes(env, *data) : nullptr;
}

jbyteArray JNICALL nativeEnchantmentData(JNIEnv* env, jclass, jstring id) {
    const std::optional<std::vector<std::uint8_t>> data = inventories().enchantmentData(toStd(env, id));
    return data.has_value() ? javaBytes(env, *data) : nullptr;
}

jobjectArray JNICALL nativeEnchantmentIds(JNIEnv* env, jclass) {
    const std::vector<std::string> ids = inventories().enchantmentIds();
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(ids.size()), string, nullptr);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        jstring text = toJava(env, ids[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

jbyteArray JNICALL nativeCraft(JNIEnv* env, jclass, jint width, jint height, jbyteArray items) {
    const std::vector<std::uint8_t> bytes = bytesOf(env, items);
    const std::optional<std::vector<std::uint8_t>> made = onServerThread<std::optional<std::vector<std::uint8_t>>>(
        std::nullopt, [&] { return inventories().craft(width, height, bytes); });
    return made.has_value() ? javaBytes(env, *made) : nullptr;
}

jobjectArray JNICALL nativeRecipeIds(JNIEnv* env, jclass) {
    const std::vector<std::string> ids =
        onServerThread<std::vector<std::string>>({}, [] { return inventories().recipeIds(); });
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(ids.size()), string, nullptr);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        jstring text = toJava(env, ids[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

jbyteArray JNICALL nativeRecipeData(JNIEnv* env, jclass, jstring id) {
    const std::string name = toStd(env, id);
    const std::optional<std::vector<std::uint8_t>> data = onServerThread<std::optional<std::vector<std::uint8_t>>>(
        std::nullopt, [&] { return inventories().recipeData(name); });
    return data.has_value() ? javaBytes(env, *data) : nullptr;
}

jboolean JNICALL nativeAddRecipe(JNIEnv* env, jclass, jstring id, jbyteArray data) {
    const std::string name = toStd(env, id);
    const std::vector<std::uint8_t> bytes = bytesOf(env, data);
    return onServerThread<bool>(false, [&] { return inventories().addRecipe(name, bytes); }) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeRemoveRecipe(JNIEnv* env, jclass, jstring id) {
    const std::string name = toStd(env, id);
    return onServerThread<bool>(false, [&] { return inventories().removeRecipe(name); }) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeClearRecipes(JNIEnv*, jclass) {
    onServerThread<bool>(false, [] {
        inventories().clearRecipes();
        return true;
    });
}

void JNICALL nativeResetRecipes(JNIEnv*, jclass) {
    onServerThread<bool>(false, [] {
        inventories().resetRecipes();
        return true;
    });
}

jbyteArray JNICALL nativeMobEffects(JNIEnv* env, jclass) {
    const std::optional<std::vector<std::uint8_t>> data = inventories().mobEffects();
    return data.has_value() ? javaBytes(env, *data) : nullptr;
}

jbyteArray JNICALL nativePotions(JNIEnv* env, jclass) {
    const std::optional<std::vector<std::uint8_t>> data = inventories().potions();
    return data.has_value() ? javaBytes(env, *data) : nullptr;
}

jobjectArray JNICALL nativeRegistryTags(JNIEnv* env, jclass, jstring registry) {
    const std::vector<std::string> flat = inventories().registryTags(toStd(env, registry));
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(flat.size()), string, nullptr);
    for (std::size_t index = 0; index < flat.size(); ++index) {
        jstring text = toJava(env, flat[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

jbyteArray JNICALL nativeParseSnbt(JNIEnv* env, jclass, jstring text) {
    const std::expected<NbtTag, wire::nbt::NbtError> parsed = wire::nbt::snbt::parse(toStd(env, text));
    if (!parsed.has_value()) {
        return nullptr;
    }
    NbtCompound wrapper;
    wrapper.set("value", *parsed);
    return javaBytes(env, wire::nbt::nbtIO::writeAnonymous(NbtTag::ofCompound(std::move(wrapper))));
}

} // namespace

void registerInventoryNatives(JNIEnv* env, jclass natives) {
    const std::array methods{
        JNINativeMethod{const_cast<char*>("playerItem"), const_cast<char*>("(Ljava/lang/String;I)[B"),
                        reinterpret_cast<void*>(&nativePlayerItem)},
        JNINativeMethod{const_cast<char*>("setPlayerItem"), const_cast<char*>("(Ljava/lang/String;I[B)V"),
                        reinterpret_cast<void*>(&nativeSetPlayerItem)},
        JNINativeMethod{const_cast<char*>("windowOpen"),
                        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;I[B[[BZ)I"),
                        reinterpret_cast<void*>(&nativeWindowOpen)},
        JNINativeMethod{const_cast<char*>("windowClose"), const_cast<char*>("(Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativeWindowClose)},
        JNINativeMethod{const_cast<char*>("windowInfo"), const_cast<char*>("(Ljava/lang/String;)[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeWindowInfo)},
        JNINativeMethod{const_cast<char*>("windowItem"), const_cast<char*>("(Ljava/lang/String;I)[B"),
                        reinterpret_cast<void*>(&nativeWindowItem)},
        JNINativeMethod{const_cast<char*>("setWindowItem"), const_cast<char*>("(Ljava/lang/String;I[B)V"),
                        reinterpret_cast<void*>(&nativeSetWindowItem)},
        JNINativeMethod{const_cast<char*>("cursorItem"), const_cast<char*>("(Ljava/lang/String;)[B"),
                        reinterpret_cast<void*>(&nativeCursorItem)},
        JNINativeMethod{const_cast<char*>("setCursorItem"), const_cast<char*>("(Ljava/lang/String;[B)V"),
                        reinterpret_cast<void*>(&nativeSetCursorItem)},
        JNINativeMethod{const_cast<char*>("windowResync"), const_cast<char*>("(Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativeWindowResync)},
        JNINativeMethod{const_cast<char*>("setHeldSlot"), const_cast<char*>("(Ljava/lang/String;I)V"),
                        reinterpret_cast<void*>(&nativeSetHeldSlot)},
        JNINativeMethod{const_cast<char*>("itemData"), const_cast<char*>("(Ljava/lang/String;)[B"),
                        reinterpret_cast<void*>(&nativeItemData)},
        JNINativeMethod{const_cast<char*>("enchantmentData"), const_cast<char*>("(Ljava/lang/String;)[B"),
                        reinterpret_cast<void*>(&nativeEnchantmentData)},
        JNINativeMethod{const_cast<char*>("enchantmentIds"), const_cast<char*>("()[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeEnchantmentIds)},
        JNINativeMethod{const_cast<char*>("craft"), const_cast<char*>("(II[B)[B"),
                        reinterpret_cast<void*>(&nativeCraft)},
        JNINativeMethod{const_cast<char*>("recipeIds"), const_cast<char*>("()[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeRecipeIds)},
        JNINativeMethod{const_cast<char*>("recipeData"), const_cast<char*>("(Ljava/lang/String;)[B"),
                        reinterpret_cast<void*>(&nativeRecipeData)},
        JNINativeMethod{const_cast<char*>("addRecipe"), const_cast<char*>("(Ljava/lang/String;[B)Z"),
                        reinterpret_cast<void*>(&nativeAddRecipe)},
        JNINativeMethod{const_cast<char*>("removeRecipe"), const_cast<char*>("(Ljava/lang/String;)Z"),
                        reinterpret_cast<void*>(&nativeRemoveRecipe)},
        JNINativeMethod{const_cast<char*>("clearRecipes"), const_cast<char*>("()V"),
                        reinterpret_cast<void*>(&nativeClearRecipes)},
        JNINativeMethod{const_cast<char*>("resetRecipes"), const_cast<char*>("()V"),
                        reinterpret_cast<void*>(&nativeResetRecipes)},
        JNINativeMethod{const_cast<char*>("mobEffects"), const_cast<char*>("()[B"),
                        reinterpret_cast<void*>(&nativeMobEffects)},
        JNINativeMethod{const_cast<char*>("potions"), const_cast<char*>("()[B"),
                        reinterpret_cast<void*>(&nativePotions)},
        JNINativeMethod{const_cast<char*>("registryTags"), const_cast<char*>("(Ljava/lang/String;)[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeRegistryTags)},
        JNINativeMethod{const_cast<char*>("parseSnbt"), const_cast<char*>("(Ljava/lang/String;)[B"),
                        reinterpret_cast<void*>(&nativeParseSnbt)},
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
