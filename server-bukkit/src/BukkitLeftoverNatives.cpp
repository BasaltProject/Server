#include "JniSupport.hpp"

#include <basalt/server/plugins/PluginDatafix.hpp>
#include <basalt/server/plugins/PluginStructures.hpp>
#include <basalt/server/plugins/PluginWindow.hpp>
#include <basalt/server/plugins/PluginWorldEnv.hpp>
#include <basalt/server/plugins/PluginWorldgen.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::bukkit {
namespace {
using Blob = std::optional<std::vector<std::uint8_t>>;

[[nodiscard]] jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

[[nodiscard]] plugins::PluginStructures& structures() { return BukkitBridge::current().services().structures(); }

jobjectArray JNICALL nativeStructureIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] { return structures().ids(); }));
}

jobjectArray JNICALL nativeStopwatchIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] {
                                  return BukkitBridge::current().services().serverControl().stopwatchIds();
                              }));
}

jdouble JNICALL nativeStopwatch(JNIEnv* env, jclass, jstring operation, jstring id) {
    const std::string what = toStd(env, operation);
    const std::string name = toStd(env, id);
    return jni::onServer<jdouble>(-1.0, [&] {
        const std::optional<double> value = BukkitBridge::current().services().serverControl().stopwatch(what, name);
        return value.has_value() ? static_cast<jdouble>(*value) : -1.0;
    });
}

jobjectArray JNICALL nativeFunctionIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] {
                                  return BukkitBridge::current().services().datapacks().functionIds();
                              }));
}

jobjectArray JNICALL nativeFunctionTagIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] {
                                  return BukkitBridge::current().services().datapacks().functionTagIds();
                              }));
}

jbyteArray JNICALL nativeResourceRead(JNIEnv* env, jclass, jstring name) {
    const std::string path = toStd(env, name);
    return jni::byteArrayOf(env, jni::onServer<Blob>(std::nullopt, [&] {
                                return BukkitBridge::current().services().datapacks().resource(path);
                            }));
}

jobjectArray JNICALL nativeResourcesUnder(JNIEnv* env, jclass, jstring prefix) {
    const std::string start = toStd(env, prefix);
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [&] {
                                  return BukkitBridge::current().services().datapacks().resourcesUnder(start);
                              }));
}

jstring JNICALL nativeResourcePack(JNIEnv* env, jclass, jstring name) {
    const std::string path = toStd(env, name);
    const std::string pack = jni::onServer<std::string>(
        {}, [&] { return BukkitBridge::current().services().datapacks().resourcePack(path); });
    return pack.empty() ? nullptr : env->NewStringUTF(pack.c_str());
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

jobjectArray JNICALL nativeTemplateScan(JNIEnv* env, jclass, jbyteArray nbt, jint x, jint y, jint z, jint rotation,
                                        jint mirror, jint pivotX, jint pivotZ, jstring block, jboolean absolute) {
    const std::vector<std::uint8_t> data = jni::bytesOf(env, nbt);
    const plugins::PluginTemplateScanRequest request{.x = x,
                                                     .y = y,
                                                     .z = z,
                                                     .rotation = rotation,
                                                     .mirror = mirror,
                                                     .pivotX = pivotX,
                                                     .pivotZ = pivotZ,
                                                     .block = toStd(env, block),
                                                     .absolute = absolute == JNI_TRUE};
    const std::optional<plugins::PluginTemplateScan> scan = jni::onServer<std::optional<plugins::PluginTemplateScan>>(
        std::nullopt, [&] { return structures().scan(data, request); });
    if (!scan.has_value()) {
        return nullptr;
    }
    const auto ints = [env](const std::vector<jint>& values) {
        jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
        env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
        return array;
    };
    std::vector<jint> blocks;
    for (const plugins::PluginTemplateBlock& entry : scan->blocks) {
        blocks.insert(blocks.end(), {entry.x, entry.y, entry.z, entry.stateId});
    }
    jclass byteArrayClass = env->FindClass("[B");
    jobjectArray tiles = env->NewObjectArray(static_cast<jsize>(scan->blocks.size()), byteArrayClass, nullptr);
    for (std::size_t index = 0; index < scan->blocks.size(); ++index) {
        if (!scan->blocks[index].nbt.empty()) {
            jbyteArray bytes = jni::byteArrayOf(env, scan->blocks[index].nbt);
            env->SetObjectArrayElement(tiles, static_cast<jsize>(index), bytes);
            env->DeleteLocalRef(bytes);
        }
    }
    std::vector<jint> jigsawInts;
    std::vector<std::string> jigsawText;
    for (const plugins::PluginTemplateJigsaw& entry : scan->jigsaws) {
        jigsawInts.insert(jigsawInts.end(), {entry.x, entry.y, entry.z, entry.rollable ? 1 : 0, entry.placementPriority,
                                             entry.selectionPriority});
        jigsawText.insert(jigsawText.end(),
                          {entry.front, entry.top, entry.name, entry.pool, entry.target, entry.finalState});
    }
    jclass object = env->FindClass("java/lang/Object");
    jobjectArray result = env->NewObjectArray(5, object, nullptr);
    jintArray size = ints({scan->sizeX, scan->sizeY, scan->sizeZ});
    jintArray blockInts = ints(blocks);
    jintArray jigsawIntArray = ints(jigsawInts);
    jobjectArray jigsawTextArray = jni::stringArrayOf(env, jigsawText);
    env->SetObjectArrayElement(result, 0, size);
    env->SetObjectArrayElement(result, 1, blockInts);
    env->SetObjectArrayElement(result, 2, tiles);
    env->SetObjectArrayElement(result, 3, jigsawIntArray);
    env->SetObjectArrayElement(result, 4, jigsawTextArray);
    return result;
}

jbyteArray JNICALL nativeDatafix(JNIEnv* env, jclass, jstring type, jbyteArray nbt, jint from, jint to) {
    return jni::byteArrayOf(env, plugins::datafix::upgrade(toStd(env, type), jni::bytesOf(env, nbt), from, to));
}

jboolean JNICALL nativeDatafixSupports(JNIEnv* env, jclass, jstring type) {
    return flag(plugins::datafix::supports(toStd(env, type)));
}

jintArray JNICALL nativeTerrainShape(JNIEnv* env, jclass, jstring settings) {
    const std::string name = toStd(env, settings);
    const std::optional<plugins::PluginTerrainShape> shape = jni::onServer<std::optional<plugins::PluginTerrainShape>>(
        std::nullopt, [&] { return BukkitBridge::current().services().worldgen().shape(name); });
    if (!shape.has_value()) {
        return nullptr;
    }
    const jint values[] = {shape->minY,
                           shape->height,
                           shape->seaLevel,
                           shape->defaultBlock,
                           shape->defaultFluid,
                           shape->legacyRandomSource ? 1 : 0,
                           shape->disableMobGeneration ? 1 : 0};
    jintArray array = env->NewIntArray(7);
    env->SetIntArrayRegion(array, 0, 7, values);
    return array;
}

jdouble JNICALL nativeTerrainDensity(JNIEnv* env, jclass, jstring settings, jlong seed, jstring function, jint x,
                                     jint y, jint z) {
    const std::string name = toStd(env, settings);
    const std::string router = toStd(env, function);
    return jni::onServer<jdouble>(std::nan(""), [&] {
        return BukkitBridge::current()
            .services()
            .worldgen()
            .density(name, seed, router, x, y, z)
            .value_or(std::nan(""));
    });
}

jdouble JNICALL nativeTerrainNoise(JNIEnv* env, jclass, jstring settings, jlong seed, jstring noise, jdouble x,
                                   jdouble y, jdouble z) {
    const std::string name = toStd(env, settings);
    const std::string id = toStd(env, noise);
    return jni::onServer<jdouble>(std::nan(""), [&] {
        return BukkitBridge::current().services().worldgen().noise(name, seed, id, x, y, z).value_or(std::nan(""));
    });
}

jdoubleArray JNICALL nativeTerrainNoiseRange(JNIEnv* env, jclass, jstring settings, jlong seed, jstring noise) {
    const std::string name = toStd(env, settings);
    const std::string id = toStd(env, noise);
    const auto range = jni::onServer<std::optional<std::pair<double, double>>>(
        std::nullopt, [&] { return BukkitBridge::current().services().worldgen().noiseRange(name, seed, id); });
    if (!range.has_value()) {
        return nullptr;
    }
    const jdouble values[] = {range->first, range->second};
    jdoubleArray array = env->NewDoubleArray(2);
    env->SetDoubleArrayRegion(array, 0, 2, values);
    return array;
}

plugins::PluginNoiseSeed noiseSeed(jboolean legacy, jlong low, jlong high, jboolean nether) {
    return {.legacyRandom = legacy == JNI_TRUE, .low = low, .high = high, .legacyNether = nether == JNI_TRUE};
}

jlong JNICALL nativeNoiseCreate(JNIEnv* env, jclass, jstring noise, jboolean legacy, jlong low, jlong high,
                                jboolean nether) {
    const std::string id = toStd(env, noise);
    const plugins::PluginNoiseSeed seed = noiseSeed(legacy, low, high, nether);
    return jni::onServer<jlong>(
        -1, [&] { return BukkitBridge::current().services().worldgen().createNoise(id, seed).value_or(-1); });
}

jlong JNICALL nativeNoiseCreateWith(JNIEnv* env, jclass, jdouble baseAmplitude, jint baseOctave, jint octaveCount,
                                    jint normalize, jdoubleArray modifiers, jboolean legacy, jlong low, jlong high,
                                    jboolean nether) {
    plugins::PluginNoiseParameters parameters{
        .baseAmplitude = baseAmplitude, .baseOctave = baseOctave, .octaveCount = octaveCount, .normalize = normalize};
    const jsize count = env->GetArrayLength(modifiers);
    parameters.amplitudeModifiers.resize(static_cast<std::size_t>(count));
    env->GetDoubleArrayRegion(modifiers, 0, count, parameters.amplitudeModifiers.data());
    const plugins::PluginNoiseSeed seed = noiseSeed(legacy, low, high, nether);
    return jni::onServer<jlong>(
        -1, [&] { return BukkitBridge::current().services().worldgen().createNoise(parameters, seed).value_or(-1); });
}

jdouble JNICALL nativeNoiseSample(JNIEnv*, jclass, jlong handle, jdouble x, jdouble y, jdouble z) {
    return jni::onServer<jdouble>(std::nan(""), [&] {
        return BukkitBridge::current().services().worldgen().sampleNoise(handle, x, y, z).value_or(std::nan(""));
    });
}

jdoubleArray JNICALL nativeNoiseHandleRange(JNIEnv* env, jclass, jlong handle) {
    const auto range = jni::onServer<std::optional<std::pair<double, double>>>(
        std::nullopt, [&] { return BukkitBridge::current().services().worldgen().handleRange(handle); });
    if (!range.has_value()) {
        return nullptr;
    }
    const jdouble values[] = {range->first, range->second};
    jdoubleArray array = env->NewDoubleArray(2);
    env->SetDoubleArrayRegion(array, 0, 2, values);
    return array;
}

void JNICALL nativeNoiseRelease(JNIEnv*, jclass, jlong handle) {
    jni::onServer<bool>(false, [&] {
        BukkitBridge::current().services().worldgen().releaseNoise(handle);
        return true;
    });
}

jint JNICALL nativeTerrainHeight(JNIEnv* env, jclass, jstring settings, jlong seed, jint x, jint z, jboolean floor) {
    const std::string name = toStd(env, settings);
    return jni::onServer<jint>(INT32_MIN, [&] {
        return BukkitBridge::current()
            .services()
            .worldgen()
            .baseHeight(name, seed, x, z, floor == JNI_TRUE)
            .value_or(INT32_MIN);
    });
}

jintArray JNICALL nativeTerrainColumn(JNIEnv* env, jclass, jstring settings, jlong seed, jint x, jint z) {
    const std::string name = toStd(env, settings);
    const std::vector<std::int32_t> states = jni::onServer<std::vector<std::int32_t>>(
        {}, [&] { return BukkitBridge::current().services().worldgen().baseColumn(name, seed, x, z); });
    jintArray array = env->NewIntArray(static_cast<jsize>(states.size()));
    env->SetIntArrayRegion(array, 0, static_cast<jsize>(states.size()), states.data());
    return array;
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

jlongArray JNICALL nativeClimate(JNIEnv* env, jclass, jstring world, jint quartX, jint quartY, jint quartZ) {
    const std::string name = toStd(env, world);
    const auto point = jni::onServer<std::optional<std::array<std::int64_t, 6>>>(std::nullopt, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        plugins::PluginWorldEnv* target = found == nullptr ? nullptr : found->env();
        return target == nullptr ? std::nullopt : target->climateAtQuart(quartX, quartY, quartZ);
    });
    if (!point.has_value()) {
        return nullptr;
    }
    jlongArray out = env->NewLongArray(6);
    if (out != nullptr) {
        std::array<jlong, 6> values{};
        std::copy(point->begin(), point->end(), values.begin());
        env->SetLongArrayRegion(out, 0, 6, values.data());
    }
    return out;
}

jboolean JNICALL nativeDragonAction(JNIEnv* env, jclass, jstring world, jstring action, jint value) {
    const std::string name = toStd(env, world);
    const std::string what = toStd(env, action);
    return flag(jni::onServer<bool>(false, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        return found != nullptr && found->dragonFightAction(what, value);
    }));
}

jboolean JNICALL nativeDragonBar(JNIEnv* env, jclass, jstring world, jstring action, jstring text, jint value) {
    const std::string name = toStd(env, world);
    const std::string what = toStd(env, action);
    const std::string content = text == nullptr ? std::string{} : toStd(env, text);
    return flag(jni::onServer<bool>(false, [&] {
        const auto found = BukkitBridge::current().services().worlds().byName(name);
        return found != nullptr && found->dragonFightBar(what, content, value);
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
        method("stopwatchIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeStopwatchIds)),
        method("stopwatch", "(Ljava/lang/String;Ljava/lang/String;)D", reinterpret_cast<void*>(&nativeStopwatch)),
        method("functionIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeFunctionIds)),
        method("functionTagIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeFunctionTagIds)),
        method("resourceRead", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeResourceRead)),
        method("resourcesUnder", "(Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeResourcesUnder)),
        method("resourcePack", "(Ljava/lang/String;)Ljava/lang/String;", reinterpret_cast<void*>(&nativeResourcePack)),
        method("structureRead", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeStructureRead)),
        method("structureStore", "(Ljava/lang/String;[BZ)Z", reinterpret_cast<void*>(&nativeStructureStore)),
        method("structureRemove", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeStructureRemove)),
        method("structureFile", "(Ljava/lang/String;)Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeStructureFile)),
        method("structureCapture", "(Ljava/lang/String;IIIIIIZ)[B", reinterpret_cast<void*>(&nativeStructureCapture)),
        method("structurePlace", "([BLjava/lang/String;IIIIIFJZI)Z", reinterpret_cast<void*>(&nativeStructurePlace)),
        method("templateScan", "([BIIIIIIILjava/lang/String;Z)[Ljava/lang/Object;",
               reinterpret_cast<void*>(&nativeTemplateScan)),
        method("datafix", "(Ljava/lang/String;[BII)[B", reinterpret_cast<void*>(&nativeDatafix)),
        method("datafixSupports", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeDatafixSupports)),
        method("terrainShape", "(Ljava/lang/String;)[I", reinterpret_cast<void*>(&nativeTerrainShape)),
        method("terrainDensity", "(Ljava/lang/String;JLjava/lang/String;III)D",
               reinterpret_cast<void*>(&nativeTerrainDensity)),
        method("terrainNoise", "(Ljava/lang/String;JLjava/lang/String;DDD)D",
               reinterpret_cast<void*>(&nativeTerrainNoise)),
        method("terrainNoiseRange", "(Ljava/lang/String;JLjava/lang/String;)[D",
               reinterpret_cast<void*>(&nativeTerrainNoiseRange)),
        method("noiseCreate", "(Ljava/lang/String;ZJJZ)J", reinterpret_cast<void*>(&nativeNoiseCreate)),
        method("noiseCreateWith", "(DIII[DZJJZ)J", reinterpret_cast<void*>(&nativeNoiseCreateWith)),
        method("noiseSample", "(JDDD)D", reinterpret_cast<void*>(&nativeNoiseSample)),
        method("noiseHandleRange", "(J)[D", reinterpret_cast<void*>(&nativeNoiseHandleRange)),
        method("noiseRelease", "(J)V", reinterpret_cast<void*>(&nativeNoiseRelease)),
        method("terrainHeight", "(Ljava/lang/String;JIIZ)I", reinterpret_cast<void*>(&nativeTerrainHeight)),
        method("terrainColumn", "(Ljava/lang/String;JII)[I", reinterpret_cast<void*>(&nativeTerrainColumn)),
        method("registryData", "(Ljava/lang/String;Ljava/lang/String;)[B",
               reinterpret_cast<void*>(&nativeRegistryData)),
        method("entityDefaults", "(Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeEntityDefaults)),
        method("humidity", "(Ljava/lang/String;III)D", reinterpret_cast<void*>(&nativeHumidity)),
        method("climate", "(Ljava/lang/String;III)[J", reinterpret_cast<void*>(&nativeClimate)),
        method("dragonBar", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)Z",
               reinterpret_cast<void*>(&nativeDragonBar)),
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
