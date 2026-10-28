#include "BridgeInternal.hpp"

#include <array>

namespace basalt::server::bukkit {
namespace {
using plugins::PluginPlayerRef;

[[nodiscard]] std::shared_ptr<PluginPlayerRef> playerOf(JNIEnv* env, jstring uuid) {
    return BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
}

[[nodiscard]] jobjectArray stringArray(JNIEnv* env, const std::vector<std::string>& values) {
    jobjectArray array =
        env->NewObjectArray(static_cast<jsize>(values.size()), env->FindClass("java/lang/String"), nullptr);
    for (std::size_t index = 0; index < values.size(); ++index) {
        jstring text = toJava(env, values[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

void logLine(const wire::common::Logger& logger, jint level, const std::string& text) {
    switch (level) {
    case 0:
        logger.debug([&text] { return text; });
        break;
    case 1:
        logger.info([&text] { return text; });
        break;
    case 2:
        logger.warn([&text] { return text; });
        break;
    default:
        logger.error([&text] { return text; });
        break;
    }
}

void JNICALL nativeLog(JNIEnv* env, jclass, jint level, jstring name, jstring message) {
    const wire::common::Logger logger{toStd(env, name)};
    const std::string text = toStd(env, message);
    std::string_view rest = text;
    while (!rest.empty() && (rest.back() == '\n' || rest.back() == '\r')) {
        rest.remove_suffix(1);
    }
    while (true) {
        const std::size_t cut = rest.find('\n');
        std::string_view line = rest.substr(0, cut);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        logLine(logger, level, std::string{line});
        if (cut == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(cut + 1);
    }
}

jlong JNICALL nativeTick(JNIEnv*, jclass) { return BukkitBridge::current().services().tasks().tick(); }

void JNICALL nativeAfter(JNIEnv*, jclass, jlong id, jint delay) { BukkitBridge::current().scheduleAfter(id, delay); }

void JNICALL nativeEvery(JNIEnv*, jclass, jlong id, jint delay, jint period) {
    BukkitBridge::current().scheduleEvery(id, delay, period);
}

void JNICALL nativeCancel(JNIEnv*, jclass, jlong id) { BukkitBridge::current().cancelTask(id); }

jboolean JNICALL nativeSubscribe(JNIEnv* env, jclass, jstring event, jint priority) {
    return BukkitBridge::current().subscribe(toStd(env, event), priority) ? JNI_TRUE : JNI_FALSE;
}

jobjectArray JNICALL nativeEventFields(JNIEnv* env, jclass, jstring event) {
    const plugins::PluginEventType* type = BukkitBridge::current().typeNamed(toStd(env, event));
    return stringArray(env, type == nullptr ? std::vector<std::string>{} : type->fields());
}

void JNICALL nativeRegisterCommand(JNIEnv* env, jclass, jstring plugin, jstring name, jstring permission) {
    BukkitBridge::current().registerCommand(toStd(env, plugin), toStd(env, name), toStd(env, permission));
}

void JNICALL nativeUnregisterCommands(JNIEnv* env, jclass, jstring plugin) {
    BukkitBridge::current().unregisterCommands(toStd(env, plugin));
}

void JNICALL nativeListenChannel(JNIEnv* env, jclass, jstring channel) {
    BukkitBridge::current().listenChannel(toStd(env, channel));
}

jboolean JNICALL nativeChannelSend(JNIEnv* env, jclass, jstring uuid, jstring channel, jbyteArray payload) {
    const auto player = playerOf(env, uuid);
    if (player == nullptr || payload == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(env->GetArrayLength(payload)));
    env->GetByteArrayRegion(payload, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<jbyte*>(bytes.data()));
    return BukkitBridge::current().services().channels().send(*player, toStd(env, channel), bytes) ? JNI_TRUE
                                                                                                   : JNI_FALSE;
}

jboolean JNICALL nativeConsole(JNIEnv* env, jclass, jstring line) {
    return BukkitBridge::current().services().commands().runAsConsole(toStd(env, line)) ? JNI_TRUE : JNI_FALSE;
}

jstring JNICALL nativePluginFolder(JNIEnv* env, jclass, jstring id) {
    return toJava(env, BukkitBridge::current().services().paths().folder(toStd(env, id)));
}

jobjectArray JNICALL nativeOnline(JNIEnv* env, jclass) {
    std::vector<std::string> flat;
    for (const std::shared_ptr<PluginPlayerRef>& player : BukkitBridge::current().services().players().online()) {
        flat.push_back(player->uuid());
        flat.push_back(player->name());
    }
    return stringArray(env, flat);
}

jobjectArray JNICALL nativePlayerStrings(JNIEnv* env, jclass, jstring uuid) {
    const std::shared_ptr<PluginPlayerRef> player = playerOf(env, uuid);
    if (player == nullptr) {
        return nullptr;
    }
    return stringArray(env, {player->name(), player->world(), player->gamemode(), player->online() ? "1" : "0"});
}

jdoubleArray JNICALL nativePlayerNumbers(JNIEnv* env, jclass, jstring uuid) {
    const std::shared_ptr<PluginPlayerRef> player = playerOf(env, uuid);
    if (player == nullptr) {
        return nullptr;
    }
    const std::array<double, 9> values{player->health(),
                                       static_cast<double>(player->food()),
                                       static_cast<double>(player->level()),
                                       player->x(),
                                       player->y(),
                                       player->z(),
                                       static_cast<double>(player->ping()),
                                       player->sneaking() ? 1.0 : 0.0,
                                       0.0};
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(values.size()));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

void JNICALL nativePlayerSend(JNIEnv* env, jclass, jstring uuid, jstring message) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->send(toStd(env, message));
    }
}

void JNICALL nativePlayerActionBar(JNIEnv* env, jclass, jstring uuid, jstring message) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->actionBar(toStd(env, message));
    }
}

void JNICALL nativePlayerKick(JNIEnv* env, jclass, jstring uuid, jstring reason) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->kick(toStd(env, reason));
    }
}

jboolean JNICALL nativePlayerTeleport(JNIEnv* env, jclass, jstring uuid, jstring world, jdouble x, jdouble y,
                                      jdouble z) {
    const auto player = playerOf(env, uuid);
    return player != nullptr && player->teleport(toStd(env, world), x, y, z) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativePlayerMay(JNIEnv* env, jclass, jstring uuid, jstring permission) {
    const auto player = playerOf(env, uuid);
    return player != nullptr && player->may(toStd(env, permission)) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativePlayerSetHealth(JNIEnv* env, jclass, jstring uuid, jdouble value) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->setHealth(value);
    }
}

void JNICALL nativePlayerSetFood(JNIEnv* env, jclass, jstring uuid, jint value) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->setFood(value);
    }
}

void JNICALL nativePlayerSetGamemode(JNIEnv* env, jclass, jstring uuid, jstring mode) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->setGamemode(toStd(env, mode));
    }
}

jboolean JNICALL nativePlayerGive(JNIEnv* env, jclass, jstring uuid, jstring item, jint count) {
    const auto player = playerOf(env, uuid);
    return player != nullptr && player->give(toStd(env, item), count) ? JNI_TRUE : JNI_FALSE;
}

jobjectArray JNICALL nativeWorlds(JNIEnv* env, jclass) {
    std::vector<std::string> names;
    for (const auto& world : BukkitBridge::current().services().worlds().all()) {
        names.push_back(world->name());
    }
    return stringArray(env, names);
}

jstring JNICALL nativeBlock(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    if (found == nullptr) {
        return nullptr;
    }
    const std::optional<std::string> block = found->block(x, y, z);
    return block.has_value() ? toJava(env, *block) : nullptr;
}

jboolean JNICALL nativeSetBlock(JNIEnv* env, jclass, jstring world, jint x, jint y, jint z, jstring block) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    return found != nullptr && found->setBlock(x, y, z, toStd(env, block)) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeOnPluginThread(JNIEnv*, jclass) {
    return BukkitBridge::current().plugins().onPluginThread() ? JNI_TRUE : JNI_FALSE;
}

jlong JNICALL nativeWorldTime(JNIEnv* env, jclass, jstring world) {
    const auto found = BukkitBridge::current().services().worlds().byName(toStd(env, world));
    return found == nullptr ? 0 : found->time();
}
} // namespace

void registerNatives(JNIEnv* env, jclass natives) {
    const std::array methods{
        JNINativeMethod{const_cast<char*>("log"), const_cast<char*>("(ILjava/lang/String;Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativeLog)},
        JNINativeMethod{const_cast<char*>("currentTick"), const_cast<char*>("()J"),
                        reinterpret_cast<void*>(&nativeTick)},
        JNINativeMethod{const_cast<char*>("onPluginThread"), const_cast<char*>("()Z"),
                        reinterpret_cast<void*>(&nativeOnPluginThread)},
        JNINativeMethod{const_cast<char*>("scheduleAfter"), const_cast<char*>("(JI)V"),
                        reinterpret_cast<void*>(&nativeAfter)},
        JNINativeMethod{const_cast<char*>("scheduleEvery"), const_cast<char*>("(JII)V"),
                        reinterpret_cast<void*>(&nativeEvery)},
        JNINativeMethod{const_cast<char*>("cancelTask"), const_cast<char*>("(J)V"),
                        reinterpret_cast<void*>(&nativeCancel)},
        JNINativeMethod{const_cast<char*>("subscribe"), const_cast<char*>("(Ljava/lang/String;I)Z"),
                        reinterpret_cast<void*>(&nativeSubscribe)},
        JNINativeMethod{const_cast<char*>("eventFields"), const_cast<char*>("(Ljava/lang/String;)[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeEventFields)},
        JNINativeMethod{const_cast<char*>("registerCommand"),
                        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativeRegisterCommand)},
        JNINativeMethod{const_cast<char*>("unregisterCommands"), const_cast<char*>("(Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativeUnregisterCommands)},
        JNINativeMethod{const_cast<char*>("listenChannel"), const_cast<char*>("(Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativeListenChannel)},
        JNINativeMethod{const_cast<char*>("playerChannelSend"),
                        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;[B)Z"),
                        reinterpret_cast<void*>(&nativeChannelSend)},
        JNINativeMethod{const_cast<char*>("runConsoleCommand"), const_cast<char*>("(Ljava/lang/String;)Z"),
                        reinterpret_cast<void*>(&nativeConsole)},
        JNINativeMethod{const_cast<char*>("pluginFolder"), const_cast<char*>("(Ljava/lang/String;)Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativePluginFolder)},
        JNINativeMethod{const_cast<char*>("onlinePlayers"), const_cast<char*>("()[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeOnline)},
        JNINativeMethod{const_cast<char*>("playerStrings"),
                        const_cast<char*>("(Ljava/lang/String;)[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativePlayerStrings)},
        JNINativeMethod{const_cast<char*>("playerNumbers"), const_cast<char*>("(Ljava/lang/String;)[D"),
                        reinterpret_cast<void*>(&nativePlayerNumbers)},
        JNINativeMethod{const_cast<char*>("playerSend"), const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativePlayerSend)},
        JNINativeMethod{const_cast<char*>("playerActionBar"),
                        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativePlayerActionBar)},
        JNINativeMethod{const_cast<char*>("playerKick"), const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativePlayerKick)},
        JNINativeMethod{const_cast<char*>("playerTeleport"),
                        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;DDD)Z"),
                        reinterpret_cast<void*>(&nativePlayerTeleport)},
        JNINativeMethod{const_cast<char*>("playerMay"), const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)Z"),
                        reinterpret_cast<void*>(&nativePlayerMay)},
        JNINativeMethod{const_cast<char*>("playerSetHealth"), const_cast<char*>("(Ljava/lang/String;D)V"),
                        reinterpret_cast<void*>(&nativePlayerSetHealth)},
        JNINativeMethod{const_cast<char*>("playerSetFood"), const_cast<char*>("(Ljava/lang/String;I)V"),
                        reinterpret_cast<void*>(&nativePlayerSetFood)},
        JNINativeMethod{const_cast<char*>("playerSetGamemode"),
                        const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
                        reinterpret_cast<void*>(&nativePlayerSetGamemode)},
        JNINativeMethod{const_cast<char*>("playerGive"), const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;I)Z"),
                        reinterpret_cast<void*>(&nativePlayerGive)},
        JNINativeMethod{const_cast<char*>("worlds"), const_cast<char*>("()[Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeWorlds)},
        JNINativeMethod{const_cast<char*>("blockAt"), const_cast<char*>("(Ljava/lang/String;III)Ljava/lang/String;"),
                        reinterpret_cast<void*>(&nativeBlock)},
        JNINativeMethod{const_cast<char*>("setBlockAt"),
                        const_cast<char*>("(Ljava/lang/String;IIILjava/lang/String;)Z"),
                        reinterpret_cast<void*>(&nativeSetBlock)},
        JNINativeMethod{const_cast<char*>("worldTime"), const_cast<char*>("(Ljava/lang/String;)J"),
                        reinterpret_cast<void*>(&nativeWorldTime)},
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
    registerPacketNatives(env, natives);
}
} // namespace basalt::server::bukkit
