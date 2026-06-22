#include "JniSupport.hpp"

#include <basalt/server/plugins/PluginWindow.hpp>
#include <basalt/server/plugins/PluginWorldEnv.hpp>

namespace basalt::server::bukkit {
namespace {
using Blob = std::optional<std::vector<std::uint8_t>>;

[[nodiscard]] jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

[[nodiscard]] plugins::PluginStructures& structures() { return BukkitBridge::current().services().structures(); }

jobjectArray JNICALL nativeStructureIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] { return structures().ids(); }));
}

jbyteArray JNICALL nativeStructureRead(JNIEnv* env, jclass, jstring id) {
    const std::string name = toStd(env, id);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] { return structures().read(name); }));
}

jboolean JNICALL nativeStructureStore(JNIEnv* env, jclass, jstring id, jbyteArray nbt, jboolean toDisk) {
    const std::string name = toStd(env, id);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    return flag(jni::onServer<bool>(false, [&] { return structures().store(name, data, toDisk == JNI_TRUE); }));
}

jboolean JNICALL nativeStructureRemove(JNIEnv* env, jclass, jstring id, jboolean fromDisk) {
    const std::string name = toStd(env, id);
    return flag(jni::onServer<bool>(false, [&] { return structures().remove(name, fromDisk == JNI_TRUE); }));
}

jstring JNICALL nativeStructureFile(JNIEnv* env, jclass, jstring id) {
    const std::string name = toStd(env, id);
    return toJava(env, jni::onServer<std::string>({}, [&] { return structures().file(name); }));
}

jbyteArray JNICALL nativeStructureCapture(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jint sizeX,
                                          jint sizeY, jint sizeZ, jboolean entities) {
    const std::string name = toStd(env, world);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                return structures().capture(name, x, y, z, sizeX, sizeY, sizeZ, entities == JNI_TRUE);
                            }));
}

jboolean JNICALL nativeStructurePlace(JNIEnv* env, jclass, jbyteArray nbt, jstring world, jint x, jint y, jint z,
                                      jint rotation, jint mirror, jfloat integrity, jlong seed, jboolean entities,
                                      jint palette) {
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    const plugins::PluginStructurePlacement placement{.world = toStd(env, world),
                                                      .x = x,
                                                      .y = y,
                                                      .z = z,
                                                      .rotation = rotation,
                                                      .mirror = mirror,
                                                      .integrity = integrity,
                                                      .seed = seed,
                                                      .entities = entities == JNI_TRUE,
                                                      .palette = palette};
    return flag(jni::onServer<bool>(false, [&] { return structures().place(data, placement); }));
}

jbyteArray JNICALL nativeExplorerMap(JNIEnv* env, jclass, jstring world, jdouble x, jdouble y, jdouble z,
                                     jobjectArray destination, jstring decoration, jint zoom, jint radius,
                                     jboolean skipExisting) {
    const std::string name = toStd(env, world);
    plugins::PluginExploration request;
    request.destination = jni::stringsOf(env, destination);
    request.decoration = toStd(env, decoration);
    request.zoom = zoom;
    request.radius = radius;
    request.skipExisting = skipExisting == JNI_TRUE;
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                return BukkitBridge::current().services().maps().explorerMap(name, x, y, z, request);
                            }));
}

jbyteArray JNICALL nativeRegistryData(JNIEnv* env, jclass, jstring registry, jstring id) {
    const std::string kind = toStd(env, registry);
    const std::string name = toStd(env, id);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                return BukkitBridge::current().services().worlds().registryData(kind, name);
                            }));
}

jobjectArray JNICALL nativeEntityDefaults(JNIEnv* env, jclass, jstring type) {
    const std::string name = toStd(env, type);
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [&] {
                                  return BukkitBridge::current().services().worlds().entityDefaultAttributes(name);
                              }));
}

jdouble JNICALL nativeHumidity(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const std::string name = toStd(env, world);
    return jni::onServer<double>(0.4, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        plugins::PluginWorldEnv* target = found == nullptr ? nullptr : found->env();
        return target == nullptr ? 0.4 : target->humidity(x, y, z);
    });
}

jboolean JNICALL nativeDragonAction(JNIEnv* env, jclass, jstring world, jstring action, jint value) {
    const std::string name = toStd(env, world);
    const std::string what = toStd(env, action);
    return flag(jni::onServer<bool>(false, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        return found != nullptr && found->dragonFightAction(what, value);
    }));
}

jboolean JNICALL nativeWindowProperty(JNIEnv* env, jclass, jstring uuid, jint id, jint value) {
    const std::string who = toStd(env, uuid);
    return flag(jni::onServer<bool>(false, [&] {
        const auto player = BukkitBridge::current().services().players().byUuid(who);
        plugins::PluginWindow* window = player == nullptr ? nullptr : player->window();
        return window != nullptr && window->setWindowProperty(id, value);
    }));
}

jobjectArray JNICALL nativeEnchantItem(JNIEnv* env, jclass, jobjectArray stack, jstring world, jstring options,
                                       jint levels, jlong seed) {
    const std::vector<std::string> given = jni::stringsOf(env, stack);
    if (given.size() < 3) {
        return nullptr;
    }
    const plugins::PluginLootItem item{.item = given[0], .count = std::atoi(given[1].c_str()), .components = given[2]};
    const plugins::PluginLootEnchant how{
        .world = toStd(env, world), .options = toStd(env, options), .levels = levels, .seed = seed};
    const auto made = jni::onServer<std::optional<plugins::PluginLootItem>>(
        std::nullopt, [&] { return BukkitBridge::current().services().loot().enchant(item, how); });
    if (!made.has_value()) {
        return nullptr;
    }
    return jni::stringArrayOf(env, {made->item, std::to_string(made->count), made->components});
}
} // namespace

void registerLeftoverNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("structureIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeStructureIds)),
        method("structureRead", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeStructureRead)),
        method("structureStore", "(Ljava/lang/String;[BZ)Z", reinterpret_cast<void*>(&nativeStructureStore)),
        method("structureRemove", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeStructureRemove)),
        method("structureFile", "(Ljava/lang/String;)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeStructureFile)),
        method("structureCapture", "(Ljava/lang/String;IIIIIIZ)[B", reinterpret_cast<void*>(&nativeStructureCapture)),
        method("structurePlace", "([BLjava/lang/String;IIIIIFJZI)Z", reinterpret_cast<void*>(&nativeStructurePlace)),
        method("registryData", "(Ljava/lang/String;Ljava/lang/String;)[B",
               reinterpret_cast<void*>(&nativeRegistryData)),
        method("entityDefaults", "(Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeEntityDefaults)),
        method("humidity", "(Ljava/lang/String;III)D", reinterpret_cast<void*>(&nativeHumidity)),
        method("dragonAction", "(Ljava/lang/String;Ljava/lang/String;I)Z",
               reinterpret_cast<void*>(&nativeDragonAction)),
        method("windowProperty", "(Ljava/lang/String;II)Z", reinterpret_cast<void*>(&nativeWindowProperty)),
        method("enchantItem", "([Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;IJ)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeEnchantItem)),
        method("explorerMap", "(Ljava/lang/String;DDD[Ljava/lang/String;Ljava/lang/String;IIZ)[B",
               reinterpret_cast<void*>(&nativeExplorerMap)),
    };
    if (env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size())) != 0) {
        BukkitBridge::current().log().error(
            [] { return std::string{"LeftoverNatives has a native the runtime does not declare"}; });
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}
} // namespace basalt::server::bukkit
