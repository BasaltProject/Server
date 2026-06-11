#include "BridgeInternal.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <mutex>

namespace basalt::server::bukkit {
namespace {
using plugins::PluginEntityRef;
using plugins::PluginItem;
using plugins::PluginPlayerRef;
using plugins::PluginWorldRef;

[[nodiscard]] plugins::PluginWorlds& worlds() { return BukkitBridge::current().services().worlds(); }

[[nodiscard]] std::shared_ptr<PluginWorldRef> worldOf(JNIEnv* env, jstring name) {
    return worlds().byName(toStd(env, name));
}

[[nodiscard]] std::shared_ptr<PluginPlayerRef> playerOf(JNIEnv* env, jstring uuid) {
    return BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
}

[[nodiscard]] jstring javaOrNull(JNIEnv* env, const std::optional<std::string>& text) {
    return text.has_value() ? toJava(env, *text) : nullptr;
}

[[nodiscard]] jobjectArray stringArray(JNIEnv* env, const std::vector<std::string>& values) {
    jclass string = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), string, nullptr);
    for (std::size_t index = 0; index < values.size(); ++index) {
        jstring text = toJava(env, values[index]);
        env->SetObjectArrayElement(array, static_cast<jsize>(index), text);
        env->DeleteLocalRef(text);
    }
    return array;
}

[[nodiscard]] jintArray intArray(JNIEnv* env, const std::vector<std::int32_t>& values) {
    jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

[[nodiscard]] jdoubleArray doubleArray(JNIEnv* env, const double* values, std::size_t count) {
    jdoubleArray array = env->NewDoubleArray(static_cast<jsize>(count));
    env->SetDoubleArrayRegion(array, 0, static_cast<jsize>(count), values);
    return array;
}

// No C++ exception may reach Java; a plugin worker thread has the call run on the tick thread and waits for it.
template <typename Result, typename Body>
[[nodiscard]] Result guarded(Result fallback, Body&& body) {
    const auto run = [&fallback, &body]() -> Result {
        try {
            return body();
        } catch (const std::exception& error) {
            BukkitBridge::current().log().warn([&error] { return std::string{"world call failed: "} + error.what(); });
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

jint JNICALL nativeStateId(JNIEnv* env, jclass, jstring text) { return worlds().stateId(toStd(env, text)); }

jstring JNICALL nativeStateText(JNIEnv* env, jclass, jint id) { return toJava(env, worlds().stateText(id)); }

jobjectArray JNICALL nativeBlockTable(JNIEnv* env, jclass) { return stringArray(env, worlds().blockTable()); }

jobjectArray JNICALL nativeBlockProperties(JNIEnv* env, jclass, jstring block) {
    return stringArray(env, worlds().blockProperties(toStd(env, block)));
}

jint JNICALL nativeStateFlags(JNIEnv*, jclass, jint id) { return worlds().stateFlags(id); }

jdoubleArray JNICALL nativeBlockHardness(JNIEnv* env, jclass, jstring block) {
    const std::vector<double> facts = worlds().blockHardness(toStd(env, block));
    return facts.empty() ? nullptr : doubleArray(env, facts.data(), facts.size());
}

jdoubleArray JNICALL nativeBlockInfo(JNIEnv* env, jclass, jint id) {
    const std::vector<double> facts = worlds().blockInfo(id);
    return facts.empty() ? nullptr : doubleArray(env, facts.data(), facts.size());
}

jobjectArray JNICALL nativeBiomeIds(JNIEnv* env, jclass) { return stringArray(env, worlds().biomeIds()); }

jobjectArray JNICALL nativeItemIds(JNIEnv* env, jclass) { return stringArray(env, worlds().itemIds()); }

jobjectArray JNICALL nativeTags(JNIEnv* env, jclass, jstring registry) {
    return stringArray(env, worlds().tags(toStd(env, registry)));
}

jobjectArray JNICALL nativeRegistryIds(JNIEnv* env, jclass, jstring registry) {
    return stringArray(env, worlds().registryIds(toStd(env, registry)));
}

jobjectArray JNICALL nativeEntityTypes(JNIEnv* env, jclass) { return stringArray(env, worlds().entityTypeIds()); }

jobjectArray JNICALL nativeWorldStrings(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    return stringArray(env, {world->dimension(), world->folder(), world->difficulty()});
}

jlongArray JNICALL nativeWorldNumbers(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    const std::array<std::int32_t, 3> spawn = world->spawn();
    const std::array<jlong, 11> values{world->seed(),
                                       world->minHeight(),
                                       world->height(),
                                       spawn[0],
                                       spawn[1],
                                       spawn[2],
                                       world->dayTime(),
                                       world->raining() ? 1 : 0,
                                       world->thundering() ? 1 : 0,
                                       world->time(),
                                       0};
    jlongArray array = env->NewLongArray(static_cast<jsize>(values.size()));
    env->SetLongArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    return array;
}

void JNICALL nativeSetTime(JNIEnv* env, jclass, jstring name, jlong ticks) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        world->setDayTime(ticks);
    }
}

void JNICALL nativeSetWeather(JNIEnv* env, jclass, jstring name, jboolean raining, jboolean thundering) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        world->setWeather(raining == JNI_TRUE, thundering == JNI_TRUE);
    }
}

void JNICALL nativeSetSpawn(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        world->setSpawn(x, y, z);
    }
}

jboolean JNICALL nativeSetDifficulty(JNIEnv* env, jclass, jstring name, jstring difficulty) {
    const auto world = worldOf(env, name);
    return world != nullptr && world->setDifficulty(toStd(env, difficulty)) ? JNI_TRUE : JNI_FALSE;
}

jstring JNICALL nativeGameRule(JNIEnv* env, jclass, jstring name, jstring rule) {
    const auto world = worldOf(env, name);
    return world == nullptr ? nullptr : javaOrNull(env, world->gameRule(toStd(env, rule)));
}

jboolean JNICALL nativeSetGameRule(JNIEnv* env, jclass, jstring name, jstring rule, jstring value) {
    const auto world = worldOf(env, name);
    return world != nullptr && world->setGameRule(toStd(env, rule), toStd(env, value)) ? JNI_TRUE : JNI_FALSE;
}

jobjectArray JNICALL nativeGameRules(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    return stringArray(env, world == nullptr ? std::vector<std::string>{} : world->gameRules());
}

void JNICALL nativeSaveWorld(JNIEnv* env, jclass, jstring name) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        guarded<int>(0, [&] {
            world->save();
            return 0;
        });
    }
}

jint JNICALL nativeState(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    return guarded<jint>(-1, [&] { return world->stateAt(x, y, z, load == JNI_TRUE); });
}

jboolean JNICALL nativeSetState(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint state,
                                jboolean notify) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    return guarded<jboolean>(
        JNI_FALSE, [&] { return world->setStateAt(x, y, z, state, notify == JNI_TRUE) ? JNI_TRUE : JNI_FALSE; });
}

jint JNICALL nativeReadStates(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray out, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const jsize count = env->GetArrayLength(out);
    std::vector<std::int32_t> at(static_cast<std::size_t>(env->GetArrayLength(positions)));
    env->GetIntArrayRegion(positions, 0, static_cast<jsize>(at.size()), at.data());
    std::vector<std::int32_t> read(static_cast<std::size_t>(count), -1);
    const jint found = guarded<jint>(0, [&] { return world->readStates(at, read, load == JNI_TRUE); });
    env->SetIntArrayRegion(out, 0, count, read.data());
    return found;
}

jint JNICALL nativeWriteStates(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray states,
                               jboolean notify) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    std::vector<std::int32_t> at(static_cast<std::size_t>(env->GetArrayLength(positions)));
    env->GetIntArrayRegion(positions, 0, static_cast<jsize>(at.size()), at.data());
    std::vector<std::int32_t> next(static_cast<std::size_t>(env->GetArrayLength(states)));
    env->GetIntArrayRegion(states, 0, static_cast<jsize>(next.size()), next.data());
    return guarded<jint>(0, [&] { return world->writeStates(at, next, notify == JNI_TRUE); });
}

jboolean JNICALL nativeReadSection(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                   jboolean load, jintArray out) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> read(4096);
    // A loaded section is copied under the world's read lock, so no thread hop is needed.
    if (world->readSection(chunkX, sectionY, chunkZ, false, read)) {
        env->SetIntArrayRegion(out, 0, 4096, read.data());
        return JNI_TRUE;
    }
    const bool ok =
        guarded<bool>(false, [&] { return world->readSection(chunkX, sectionY, chunkZ, load == JNI_TRUE, read); });
    if (ok) {
        env->SetIntArrayRegion(out, 0, 4096, read.data());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeWriteSection(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                    jintArray states, jboolean notify) {
    const auto world = worldOf(env, name);
    if (world == nullptr || env->GetArrayLength(states) < 4096) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> next(4096);
    env->GetIntArrayRegion(states, 0, 4096, next.data());
    return guarded<jboolean>(JNI_FALSE, [&] {
        return world->writeSection(chunkX, sectionY, chunkZ, next, notify == JNI_TRUE) ? JNI_TRUE : JNI_FALSE;
    });
}

jint JNICALL nativeApplySections(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jintArray sections,
                                 jintArray states, jboolean resend, jintArray previous) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    std::vector<std::int32_t> ys(static_cast<std::size_t>(env->GetArrayLength(sections)));
    env->GetIntArrayRegion(sections, 0, static_cast<jsize>(ys.size()), ys.data());
    std::vector<std::int32_t> next(static_cast<std::size_t>(env->GetArrayLength(states)));
    env->GetIntArrayRegion(states, 0, static_cast<jsize>(next.size()), next.data());
    std::vector<std::int32_t> before(previous == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(previous)));
    const jint changed =
        guarded<jint>(-1, [&] { return world->applySections(chunkX, chunkZ, ys, next, resend == JNI_TRUE, before); });
    if (previous != nullptr && changed >= 0) {
        env->SetIntArrayRegion(previous, 0, static_cast<jsize>(before.size()), before.data());
    }
    return changed;
}

void JNICALL nativeResendChunks(JNIEnv* env, jclass, jstring name, jintArray chunks) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return;
    }
    std::vector<std::int32_t> pairs(static_cast<std::size_t>(env->GetArrayLength(chunks)));
    env->GetIntArrayRegion(chunks, 0, static_cast<jsize>(pairs.size()), pairs.data());
    guarded<int>(0, [&] {
        world->resendChunks(pairs);
        return 0;
    });
}

jstring JNICALL nativeBiome(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    return world == nullptr ? nullptr : javaOrNull(env, guarded<std::optional<std::string>>(std::nullopt, [&] {
                                                       return world->biomeAt(x, y, z);
                                                   }));
}

jboolean JNICALL nativeSetBiome(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jstring biome) {
    const auto world = worldOf(env, name);
    const std::string biomeName = world == nullptr ? std::string{} : toStd(env, biome);
    return world != nullptr && guarded<bool>(false, [&] { return world->setBiomeAt(x, y, z, biomeName); }) ? JNI_TRUE
                                                                                                           : JNI_FALSE;
}

jint JNICALL nativeHighest(JNIEnv* env, jclass, jstring name, jint x, jint z, jstring kind, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::string heightmap = toStd(env, kind);
    return guarded<jint>(0, [&] { return world->highest(x, z, heightmap, load == JNI_TRUE); });
}

jboolean JNICALL nativeChunkLoaded(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world != nullptr && world->chunkLoaded(chunkX, chunkZ) ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeLoadChunk(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean generate) {
    const auto world = worldOf(env, name);
    return world != nullptr &&
                   guarded<bool>(false, [&] { return world->loadChunk(chunkX, chunkZ, generate == JNI_TRUE); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeUnloadChunk(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world != nullptr && guarded<bool>(false, [&] { return world->unloadChunk(chunkX, chunkZ); }) ? JNI_TRUE
                                                                                                        : JNI_FALSE;
}

jintArray JNICALL nativeLoadedChunks(JNIEnv* env, jclass, jstring name) {
    const auto world = worldOf(env, name);
    return intArray(env, world == nullptr ? std::vector<std::int32_t>{} : world->loadedChunks());
}

jintArray JNICALL nativeEntityIds(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean all) {
    const auto world = worldOf(env, name);
    std::vector<std::int32_t> ids;
    if (world != nullptr) {
        const auto found = all == JNI_TRUE ? world->entities() : world->entitiesIn(chunkX, chunkZ);
        for (const std::shared_ptr<PluginEntityRef>& entity : found) {
            ids.push_back(entity->id());
        }
    }
    return intArray(env, ids);
}

jobjectArray JNICALL nativeEntityStrings(JNIEnv* env, jclass, jstring name, jint id) {
    const auto world = worldOf(env, name);
    const auto entity = world == nullptr ? nullptr : world->entityById(id);
    if (entity == nullptr) {
        return nullptr;
    }
    const std::optional<std::string> custom = entity->customName();
    return stringArray(env, {entity->uuid(), entity->type(), custom.value_or(""), custom.has_value() ? "1" : "0"});
}

jdoubleArray JNICALL nativeEntityNumbers(JNIEnv* env, jclass, jstring name, jint id) {
    const auto world = worldOf(env, name);
    const auto entity = world == nullptr ? nullptr : world->entityById(id);
    if (entity == nullptr) {
        return nullptr;
    }
    const std::array<float, 2> rotation = entity->rotation();
    const std::array<double, 3> velocity = entity->velocity();
    const std::array<double, 13> values{entity->x(),
                                        entity->y(),
                                        entity->z(),
                                        static_cast<double>(rotation[0]),
                                        static_cast<double>(rotation[1]),
                                        velocity[0],
                                        velocity[1],
                                        velocity[2],
                                        entity->health(),
                                        entity->onGround() ? 1.0 : 0.0,
                                        static_cast<double>(entity->age()),
                                        entity->width(),
                                        entity->height()};
    return doubleArray(env, values.data(), values.size());
}

jboolean JNICALL nativeEntityMove(JNIEnv* env, jclass, jstring name, jint id, jdouble x, jdouble y, jdouble z,
                                  jfloat yaw, jfloat pitch) {
    const auto world = worldOf(env, name);
    const auto entity = world == nullptr ? nullptr : world->entityById(id);
    if (entity == nullptr || !entity->teleport(x, y, z)) {
        return JNI_FALSE;
    }
    entity->setRotation(yaw, pitch);
    return JNI_TRUE;
}

void JNICALL nativeEntityVelocity(JNIEnv* env, jclass, jstring name, jint id, jdouble x, jdouble y, jdouble z) {
    const auto world = worldOf(env, name);
    if (const auto entity = world == nullptr ? nullptr : world->entityById(id); entity != nullptr) {
        entity->setVelocity(x, y, z);
    }
}

void JNICALL nativeEntityHealth(JNIEnv* env, jclass, jstring name, jint id, jdouble health) {
    const auto world = worldOf(env, name);
    if (const auto entity = world == nullptr ? nullptr : world->entityById(id); entity != nullptr) {
        entity->setHealth(health);
    }
}

void JNICALL nativeEntityRemove(JNIEnv* env, jclass, jstring name, jint id) {
    const auto world = worldOf(env, name);
    if (const auto entity = world == nullptr ? nullptr : world->entityById(id); entity != nullptr) {
        entity->remove();
    }
}

jint JNICALL nativeSpawn(JNIEnv* env, jclass, jstring name, jstring type, jdouble x, jdouble y, jdouble z) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    const auto spawned =
        guarded<std::shared_ptr<PluginEntityRef>>(nullptr, [&] { return world->spawn(toStd(env, type), x, y, z); });
    return spawned == nullptr ? -1 : spawned->id();
}

jboolean JNICALL nativePlayerMove(JNIEnv* env, jclass, jstring uuid, jstring world, jdouble x, jdouble y, jdouble z,
                                  jfloat yaw, jfloat pitch) {
    const auto player = playerOf(env, uuid);
    return player != nullptr && player->teleport(toStd(env, world), x, y, z, yaw, pitch) ? JNI_TRUE : JNI_FALSE;
}

jdoubleArray JNICALL nativePlayerExtra(JNIEnv* env, jclass, jstring uuid) {
    const auto player = playerOf(env, uuid);
    if (player == nullptr) {
        return nullptr;
    }
    const std::array<double, 12> values{static_cast<double>(player->yaw()),
                                        static_cast<double>(player->pitch()),
                                        static_cast<double>(player->permissionLevel()),
                                        player->flying() ? 1.0 : 0.0,
                                        player->onGround() ? 1.0 : 0.0,
                                        player->sprinting() ? 1.0 : 0.0,
                                        static_cast<double>(player->entityId()),
                                        static_cast<double>(player->experienceProgress()),
                                        static_cast<double>(player->experiencePoints()),
                                        static_cast<double>(player->saturation()),
                                        static_cast<double>(player->inventory().heldSlot()),
                                        0.0};
    return doubleArray(env, values.data(), values.size());
}

jstring JNICALL nativePlayerAddress(JNIEnv* env, jclass, jstring uuid) {
    const auto player = playerOf(env, uuid);
    return player == nullptr ? nullptr : toJava(env, player->address());
}

void JNICALL nativePlayerShowBlock(JNIEnv* env, jclass, jstring uuid, jint x, jint y, jint z, jint state) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->showBlock(x, y, z, state);
    }
}

void JNICALL nativePlayerExperience(JNIEnv* env, jclass, jstring uuid, jfloat progress, jint level) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->setExperience(progress, level);
    }
}

jboolean JNICALL nativePlayerCommand(JNIEnv* env, jclass, jstring uuid, jstring line) {
    const auto player = playerOf(env, uuid);
    return player != nullptr && BukkitBridge::current().services().commands().runAsPlayer(*player, toStd(env, line))
               ? JNI_TRUE
               : JNI_FALSE;
}

void JNICALL nativePlayerTitle(JNIEnv* env, jclass, jstring uuid, jstring title, jstring subtitle, jint fadeIn,
                               jint stay, jint fadeOut) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->title(toStd(env, title), toStd(env, subtitle), fadeIn, stay, fadeOut);
    }
}

void JNICALL nativePlayerSound(JNIEnv* env, jclass, jstring uuid, jstring sound, jfloat volume, jfloat pitch) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->sound(toStd(env, sound), volume, pitch);
    }
}

// An item crosses as strings: id, count, damage, name (null when none), lore joined by newlines, then `id=level` pairs.
jobjectArray itemStrings(JNIEnv* env, const std::optional<PluginItem>& item) {
    if (!item.has_value()) {
        return nullptr;
    }
    std::vector<std::string> flat{item->id, std::to_string(item->count), std::to_string(item->damage),
                                  item->name.value_or(""), item->name.has_value() ? "1" : "0"};
    std::string lore;
    for (const std::string& line : item->lore) {
        lore += lore.empty() ? line : "\n" + line;
    }
    flat.push_back(lore);
    for (const auto& [id, level] : item->enchantments) {
        flat.push_back(id + "=" + std::to_string(level));
    }
    return stringArray(env, flat);
}

jint JNICALL nativeInventorySize(JNIEnv* env, jclass, jstring uuid) {
    const auto player = playerOf(env, uuid);
    return player == nullptr ? 0 : player->inventory().size();
}

jint JNICALL nativeInventoryHeld(JNIEnv* env, jclass, jstring uuid) {
    const auto player = playerOf(env, uuid);
    return player == nullptr ? 0 : player->inventory().heldSlot();
}

jobjectArray JNICALL nativeInventoryGet(JNIEnv* env, jclass, jstring uuid, jint slot) {
    const auto player = playerOf(env, uuid);
    return player == nullptr ? nullptr : itemStrings(env, player->inventory().at(slot));
}

void JNICALL nativeInventorySet(JNIEnv* env, jclass, jstring uuid, jint slot, jobjectArray item) {
    const auto player = playerOf(env, uuid);
    if (player == nullptr) {
        return;
    }
    if (item == nullptr) {
        player->inventory().set(slot, std::nullopt);
        return;
    }
    const auto at = [&](jsize index) {
        return toStd(env, static_cast<jstring>(env->GetObjectArrayElement(item, index)));
    };
    PluginItem built;
    built.id = at(0);
    built.count = std::atoi(at(1).c_str());
    built.damage = std::atoi(at(2).c_str());
    if (at(4) == "1") {
        built.name = at(3);
    }
    const std::string lore = at(5);
    for (std::size_t from = 0; !lore.empty() && from <= lore.size();) {
        const std::size_t end = lore.find('\n', from);
        built.lore.push_back(lore.substr(from, end == std::string::npos ? std::string::npos : end - from));
        if (end == std::string::npos) {
            break;
        }
        from = end + 1;
    }
    for (jsize index = 6; index < env->GetArrayLength(item); ++index) {
        const std::string pair = at(index);
        if (const std::size_t equals = pair.rfind('='); equals != std::string::npos) {
            built.enchantments[pair.substr(0, equals)] = std::atoi(pair.c_str() + equals + 1);
        }
    }
    player->inventory().set(slot, built);
}

void JNICALL nativeInventoryClear(JNIEnv* env, jclass, jstring uuid) {
    if (const auto player = playerOf(env, uuid); player != nullptr) {
        player->inventory().clear();
    }
}

// Worlds handed out by number so the hot version check needs no name lookup; they live as long as the process.
constexpr std::size_t kMaxHandles = 256;
std::array<std::shared_ptr<PluginWorldRef>, kMaxHandles> gHandleOwners;
std::array<std::atomic<PluginWorldRef*>, kMaxHandles> gHandles{};
std::atomic<std::int32_t> gHandleCount{0};
std::mutex gHandleMutex;

jint JNICALL nativeWorldHandle(JNIEnv* env, jclass, jstring name) {
    const std::string wanted = toStd(env, name);
    const std::lock_guard<std::mutex> lock{gHandleMutex};
    const std::int32_t count = gHandleCount.load();
    for (std::int32_t at = 0; at < count; ++at) {
        if (gHandles[static_cast<std::size_t>(at)].load()->name() == wanted) {
            return at;
        }
    }
    const auto world = worlds().byName(wanted);
    if (world == nullptr || count >= static_cast<std::int32_t>(kMaxHandles)) {
        return -1;
    }
    gHandleOwners[static_cast<std::size_t>(count)] = world;
    gHandles[static_cast<std::size_t>(count)].store(world.get());
    gHandleCount.store(count + 1);
    return count;
}

jlong JNICALL nativeBlockVersion(JNIEnv*, jclass, jint handle) {
    if (handle < 0 || handle >= gHandleCount.load()) {
        return -1;
    }
    return static_cast<jlong>(gHandles[static_cast<std::size_t>(handle)].load()->blockVersion());
}

[[nodiscard]] std::vector<std::int32_t> ints(JNIEnv* env, jintArray array) {
    std::vector<std::int32_t> values(array == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(array)));
    if (!values.empty()) {
        env->GetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    }
    return values;
}

[[nodiscard]] std::vector<std::uint8_t> bytes(JNIEnv* env, jbyteArray array) {
    std::vector<std::uint8_t> values(array == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(array)));
    if (!values.empty()) {
        env->GetByteArrayRegion(array, 0, static_cast<jsize>(values.size()), reinterpret_cast<jbyte*>(values.data()));
    }
    return values;
}

[[nodiscard]] jbyteArray byteArray(JNIEnv* env, const std::optional<std::vector<std::uint8_t>>& values) {
    if (!values.has_value()) {
        return nullptr;
    }
    jbyteArray array = env->NewByteArray(static_cast<jsize>(values->size()));
    env->SetByteArrayRegion(array, 0, static_cast<jsize>(values->size()),
                            reinterpret_cast<const jbyte*>(values->data()));
    return array;
}

jint JNICALL nativeApplyChunks(JNIEnv* env, jclass, jstring name, jintArray chunks, jintArray sections,
                               jintArray states, jboolean resend, jintArray previous) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return -1;
    }
    const std::vector<std::int32_t> chunkList = ints(env, chunks);
    const std::vector<std::int32_t> sectionList = ints(env, sections);
    const std::vector<std::int32_t> next = ints(env, states);
    std::vector<std::int32_t> before(previous == nullptr ? 0 : static_cast<std::size_t>(env->GetArrayLength(previous)));
    const jint changed =
        guarded<jint>(-1, [&] { return world->applyChunks(chunkList, sectionList, next, resend == JNI_TRUE, before); });
    if (previous != nullptr && !before.empty()) {
        env->SetIntArrayRegion(previous, 0, static_cast<jsize>(before.size()), before.data());
    }
    return changed;
}

jobjectArray JNICALL nativeBlockEntityTypes(JNIEnv* env, jclass) {
    return stringArray(env, worlds().blockEntityTypes());
}

jint JNICALL nativeDataVersion(JNIEnv*, jclass) { return worlds().dataVersion(); }

jbyteArray JNICALL nativeFixNbt(JNIEnv* env, jclass, jstring kind, jbyteArray nbt, jint fromVersion) {
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    return byteArray(env, worlds().fixNbt(toStd(env, kind), data, fromVersion));
}

jstring JNICALL nativeFixName(JNIEnv* env, jclass, jstring kind, jstring text, jint fromVersion) {
    return javaOrNull(env, worlds().fixName(toStd(env, kind), toStd(env, text), fromVersion));
}

jbyteArray JNICALL nativeBlockEntityNbt(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    return byteArray(env, guarded<std::optional<std::vector<std::uint8_t>>>(
                              std::nullopt, [&] { return world->blockEntityNbt(x, y, z); }));
}

jboolean JNICALL nativeSetBlockEntityNbt(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    return world != nullptr && guarded<bool>(false, [&] { return world->setBlockEntityNbt(x, y, z, data); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jintArray JNICALL nativeBlockEntityPositions(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return intArray(env, world == nullptr ? std::vector<std::int32_t>{} : guarded<std::vector<std::int32_t>>({}, [&] {
                        return world->blockEntityPositions(chunkX, chunkZ);
                    }));
}

jbyteArray JNICALL nativeEntityNbt(JNIEnv* env, jclass, jstring name, jint id) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    return byteArray(
        env, guarded<std::optional<std::vector<std::uint8_t>>>(std::nullopt, [&] { return world->entityNbt(id); }));
}

jint JNICALL nativeSpawnEntityNbt(JNIEnv* env, jclass, jstring name, jbyteArray nbt, jstring cause) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    const std::string why = toStd(env, cause);
    return world == nullptr ? -1 : guarded<jint>(-1, [&] { return world->spawnEntityNbt(data, why); });
}

jboolean JNICALL nativeCreateBlockEntity(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jbyteArray nbt) {
    const auto world = worldOf(env, name);
    const std::vector<std::uint8_t> data = bytes(env, nbt);
    return world != nullptr && guarded<bool>(false, [&] { return world->createBlockEntity(x, y, z, data); })
               ? JNI_TRUE
               : JNI_FALSE;
}

jboolean JNICALL nativeRemoveBlockEntity(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z) {
    const auto world = worldOf(env, name);
    return world != nullptr && guarded<bool>(false, [&] { return world->removeBlockEntity(x, y, z); }) ? JNI_TRUE
                                                                                                       : JNI_FALSE;
}

jint JNICALL nativeEntityByUuid(JNIEnv* env, jclass, jstring name, jstring uuid) {
    const auto world = worldOf(env, name);
    const std::string wanted = toStd(env, uuid);
    return world == nullptr ? -1 : guarded<jint>(-1, [&] {
        const std::shared_ptr<PluginEntityRef> found = world->entityByUuid(wanted);
        return found == nullptr ? -1 : found->id();
    });
}

jboolean JNICALL nativeRemoveEntityByUuid(JNIEnv* env, jclass, jstring name, jstring uuid) {
    const auto world = worldOf(env, name);
    const std::string wanted = toStd(env, uuid);
    return world != nullptr && guarded<bool>(false, [&] { return world->removeEntityByUuid(wanted); }) ? JNI_TRUE
                                                                                                       : JNI_FALSE;
}

jboolean JNICALL nativeReadBiomes(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jintArray out) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> read(static_cast<std::size_t>(env->GetArrayLength(out)));
    const bool ok = guarded<bool>(false, [&] { return world->readBiomes(chunkX, chunkZ, read); });
    if (ok) {
        env->SetIntArrayRegion(out, 0, static_cast<jsize>(read.size()), read.data());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

jboolean JNICALL nativeWriteBiomes(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jintArray ids) {
    const auto world = worldOf(env, name);
    const std::vector<std::int32_t> next = ints(env, ids);
    return world != nullptr && guarded<bool>(false, [&] { return world->writeBiomes(chunkX, chunkZ, next); })
               ? JNI_TRUE
               : JNI_FALSE;
}

void JNICALL nativeRelightChunks(JNIEnv* env, jclass, jstring name, jintArray chunks) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return;
    }
    const std::vector<std::int32_t> pairs = ints(env, chunks);
    guarded<int>(0, [&] {
        world->relightChunks(pairs);
        return 0;
    });
}

jboolean JNICALL nativeReadSections(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean load,
                                    jintArray out) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    std::vector<std::int32_t> read(static_cast<std::size_t>(env->GetArrayLength(out)));
    // A loaded chunk is copied under the world's read lock, so no thread hop is needed.
    bool ok = world->readSections(chunkX, chunkZ, false, read);
    if (!ok && load == JNI_TRUE) {
        ok = guarded<bool>(false, [&] { return world->readSections(chunkX, chunkZ, true, read); });
    }
    if (ok) {
        env->SetIntArrayRegion(out, 0, static_cast<jsize>(read.size()), read.data());
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

jint JNICALL nativeChunkStatus(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ) {
    const auto world = worldOf(env, name);
    return world == nullptr ? 0 : world->chunkStatus(chunkX, chunkZ);
}

// Holds a Java object across threads and lets go of it wherever it is last used.
struct JavaRef {
    explicit JavaRef(JNIEnv* env, jobject object) : ref(env->NewGlobalRef(object)) {}

    JavaRef(const JavaRef&) = delete;
    JavaRef& operator=(const JavaRef&) = delete;

    ~JavaRef() {
        if (JNIEnv* env = BukkitBridge::current().env(); env != nullptr && ref != nullptr) {
            env->DeleteGlobalRef(ref);
        }
    }

    jobject ref;
};

void JNICALL nativeRequestChunk(JNIEnv* env, jclass, jstring name, jint chunkX, jint chunkZ, jboolean generate,
                                jint priority, jobject future) {
    const auto world = worldOf(env, name);
    const auto held = std::make_shared<JavaRef>(env, future);
    const auto finish = [held](bool loaded) {
        JNIEnv* callEnv = BukkitBridge::current().env();
        if (callEnv == nullptr) {
            return;
        }
        jclass booleanClass = callEnv->FindClass("java/lang/Boolean");
        jmethodID valueOf = callEnv->GetStaticMethodID(booleanClass, "valueOf", "(Z)Ljava/lang/Boolean;");
        jobject value = callEnv->CallStaticObjectMethod(booleanClass, valueOf, loaded ? JNI_TRUE : JNI_FALSE);
        jclass futureClass = callEnv->FindClass("java/util/concurrent/CompletableFuture");
        jmethodID complete = callEnv->GetMethodID(futureClass, "complete", "(Ljava/lang/Object;)Z");
        callEnv->CallBooleanMethod(held->ref, complete, value);
        callEnv->ExceptionClear();
        callEnv->DeleteLocalRef(value);
        callEnv->DeleteLocalRef(booleanClass);
        callEnv->DeleteLocalRef(futureClass);
    };
    if (world == nullptr) {
        finish(false);
        return;
    }
    BukkitBridge& bridge = BukkitBridge::current();
    const auto start = [world, chunkX, chunkZ, generate, priority, finish] {
        world->requestChunk(chunkX, chunkZ, generate == JNI_TRUE, priority, finish);
    };
    if (bridge.plugins().onPluginThread()) {
        start();
    } else {
        static_cast<void>(bridge.services().tasks().after(0, start));
    }
}

jboolean JNICALL nativeHoldChunks(JNIEnv* env, jclass, jstring name, jstring token, jintArray chunks) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    const std::string holder = toStd(env, token);
    const std::vector<std::int32_t> pairs = ints(env, chunks);
    return guarded<bool>(false, [&] { return world->holdChunks(holder, pairs); }) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeReleaseChunks(JNIEnv* env, jclass, jstring name, jstring token) {
    if (const auto world = worldOf(env, name); world != nullptr) {
        const std::string holder = toStd(env, token);
        guarded<int>(0, [&] {
            world->releaseChunks(holder);
            return 0;
        });
    }
}

jint JNICALL nativeUpdateNeighbors(JNIEnv* env, jclass, jstring name, jintArray positions, jint effects) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    return guarded<jint>(0, [&] { return world->updateNeighbors(at, static_cast<std::uint32_t>(effects)); });
}

void JNICALL nativeLightJob(JNIEnv* env, jclass, jstring name, jintArray chunks, jobject future) {
    const auto world = worldOf(env, name);
    const auto held = std::make_shared<JavaRef>(env, future);
    const auto finish = [held](std::int32_t relit) {
        JNIEnv* callEnv = BukkitBridge::current().env();
        if (callEnv == nullptr) {
            return;
        }
        jclass integerClass = callEnv->FindClass("java/lang/Integer");
        jmethodID valueOf = callEnv->GetStaticMethodID(integerClass, "valueOf", "(I)Ljava/lang/Integer;");
        jobject value = callEnv->CallStaticObjectMethod(integerClass, valueOf, relit);
        jclass futureClass = callEnv->FindClass("java/util/concurrent/CompletableFuture");
        jmethodID complete = callEnv->GetMethodID(futureClass, "complete", "(Ljava/lang/Object;)Z");
        callEnv->CallBooleanMethod(held->ref, complete, value);
        callEnv->ExceptionClear();
        callEnv->DeleteLocalRef(value);
        callEnv->DeleteLocalRef(integerClass);
        callEnv->DeleteLocalRef(futureClass);
    };
    if (world == nullptr) {
        finish(0);
        return;
    }
    const std::vector<std::int32_t> pairs = ints(env, chunks);
    world->lightJob(pairs, finish);
}

jint JNICALL nativeLightAt(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint layer) {
    const auto world = worldOf(env, name);
    return world == nullptr ? -1 : world->lightAt(x, y, z, layer);
}

jbyteArray JNICALL nativeReadLight(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                   jint layer, jboolean load) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return nullptr;
    }
    std::vector<std::uint8_t> read(2048);
    bool ok = world->readLight(chunkX, sectionY, chunkZ, layer, false, read);
    if (!ok && load == JNI_TRUE) {
        ok = guarded<bool>(false, [&] { return world->readLight(chunkX, sectionY, chunkZ, layer, true, read); });
    }
    if (!ok) {
        return nullptr;
    }
    jbyteArray out = env->NewByteArray(static_cast<jsize>(read.size()));
    env->SetByteArrayRegion(out, 0, static_cast<jsize>(read.size()), reinterpret_cast<const jbyte*>(read.data()));
    return out;
}

jboolean JNICALL nativeWriteLight(JNIEnv* env, jclass, jstring name, jint chunkX, jint sectionY, jint chunkZ,
                                  jint layer, jbyteArray nibbles) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    const std::vector<std::uint8_t> data = bytes(env, nibbles);
    return world->writeLight(chunkX, sectionY, chunkZ, layer, data) ? JNI_TRUE : JNI_FALSE;
}

void JNICALL nativeCheckLight(JNIEnv* env, jclass, jstring name, jintArray positions) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return;
    }
    world->checkLight(ints(env, positions));
}

jint JNICALL nativeUseItemOn(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint face, jstring item,
                             jfloat yaw, jfloat pitch) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::string held = toStd(env, item);
    return guarded<jint>(0, [&] { return world->useItemOn(x, y, z, face, held, yaw, pitch); });
}

jboolean JNICALL nativeScheduleTick(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint delay,
                                    jboolean fluid) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return JNI_FALSE;
    }
    return guarded<bool>(false, [&] { return world->scheduleTick(x, y, z, delay, fluid != JNI_FALSE); }) ? JNI_TRUE
                                                                                                         : JNI_FALSE;
}

jint JNICALL nativeValidState(JNIEnv* env, jclass, jstring name, jint x, jint y, jint z, jint state) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return state;
    }
    return guarded<jint>(state, [&] { return world->validState(x, y, z, state); });
}

jint JNICALL nativeSettleBlocks(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray previous,
                                jint effects) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> was = ints(env, previous);
    return guarded<jint>(0, [&] { return world->settleBlocks(at, was, static_cast<std::uint32_t>(effects)); });
}

jint JNICALL nativeNeighborChanged(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray sources) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> told = ints(env, sources);
    return guarded<jint>(0, [&] { return world->neighborChanged(at, told); });
}

jint JNICALL nativeUpdateShapes(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray states,
                                jboolean indirect) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> next = ints(env, states);
    return guarded<jint>(0, [&] { return world->updateShapes(at, next, indirect != JNI_FALSE); });
}

jint JNICALL nativeApplyChanges(JNIEnv* env, jclass, jstring name, jintArray positions, jintArray states,
                                jint effects) {
    const auto world = worldOf(env, name);
    if (world == nullptr) {
        return 0;
    }
    const std::vector<std::int32_t> at = ints(env, positions);
    const std::vector<std::int32_t> next = ints(env, states);
    return guarded<jint>(0, [&] { return world->applyChanges(at, next, static_cast<std::uint32_t>(effects)); });
}
} // namespace

void registerWorldNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("stateId", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeStateId)),
        method("stateText", "(I)Ljava/lang/String;", reinterpret_cast<void*>(&nativeStateText)),
        method("blockTable", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBlockTable)),
        method("blockProperties", "(Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeBlockProperties)),
        method("stateFlags", "(I)I", reinterpret_cast<void*>(&nativeStateFlags)),
        method("blockHardness", "(Ljava/lang/String;)[D", reinterpret_cast<void*>(&nativeBlockHardness)),
        method("blockInfo", "(I)[D", reinterpret_cast<void*>(&nativeBlockInfo)),
        method("biomeIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBiomeIds)),
        method("itemIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeItemIds)),
        method("registryIds", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeRegistryIds)),
        method("tags", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeTags)),
        method("entityTypes", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeEntityTypes)),
        method("worldStrings", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeWorldStrings)),
        method("worldNumbers", "(Ljava/lang/String;)[J", reinterpret_cast<void*>(&nativeWorldNumbers)),
        method("setTime", "(Ljava/lang/String;J)V", reinterpret_cast<void*>(&nativeSetTime)),
        method("setWeather", "(Ljava/lang/String;ZZ)V", reinterpret_cast<void*>(&nativeSetWeather)),
        method("setSpawn", "(Ljava/lang/String;III)V", reinterpret_cast<void*>(&nativeSetSpawn)),
        method("setDifficulty", "(Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeSetDifficulty)),
        method("gameRule", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeGameRule)),
        method("setGameRule", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeSetGameRule)),
        method("gameRules", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeGameRules)),
        method("saveWorld", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeSaveWorld)),
        method("state", "(Ljava/lang/String;IIIZ)I", reinterpret_cast<void*>(&nativeState)),
        method("setState", "(Ljava/lang/String;IIIIZ)Z", reinterpret_cast<void*>(&nativeSetState)),
        method("readStates", "(Ljava/lang/String;[I[IZ)I", reinterpret_cast<void*>(&nativeReadStates)),
        method("writeStates", "(Ljava/lang/String;[I[IZ)I", reinterpret_cast<void*>(&nativeWriteStates)),
        method("readSection", "(Ljava/lang/String;IIIZ[I)Z", reinterpret_cast<void*>(&nativeReadSection)),
        method("writeSection", "(Ljava/lang/String;III[IZ)Z", reinterpret_cast<void*>(&nativeWriteSection)),
        method("applySections", "(Ljava/lang/String;II[I[IZ[I)I", reinterpret_cast<void*>(&nativeApplySections)),
        method("worldHandle", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeWorldHandle)),
        method("blockVersion", "(I)J", reinterpret_cast<void*>(&nativeBlockVersion)),
        method("applyChunks", "(Ljava/lang/String;[I[I[IZ[I)I", reinterpret_cast<void*>(&nativeApplyChunks)),
        method("blockEntityNbt", "(Ljava/lang/String;III)[B", reinterpret_cast<void*>(&nativeBlockEntityNbt)),
        method("setBlockEntityNbt", "(Ljava/lang/String;III[B)Z", reinterpret_cast<void*>(&nativeSetBlockEntityNbt)),
        method("blockEntityPositions", "(Ljava/lang/String;II)[I",
               reinterpret_cast<void*>(&nativeBlockEntityPositions)),
        method("entityNbt", "(Ljava/lang/String;I)[B", reinterpret_cast<void*>(&nativeEntityNbt)),
        method("spawnEntityNbt", "(Ljava/lang/String;[BLjava/lang/String;)I",
               reinterpret_cast<void*>(&nativeSpawnEntityNbt)),
        method("blockEntityTypes", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBlockEntityTypes)),
        method("dataVersion", "()I", reinterpret_cast<void*>(&nativeDataVersion)),
        method("fixNbt", "(Ljava/lang/String;[BI)[B", reinterpret_cast<void*>(&nativeFixNbt)),
        method("fixName", "(Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeFixName)),
        method("createBlockEntity", "(Ljava/lang/String;III[B)Z", reinterpret_cast<void*>(&nativeCreateBlockEntity)),
        method("removeBlockEntity", "(Ljava/lang/String;III)Z", reinterpret_cast<void*>(&nativeRemoveBlockEntity)),
        method("entityByUuid", "(Ljava/lang/String;Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeEntityByUuid)),
        method("removeEntityByUuid", "(Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeRemoveEntityByUuid)),
        method("readBiomes", "(Ljava/lang/String;II[I)Z", reinterpret_cast<void*>(&nativeReadBiomes)),
        method("writeBiomes", "(Ljava/lang/String;II[I)Z", reinterpret_cast<void*>(&nativeWriteBiomes)),
        method("relightChunks", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeRelightChunks)),
        method("readSections", "(Ljava/lang/String;IIZ[I)Z", reinterpret_cast<void*>(&nativeReadSections)),
        method("chunkStatus", "(Ljava/lang/String;II)I", reinterpret_cast<void*>(&nativeChunkStatus)),
        method("requestChunk", "(Ljava/lang/String;IIZILjava/util/concurrent/CompletableFuture;)V",
               reinterpret_cast<void*>(&nativeRequestChunk)),
        method("holdChunks", "(Ljava/lang/String;Ljava/lang/String;[I)Z", reinterpret_cast<void*>(&nativeHoldChunks)),
        method("releaseChunks", "(Ljava/lang/String;Ljava/lang/String;)V",
               reinterpret_cast<void*>(&nativeReleaseChunks)),
        method("updateNeighbors", "(Ljava/lang/String;[II)I", reinterpret_cast<void*>(&nativeUpdateNeighbors)),
        method("applyChanges", "(Ljava/lang/String;[I[II)I", reinterpret_cast<void*>(&nativeApplyChanges)),
        method("lightJob", "(Ljava/lang/String;[ILjava/util/concurrent/CompletableFuture;)V",
               reinterpret_cast<void*>(&nativeLightJob)),
        method("lightAt", "(Ljava/lang/String;IIII)I", reinterpret_cast<void*>(&nativeLightAt)),
        method("readLight", "(Ljava/lang/String;IIIIZ)[B", reinterpret_cast<void*>(&nativeReadLight)),
        method("writeLight", "(Ljava/lang/String;IIII[B)Z", reinterpret_cast<void*>(&nativeWriteLight)),
        method("checkLight", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeCheckLight)),
        method("useItemOn", "(Ljava/lang/String;IIIILjava/lang/String;FF)I", reinterpret_cast<void*>(&nativeUseItemOn)),
        method("scheduleTick", "(Ljava/lang/String;IIIIZ)Z", reinterpret_cast<void*>(&nativeScheduleTick)),
        method("validState", "(Ljava/lang/String;IIII)I", reinterpret_cast<void*>(&nativeValidState)),
        method("settleBlocks", "(Ljava/lang/String;[I[II)I", reinterpret_cast<void*>(&nativeSettleBlocks)),
        method("neighborChanged", "(Ljava/lang/String;[I[I)I", reinterpret_cast<void*>(&nativeNeighborChanged)),
        method("updateShapes", "(Ljava/lang/String;[I[IZ)I", reinterpret_cast<void*>(&nativeUpdateShapes)),
        method("resendChunks", "(Ljava/lang/String;[I)V", reinterpret_cast<void*>(&nativeResendChunks)),
        method("biome", "(Ljava/lang/String;III)Ljava/lang/String;", reinterpret_cast<void*>(&nativeBiome)),
        method("setBiome", "(Ljava/lang/String;IIILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSetBiome)),
        method("highest", "(Ljava/lang/String;IILjava/lang/String;Z)I", reinterpret_cast<void*>(&nativeHighest)),
        method("chunkLoaded", "(Ljava/lang/String;II)Z", reinterpret_cast<void*>(&nativeChunkLoaded)),
        method("loadChunk", "(Ljava/lang/String;IIZ)Z", reinterpret_cast<void*>(&nativeLoadChunk)),
        method("unloadChunk", "(Ljava/lang/String;II)Z", reinterpret_cast<void*>(&nativeUnloadChunk)),
        method("loadedChunks", "(Ljava/lang/String;)[I", reinterpret_cast<void*>(&nativeLoadedChunks)),
        method("entityIds", "(Ljava/lang/String;IIZ)[I", reinterpret_cast<void*>(&nativeEntityIds)),
        method("entityStrings", "(Ljava/lang/String;I)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeEntityStrings)),
        method("entityNumbers", "(Ljava/lang/String;I)[D", reinterpret_cast<void*>(&nativeEntityNumbers)),
        method("entityMove", "(Ljava/lang/String;IDDDFF)Z", reinterpret_cast<void*>(&nativeEntityMove)),
        method("entityVelocity", "(Ljava/lang/String;IDDD)V", reinterpret_cast<void*>(&nativeEntityVelocity)),
        method("entityHealth", "(Ljava/lang/String;ID)V", reinterpret_cast<void*>(&nativeEntityHealth)),
        method("entityRemove", "(Ljava/lang/String;I)V", reinterpret_cast<void*>(&nativeEntityRemove)),
        method("spawn", "(Ljava/lang/String;Ljava/lang/String;DDD)I", reinterpret_cast<void*>(&nativeSpawn)),
        method("playerMove", "(Ljava/lang/String;Ljava/lang/String;DDDFF)Z",
               reinterpret_cast<void*>(&nativePlayerMove)),
        method("playerExtra", "(Ljava/lang/String;)[D", reinterpret_cast<void*>(&nativePlayerExtra)),
        method("playerAddress", "(Ljava/lang/String;)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativePlayerAddress)),
        method("playerShowBlock", "(Ljava/lang/String;IIII)V", reinterpret_cast<void*>(&nativePlayerShowBlock)),
        method("playerExperience", "(Ljava/lang/String;FI)V", reinterpret_cast<void*>(&nativePlayerExperience)),
        method("playerCommand", "(Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativePlayerCommand)),
        method("playerTitle", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;III)V",
               reinterpret_cast<void*>(&nativePlayerTitle)),
        method("playerSound", "(Ljava/lang/String;Ljava/lang/String;FF)V", reinterpret_cast<void*>(&nativePlayerSound)),
        method("inventorySize", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeInventorySize)),
        method("inventoryHeld", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeInventoryHeld)),
        method("inventoryGet", "(Ljava/lang/String;I)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeInventoryGet)),
        method("inventorySet", "(Ljava/lang/String;I[Ljava/lang/String;)V",
               reinterpret_cast<void*>(&nativeInventorySet)),
        method("inventoryClear", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeInventoryClear)),
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
