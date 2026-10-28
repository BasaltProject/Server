#include "BukkitWorldNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

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
    const std::string kind = toStd(env, type);
    const auto spawned =
        guarded<std::shared_ptr<PluginEntityRef>>(nullptr, [&] { return world->spawn(kind, x, y, z); });
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
} // namespace

void registerWorldNativesEntities(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("entityIds", "(Ljava/lang/String;IIZ)[I", reinterpret_cast<void*>(&nativeEntityIds)),
        nativeMethod("entityStrings", "(Ljava/lang/String;I)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeEntityStrings)),
        nativeMethod("entityNumbers", "(Ljava/lang/String;I)[D", reinterpret_cast<void*>(&nativeEntityNumbers)),
        nativeMethod("entityMove", "(Ljava/lang/String;IDDDFF)Z", reinterpret_cast<void*>(&nativeEntityMove)),
        nativeMethod("entityVelocity", "(Ljava/lang/String;IDDD)V", reinterpret_cast<void*>(&nativeEntityVelocity)),
        nativeMethod("entityHealth", "(Ljava/lang/String;ID)V", reinterpret_cast<void*>(&nativeEntityHealth)),
        nativeMethod("entityRemove", "(Ljava/lang/String;I)V", reinterpret_cast<void*>(&nativeEntityRemove)),
        nativeMethod("spawn", "(Ljava/lang/String;Ljava/lang/String;DDD)I", reinterpret_cast<void*>(&nativeSpawn)),
        nativeMethod("playerMove", "(Ljava/lang/String;Ljava/lang/String;DDDFF)Z",
                     reinterpret_cast<void*>(&nativePlayerMove)),
        nativeMethod("playerExtra", "(Ljava/lang/String;)[D", reinterpret_cast<void*>(&nativePlayerExtra)),
        nativeMethod("playerAddress", "(Ljava/lang/String;)Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativePlayerAddress)),
        nativeMethod("playerShowBlock", "(Ljava/lang/String;IIII)V", reinterpret_cast<void*>(&nativePlayerShowBlock)),
        nativeMethod("playerExperience", "(Ljava/lang/String;FI)V", reinterpret_cast<void*>(&nativePlayerExperience)),
        nativeMethod("playerCommand", "(Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativePlayerCommand)),
        nativeMethod("playerTitle", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;III)V",
                     reinterpret_cast<void*>(&nativePlayerTitle)),
        nativeMethod("playerSound", "(Ljava/lang/String;Ljava/lang/String;FF)V",
                     reinterpret_cast<void*>(&nativePlayerSound)),
        nativeMethod("inventorySize", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeInventorySize)),
        nativeMethod("inventoryHeld", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeInventoryHeld)),
        nativeMethod("inventoryGet", "(Ljava/lang/String;I)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeInventoryGet)),
        nativeMethod("inventorySet", "(Ljava/lang/String;I[Ljava/lang/String;)V",
                     reinterpret_cast<void*>(&nativeInventorySet)),
        nativeMethod("inventoryClear", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeInventoryClear)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
