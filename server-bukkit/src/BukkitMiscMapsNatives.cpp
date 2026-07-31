#include "BukkitMiscNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

[[nodiscard]] plugins::PluginMaps& maps() { return BukkitBridge::current().services().maps(); }

jobjectArray JNICALL nativeMapInfo(JNIEnv* env, jclass, jint id) {
    const auto found = jni::onServer<std::optional<plugins::PluginMap>>(std::nullopt, [&] { return maps().map(id); });
    if (!found.has_value()) {
        return nullptr;
    }
    return jni::stringArrayOf(env, {found->dimension, std::to_string(found->scale), std::to_string(found->centerX),
                                    std::to_string(found->centerZ), found->tracking ? "1" : "0",
                                    found->unlimited ? "1" : "0", found->locked ? "1" : "0"});
}

jint JNICALL nativeMapCreate(JNIEnv* env, jclass, jstring world, jint centerX, jint centerZ, jint scale) {
    const std::string name = toStd(env, world);
    return jni::onServer<jint>(-1, [&] { return maps().create(name, centerX, centerZ, scale); });
}

jboolean JNICALL nativeMapUpdate(JNIEnv* env, jclass, jint id, jobjectArray info) {
    const std::vector<std::string> fields = jni::stringsOf(env, info);
    if (fields.size() < 7) {
        return JNI_FALSE;
    }
    const plugins::PluginMap map{.id = id,
                                 .dimension = fields[0],
                                 .scale = std::atoi(fields[1].c_str()),
                                 .centerX = std::atoi(fields[2].c_str()),
                                 .centerZ = std::atoi(fields[3].c_str()),
                                 .tracking = fields[4] == "1",
                                 .unlimited = fields[5] == "1",
                                 .locked = fields[6] == "1"};
    return flag(jni::onServer<bool>(false, [&] { return maps().update(map); }));
}

jbyteArray JNICALL nativeMapColors(JNIEnv* env, jclass, jint id) {
    return jni::byteArrayOf(env, jni::onServer<std::optional<std::vector<std::uint8_t>>>(
                                     std::nullopt, [&] { return std::optional{maps().colors(id)}; }));
}

jobjectArray JNICALL nativeMapCarriers(JNIEnv* env, jclass, jint id) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [&] { return maps().carriers(id); }));
}

void JNICALL nativeMapShow(JNIEnv* env, jclass, jstring uuid, jint id, jbyteArray colors, jintArray cursors,
                           jobjectArray captions) {
    const std::string who = toStd(env, uuid);
    const std::vector<std::uint8_t> picture = jni::bytesOf(env, colors);
    std::vector<std::int32_t> flat(cursors == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(cursors)));
    if (!flat.empty()) {
        env->GetIntArrayRegion(cursors, 0, static_cast<jsize>(flat.size()), flat.data());
    }
    std::vector<plugins::PluginMapCursor> shown;
    const jsize captionCount = captions == nullptr ? 0 : env->GetArrayLength(captions);
    for (std::size_t index = 0; index + 3 < flat.size(); index += 4) {
        plugins::PluginMapCursor cursor{.x = static_cast<std::int8_t>(flat[index]),
                                        .y = static_cast<std::int8_t>(flat[index + 1]),
                                        .direction = static_cast<std::int8_t>(flat[index + 2]),
                                        .type = flat[index + 3]};
        const auto at = static_cast<jsize>(index / 4);
        if (at < captionCount) {
            auto caption = static_cast<jstring>(env->GetObjectArrayElement(captions, at));
            if (caption != nullptr) {
                cursor.caption = toStd(env, caption);
                env->DeleteLocalRef(caption);
            }
        }
        shown.push_back(std::move(cursor));
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        maps().show(who, id, picture, shown);
        return true;
    }));
}

jdoubleArray JNICALL nativeTmPace(JNIEnv* env, jclass) {
    const std::array<double, 4> pace =
        jni::onServer<std::array<double, 4>>({20.0, 20.0, 20.0, 0.0}, [] { return control().pace(); });
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(pace.size()));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(pace.size()), pace.data());
    return array;
}

jobjectArray JNICALL nativeResourcePack(JNIEnv* env, jclass) {
    const auto pack = jni::onServer<std::optional<plugins::PluginResourcePack>>(
        std::nullopt, [] { return control().resourcePack(); });
    if (!pack.has_value()) {
        return nullptr;
    }
    return jni::stringArrayOf(env, {pack->id, pack->url, pack->hash, pack->required ? "1" : "0", pack->prompt});
}

[[nodiscard]] plugins::PluginLoot& loot() { return BukkitBridge::current().services().loot(); }

jboolean JNICALL nativeLootHas(JNIEnv* env, jclass, jstring table) {
    const std::string text = toStd(env, table);
    return flag(jni::onServer<bool>(false, [&] { return loot().has(text); }));
}

jobjectArray JNICALL nativeLootTables(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] { return loot().tables(); }));
}

jobjectArray JNICALL nativeLootRoll(JNIEnv* env, jclass, jstring table, jstring world, jdouble x, jdouble y, jdouble z,
                                    jfloat luck, jlong seed, jstring looter) {
    const std::string id = toStd(env, table);
    const plugins::PluginLootRoll rolled{.world = toStd(env, world),
                                         .x = x,
                                         .y = y,
                                         .z = z,
                                         .luck = luck,
                                         .seed = seed,
                                         .looter = looter == nullptr ? std::string{} : toStd(env, looter)};
    const auto items = jni::onServer<std::optional<std::vector<plugins::PluginLootItem>>>(
        std::nullopt, [&] { return loot().roll(id, rolled); });
    if (!items.has_value()) {
        return nullptr;
    }
    std::vector<std::string> flat;
    for (const plugins::PluginLootItem& item : *items) {
        flat.push_back(item.item);
        flat.push_back(std::to_string(item.count));
        flat.push_back(item.components);
    }
    return jni::stringArrayOf(env, flat);
}

jintArray JNICALL nativeLocateStructure(JNIEnv* env, jclass, jstring world, jstring structure, jint x, jint z,
                                        jint cells) {
    const auto found = worldOf(env, world);
    const std::string kind = toStd(env, structure);
    if (found == nullptr) {
        return nullptr;
    }
    const auto at = jni::onServer<std::optional<std::array<std::int32_t, 2>>>(
        std::nullopt, [&] { return found->locateStructure(kind, x, z, cells, false); });
    if (!at.has_value()) {
        return nullptr;
    }
    jintArray array = env->NewIntArray(2);
    env->SetIntArrayRegion(array, 0, 2, at->data());
    return array;
}

jintArray JNICALL nativeLocateBiome(JNIEnv* env, jclass, jstring world, jobjectArray biomes, jint x, jint y, jint z,
                                    jint radius, jint step) {
    const auto found = worldOf(env, world);
    const std::vector<std::string> wanted = jni::stringsOf(env, biomes);
    if (found == nullptr) {
        return nullptr;
    }
    const auto at = jni::onServer<std::optional<std::array<std::int32_t, 3>>>(
        std::nullopt, [&] { return found->locateBiome(wanted, x, y, z, radius, step); });
    if (!at.has_value()) {
        return nullptr;
    }
    jintArray array = env->NewIntArray(3);
    env->SetIntArrayRegion(array, 0, 3, at->data());
    return array;
}

jintArray JNICALL nativeShapeBoxes(JNIEnv* env, jclass, jint state) {
    const std::vector<std::int32_t> boxes = BukkitBridge::current().services().worlds().collisionBoxes(state);
    jintArray array = env->NewIntArray(static_cast<jsize>(boxes.size()));
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(boxes.size()), boxes.data());
    return array;
}

jobjectArray JNICALL nativeDragonFight(JNIEnv* env, jclass, jstring world) {
    const auto found = worldOf(env, world);
    if (found == nullptr) {
        return nullptr;
    }
    const auto facts = jni::onServer<std::optional<plugins::PluginWorldRef::DragonFightFacts>>(
        std::nullopt, [&] { return found->dragonFight(); });
    if (!facts.has_value()) {
        return nullptr;
    }
    const std::string portal = facts->portal.has_value()
                                   ? std::to_string((*facts->portal)[0]) + "," + std::to_string((*facts->portal)[1]) +
                                         "," + std::to_string((*facts->portal)[2])
                                   : std::string{};
    const auto joined = [](const auto& values, auto toText) {
        std::string out;
        for (const auto& value : values) {
            out += (out.empty() ? "" : ",") + toText(value);
        }
        return out;
    };
    return jni::stringArrayOf(
        env, {facts->previouslyKilled ? "1" : "0", portal, facts->respawnStage, std::to_string(facts->gatewaysLeft),
              facts->dragonUuid, std::to_string(facts->barProgress), facts->barVisible ? "1" : "0",
              joined(facts->barPlayers, [](std::int32_t id) { return std::to_string(id); }),
              joined(facts->respawnCrystals, [](const std::string& id) { return id; }),
              joined(facts->healingCrystals, [](const std::string& id) { return id; }), facts->barTitle,
              std::to_string(facts->barColor), std::to_string(facts->barStyle), std::to_string(facts->barFlags)});
}

void JNICALL nativeSetDragonPreviouslyKilled(JNIEnv* env, jclass, jstring world, jboolean value) {
    const auto found = worldOf(env, world);
    if (found != nullptr) {
        static_cast<void>(
            jni::onServer<bool>(false, [&] { return found->setDragonPreviouslyKilled(value == JNI_TRUE); }));
    }
}

jobjectArray JNICALL nativePlayerProfile(JNIEnv* env, jclass, jstring uuid) {
    const auto player = BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
    if (player == nullptr) {
        return nullptr;
    }
    std::vector<std::string> flat;
    for (const std::array<std::string, 3>& property :
         jni::onServer<std::vector<std::array<std::string, 3>>>({}, [&] { return player->profileProperties(); })) {
        flat.insert(flat.end(), property.begin(), property.end());
    }
    return jni::stringArrayOf(env, flat);
}

jobjectArray JNICALL nativeSelect(JNIEnv* env, jclass, jstring selector, jstring asUuid) {
    const std::string text = toStd(env, selector);
    const std::string who = asUuid == nullptr ? std::string{} : toStd(env, asUuid);
    std::vector<plugins::PluginSelected> chosen;
    std::string error;
    const bool ok = jni::onServer<bool>(
        false, [&] { return BukkitBridge::current().services().selectors().select(text, who, chosen, error); });
    std::vector<std::string> flat;
    if (!ok) {
        flat.push_back("error");
        flat.push_back(error);
        return jni::stringArrayOf(env, flat);
    }
    flat.push_back("ok");
    for (const plugins::PluginSelected& one : chosen) {
        flat.push_back(one.uuid);
        flat.push_back(one.world);
        flat.push_back(one.player ? "1" : "0");
    }
    return jni::stringArrayOf(env, flat);
}

jobjectArray JNICALL nativeSpawnPoint(JNIEnv* env, jclass, jstring uuid) {
    const std::string text = toStd(env, uuid);
    const auto point =
        accessCall<std::optional<plugins::PluginKnownPlayer>>(std::nullopt, [&] { return access().spawnPoint(text); });
    if (!point.has_value()) {
        return nullptr;
    }
    return jni::stringArrayOf(
        env, {point->world, std::to_string(point->x), std::to_string(point->y), std::to_string(point->z)});
}
} // namespace

void registerMiscNativesMaps(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("spawnPoint", "(Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeSpawnPoint)),
        nativeMethod("mapInfo", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeMapInfo)),
        nativeMethod("mapCreate", "(Ljava/lang/String;III)I", reinterpret_cast<void*>(&nativeMapCreate)),
        nativeMethod("mapUpdate", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeMapUpdate)),
        nativeMethod("mapColors", "(I)[B", reinterpret_cast<void*>(&nativeMapColors)),
        nativeMethod("mapCarriers", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeMapCarriers)),
        nativeMethod("mapShow", "(Ljava/lang/String;I[B[I[Ljava/lang/String;)V",
                     reinterpret_cast<void*>(&nativeMapShow)),
        nativeMethod("tmPace", "()[D", reinterpret_cast<void*>(&nativeTmPace)),
        nativeMethod("resourcePack", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeResourcePack)),
        nativeMethod("lootHas", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeLootHas)),
        nativeMethod("lootTables", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeLootTables)),
        nativeMethod("lootRoll", "(Ljava/lang/String;Ljava/lang/String;DDDFJLjava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeLootRoll)),
        nativeMethod("locateStructure", "(Ljava/lang/String;Ljava/lang/String;III)[I",
                     reinterpret_cast<void*>(&nativeLocateStructure)),
        nativeMethod("locateBiome", "(Ljava/lang/String;[Ljava/lang/String;IIIII)[I",
                     reinterpret_cast<void*>(&nativeLocateBiome)),
        nativeMethod("shapeBoxes", "(I)[I", reinterpret_cast<void*>(&nativeShapeBoxes)),
        nativeMethod("dragonFight", "(Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeDragonFight)),
        nativeMethod("setDragonPreviouslyKilled", "(Ljava/lang/String;Z)V",
                     reinterpret_cast<void*>(&nativeSetDragonPreviouslyKilled)),
        nativeMethod("playerProfile", "(Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativePlayerProfile)),
        nativeMethod("select", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeSelect)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
