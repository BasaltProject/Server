#include "BukkitMiscNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

jbyteArray JNICALL nativeWorldData(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    return world == nullptr
               ? nullptr
               : jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] { return Blob{world->pluginData()}; }));
}

void JNICALL nativeSetWorldData(JNIEnv* env, jclass, jstring name, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    if (world != nullptr) {
        static_cast<void>(jni::onServer<bool>(false, [&] { return world->setPluginData(data); }));
    }
}

jbyteArray JNICALL nativeChunkData(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world == nullptr ? nullptr : jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                                             return world->chunkPluginData(chunkX, chunkZ);
                                                         }));
}

void JNICALL nativeSetChunkData(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    if (world != nullptr) {
        static_cast<void>(jni::onServer<bool>(false, [&] { return world->setChunkPluginData(chunkX, chunkZ, data); }));
    }
}

jbyteArray JNICALL nativeBlockEntityData(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    return world == nullptr ? nullptr : jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                                             return world->blockEntityPluginData(x, y, z);
                                                         }));
}

void JNICALL nativeSetBlockEntityData(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    if (world != nullptr) {
        static_cast<void>(jni::onServer<bool>(false, [&] { return world->setBlockEntityPluginData(x, y, z, data); }));
    }
}

jbyteArray JNICALL nativeEntityData(JNIEnv* env, jclass, jstring name, jint id) {
    const auto world = worldOf(env, name);
    return world == nullptr
               ? nullptr
               : jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] { return world->entityPluginData(id); }));
}

void JNICALL nativeSetEntityData(JNIEnv* env, jclass, jstring name, jint id, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    if (world != nullptr) {
        static_cast<void>(jni::onServer<bool>(false, [&] { return world->setEntityPluginData(id, data); }));
    }
}

jbyteArray JNICALL nativePlayerData(JNIEnv* env, jclass, jstring uuid) {
    const std::string id = toStd(env, uuid);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] { return storage().holderData("player", id); }));
}

void JNICALL nativeSetPlayerData(JNIEnv* env, jclass, jstring uuid, jbyteArray nbt) {
    const std::string id = toStd(env, uuid);
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    static_cast<void>(jni::onServer<bool>(false, [&] {
        storage().setHolderData("player", id, data);
        return true;
    }));
}

void JNICALL nativePlayerPermissions(JNIEnv* env, jclass, jstring uuid, jobjectArray nodes, jbooleanArray values) {
    const std::vector<std::string> names = jni::stringsOf(env, nodes);
    std::vector<std::pair<std::string, bool>> grants;
    const jsize count = values == nullptr ? 0 : env->GetArrayLength(values);
    std::vector<jboolean> flags(static_cast<std::size_t>(count));
    if (count > 0) {
        env->GetBooleanArrayRegion(values, 0, count, flags.data());
    }
    for (std::size_t index = 0; index < names.size() && index < flags.size(); ++index) {
        grants.emplace_back(names[index], flags[index] == JNI_TRUE);
    }
    const auto player = BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
    if (player != nullptr) {
        static_cast<void>(jni::onServer<bool>(false, [&] {
            player->setPermissions("bukkit", grants);
            return true;
        }));
    }
}

jobjectArray JNICALL nativeKnownPlayer(JNIEnv* env, jclass, jstring key, jboolean byName) {
    const std::string text = toStd(env, key);
    const auto found = accessCall<std::optional<plugins::PluginKnownPlayer>>(
        std::nullopt, [&] { return byName == JNI_TRUE ? access().playerByName(text) : access().playerByUuid(text); });
    return found.has_value() ? jni::stringArrayOf(env, {found->uuid, found->name, found->world}) : nullptr;
}

jdoubleArray JNICALL nativeKnownPlayerNumbers(JNIEnv* env, jclass, jstring uuid) {
    const std::string text = toStd(env, uuid);
    const auto found = accessCall<std::optional<plugins::PluginKnownPlayer>>(
        std::nullopt, [&] { return access().playerByUuid(text); });
    if (!found.has_value()) {
        return nullptr;
    }
    const std::array<double, 5> values{static_cast<double>(found->firstSeenMillis),
                                       static_cast<double>(found->lastSeenMillis), found->x, found->y, found->z};
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(values.size()));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

[[nodiscard]] jobjectArray pairs(JNIEnv* env, const std::vector<plugins::PluginKnownPlayer>& players) {
    std::vector<std::string> flat;
    for (const plugins::PluginKnownPlayer& player : players) {
        flat.push_back(player.uuid);
        flat.push_back(player.name);
    }
    return jni::stringArrayOf(env, flat);
}

jobjectArray JNICALL nativeKnownPlayers(JNIEnv* env, jclass) {
    return pairs(env, accessCall<std::vector<plugins::PluginKnownPlayer>>({}, [] { return access().knownPlayers(); }));
}

jboolean JNICALL nativeIsOperator(JNIEnv* env, jclass, jstring uuid) {
    const std::string text = toStd(env, uuid);
    return accessCall<bool>(false, [&] { return access().isOperator(text); }) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeSetOperator(JNIEnv* env, jclass, jstring uuid, jboolean value) {
    const std::string text = toStd(env, uuid);
    return accessCall<bool>(false, [&] { return access().setOperator(text, value == JNI_TRUE); }) ? JNI_TRUE
                                                                                                  : JNI_FALSE;
}

jobjectArray JNICALL nativeOperators(JNIEnv* env, jclass) {
    return pairs(env, accessCall<std::vector<plugins::PluginKnownPlayer>>({}, [] { return access().operators(); }));
}

jboolean JNICALL nativeIsWhitelisted(JNIEnv* env, jclass, jstring uuid) {
    const std::string text = toStd(env, uuid);
    return accessCall<bool>(false, [&] { return access().isWhitelisted(text); }) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeSetWhitelisted(JNIEnv* env, jclass, jstring uuid, jstring name, jboolean value) {
    const std::string id = toStd(env, uuid);
    const std::string who = toStd(env, name);
    static_cast<void>(accessCall<bool>(false, [&] {
        access().setWhitelisted(id, who, value == JNI_TRUE);
        return true;
    }));
}

jobjectArray JNICALL nativeWhitelisted(JNIEnv* env, jclass) {
    return pairs(env, accessCall<std::vector<plugins::PluginKnownPlayer>>({}, [] { return access().whitelisted(); }));
}

jboolean JNICALL nativeWhitelistEnforced(JNIEnv*, jclass) {
    return accessCall<bool>(false, [] { return access().whitelistEnforced(); }) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeSetWhitelistEnforced(JNIEnv*, jclass, jboolean value) {
    static_cast<void>(accessCall<bool>(false, [&] {
        access().setWhitelistEnforced(value == JNI_TRUE);
        return true;
    }));
}

jobjectArray JNICALL nativeBanOf(JNIEnv* env, jclass, jstring subject, jboolean ip) {
    const std::string text = toStd(env, subject);
    const auto found = accessCall<std::optional<plugins::PluginBan>>(
        std::nullopt, [&] { return access().banOf(text, ip == JNI_TRUE); });
    return found.has_value() ? jni::stringArrayOf(env, {found->subject, found->name, found->reason, found->source})
                             : nullptr;
}

jlongArray JNICALL nativeBanTimes(JNIEnv* env, jclass, jstring subject, jboolean ip) {
    const std::string text = toStd(env, subject);
    const auto found = accessCall<std::optional<plugins::PluginBan>>(
        std::nullopt, [&] { return access().banOf(text, ip == JNI_TRUE); });
    if (!found.has_value()) {
        return nullptr;
    }
    const std::array<jlong, 2> values{found->createdMillis, found->expiresMillis.value_or(-1)};
    jlongArray array = env->NewLongArray(static_cast<jsize>(values.size()));
    env->SetLongArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

jobjectArray JNICALL nativeBans(JNIEnv* env, jclass, jboolean ip) {
    std::vector<std::string> subjects;
    for (const plugins::PluginBan& record :
         accessCall<std::vector<plugins::PluginBan>>({}, [&] { return access().bans(ip == JNI_TRUE); })) {
        subjects.push_back(record.subject);
    }
    return jni::stringArrayOf(env, subjects);
}

void JNICALL nativeBan(JNIEnv* env, jclass, jstring subject, jstring name, jstring reason, jstring source,
                       jlong created, jlong expires, jboolean ip) {
    plugins::PluginBan record{.subject = toStd(env, subject),
                              .name = toStd(env, name),
                              .reason = toStd(env, reason),
                              .source = toStd(env, source),
                              .createdMillis = created,
                              .expiresMillis = expires < 0 ? std::nullopt : std::optional<std::int64_t>{expires}};
    static_cast<void>(accessCall<bool>(false, [&] {
        access().ban(record, ip == JNI_TRUE);
        return true;
    }));
}

jboolean JNICALL nativePardon(JNIEnv* env, jclass, jstring subject, jboolean ip) {
    const std::string text = toStd(env, subject);
    return accessCall<bool>(false, [&] { return access().pardon(text, ip == JNI_TRUE); }) ? JNI_TRUE : JNI_FALSE;
}
} // namespace

void registerMiscNativesData(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("worldData", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeWorldData)),
        nativeMethod("setWorldData", "(Ljava/lang/String;[B)V", reinterpret_cast<void*>(&nativeSetWorldData)),
        nativeMethod("chunkData", "(Ljava/lang/String;II)[B", reinterpret_cast<void*>(&nativeChunkData)),
        nativeMethod("setChunkData", "(Ljava/lang/String;II[B)V", reinterpret_cast<void*>(&nativeSetChunkData)),
        nativeMethod("blockEntityData", "(Ljava/lang/String;III)[B", reinterpret_cast<void*>(&nativeBlockEntityData)),
        nativeMethod("setBlockEntityData", "(Ljava/lang/String;III[B)V",
                     reinterpret_cast<void*>(&nativeSetBlockEntityData)),
        nativeMethod("entityData", "(Ljava/lang/String;I)[B", reinterpret_cast<void*>(&nativeEntityData)),
        nativeMethod("setEntityData", "(Ljava/lang/String;I[B)V", reinterpret_cast<void*>(&nativeSetEntityData)),
        nativeMethod("playerData", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativePlayerData)),
        nativeMethod("setPlayerData", "(Ljava/lang/String;[B)V", reinterpret_cast<void*>(&nativeSetPlayerData)),
        nativeMethod("knownPlayer", "(Ljava/lang/String;Z)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeKnownPlayer)),
        nativeMethod("knownPlayerNumbers", "(Ljava/lang/String;)[D",
                     reinterpret_cast<void*>(&nativeKnownPlayerNumbers)),
        nativeMethod("knownPlayers", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeKnownPlayers)),
        nativeMethod("isOperator", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeIsOperator)),
        nativeMethod("setOperator", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeSetOperator)),
        nativeMethod("operators", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeOperators)),
        nativeMethod("isWhitelisted", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeIsWhitelisted)),
        nativeMethod("setWhitelisted", "(Ljava/lang/String;Ljava/lang/String;Z)V",
                     reinterpret_cast<void*>(&nativeSetWhitelisted)),
        nativeMethod("whitelisted", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeWhitelisted)),
        nativeMethod("whitelistEnforced", "()Z", reinterpret_cast<void*>(&nativeWhitelistEnforced)),
        nativeMethod("setWhitelistEnforced", "(Z)V", reinterpret_cast<void*>(&nativeSetWhitelistEnforced)),
        nativeMethod("banOf", "(Ljava/lang/String;Z)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBanOf)),
        nativeMethod("banTimes", "(Ljava/lang/String;Z)[J", reinterpret_cast<void*>(&nativeBanTimes)),
        nativeMethod("bans", "(Z)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBans)),
        nativeMethod("ban", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JJZ)V",
                     reinterpret_cast<void*>(&nativeBan)),
        nativeMethod("pardon", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativePardon)),
        nativeMethod("playerPermissions", "(Ljava/lang/String;[Ljava/lang/String;[Z)V",
                     reinterpret_cast<void*>(&nativePlayerPermissions)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
