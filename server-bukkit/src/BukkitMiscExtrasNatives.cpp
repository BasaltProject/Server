#include "BukkitMiscNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

[[nodiscard]] plugins::PluginBossBars& bars() { return BukkitBridge::current().services().bossBars(); }

[[nodiscard]] std::vector<std::string> barFields(const plugins::PluginBossBar& bar) {
    return {bar.id,
            bar.keyed ? "1" : "0",
            bar.title,
            bar.color,
            bar.style,
            bar.darkenSky ? "1" : "0",
            bar.bossMusic ? "1" : "0",
            bar.fog ? "1" : "0",
            std::to_string(bar.progress),
            bar.visible ? "1" : "0"};
}

[[nodiscard]] plugins::PluginBossBar barOf(const std::vector<std::string>& fields) {
    plugins::PluginBossBar bar;
    if (fields.size() < 10) {
        return bar;
    }
    bar.id = fields[0];
    bar.keyed = fields[1] == "1";
    bar.title = fields[2];
    bar.color = fields[3];
    bar.style = fields[4];
    bar.darkenSky = fields[5] == "1";
    bar.bossMusic = fields[6] == "1";
    bar.fog = fields[7] == "1";
    bar.progress = std::strtod(fields[8].c_str(), nullptr);
    bar.visible = fields[9] == "1";
    return bar;
}

jstring JNICALL nativeBbCreate(JNIEnv* env, jclass, jobjectArray fields) {
    const plugins::PluginBossBar bar = barOf(jni::stringsOf(env, fields));
    const auto made = jni::onServer<std::optional<std::string>>(std::nullopt, [&] { return bars().create(bar); });
    return made.has_value() ? toJava(env, *made) : nullptr;
}

jobjectArray JNICALL nativeBbGet(JNIEnv* env, jclass, jstring id) {
    const std::string text = toStd(env, id);
    const auto found =
        jni::onServer<std::optional<plugins::PluginBossBar>>(std::nullopt, [&] { return bars().get(text); });
    return found.has_value() ? jni::stringArrayOf(env, barFields(*found)) : nullptr;
}

jobjectArray JNICALL nativeBbKept(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] { return bars().kept(); }));
}

jboolean JNICALL nativeBbUpdate(JNIEnv* env, jclass, jobjectArray fields) {
    const plugins::PluginBossBar bar = barOf(jni::stringsOf(env, fields));
    return flag(jni::onServer<bool>(false, [&] { return bars().update(bar); }));
}

jboolean JNICALL nativeBbRemove(JNIEnv* env, jclass, jstring id) {
    const std::string text = toStd(env, id);
    return flag(jni::onServer<bool>(false, [&] { return bars().remove(text); }));
}

jboolean JNICALL nativeBbAddPlayer(JNIEnv* env, jclass, jstring id, jstring uuid) {
    const std::string text = toStd(env, id);
    const std::string who = toStd(env, uuid);
    return flag(jni::onServer<bool>(false, [&] { return bars().addPlayer(text, who); }));
}

jboolean JNICALL nativeBbRemovePlayer(JNIEnv* env, jclass, jstring id, jstring uuid) {
    const std::string text = toStd(env, id);
    const std::string who = toStd(env, uuid);
    return flag(jni::onServer<bool>(false, [&] { return bars().removePlayer(text, who); }));
}

void JNICALL nativeBbClear(JNIEnv* env, jclass, jstring id) {
    const std::string text = toStd(env, id);
    static_cast<void>(jni::onServer<bool>(false, [&] {
        bars().clearPlayers(text);
        return true;
    }));
}

jobjectArray JNICALL nativeBbPlayers(JNIEnv* env, jclass, jstring id) {
    const std::string text = toStd(env, id);
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [&] { return bars().players(text); }));
}

[[nodiscard]] plugins::PluginAdvancements& advancements() { return BukkitBridge::current().services().advancements(); }

jobjectArray JNICALL nativeAdvIds(JNIEnv* env, jclass) {
    return jni::stringArrayOf(env, jni::onServer<std::vector<std::string>>({}, [] { return advancements().ids(); }));
}

jobjectArray JNICALL nativeAdvInfo(JNIEnv* env, jclass, jstring id) {
    const std::string text = toStd(env, id);
    const auto found = jni::onServer<std::optional<plugins::PluginAdvancement>>(
        std::nullopt, [&] { return advancements().advancement(text); });
    if (!found.has_value()) {
        return nullptr;
    }
    jobjectArray array = jni::stringArrayOf(
        env, {found->id, found->parent.value_or(""), found->root, found->hasDisplay ? "1" : "0", found->title,
              found->description, found->iconItem, found->frame, found->background.value_or(""),
              found->toast ? "1" : "0", found->announce ? "1" : "0", found->hidden ? "1" : "0",
              std::to_string(found->x), std::to_string(found->y), std::to_string(found->experience)});
    if (!found->parent.has_value()) {
        env->SetObjectArrayElement(array, 1, nullptr);
    }
    if (!found->background.has_value()) {
        env->SetObjectArrayElement(array, 8, nullptr);
    }
    return array;
}

jobjectArray JNICALL nativeAdvLists(JNIEnv* env, jclass, jstring id, jint which) {
    const std::string text = toStd(env, id);
    const auto found = jni::onServer<std::optional<plugins::PluginAdvancement>>(
        std::nullopt, [&] { return advancements().advancement(text); });
    std::vector<std::string> out;
    if (found.has_value()) {
        if (which == 0) {
            out = found->criteria;
        } else if (which == 1) {
            out = found->children;
        } else {
            for (const std::vector<std::string>& group : found->requirements) {
                std::string joined;
                for (const std::string& name : group) {
                    joined += joined.empty() ? name : "\n" + name;
                }
                out.push_back(joined);
            }
        }
    }
    return jni::stringArrayOf(env, out);
}

jobjectArray JNICALL nativeAdvObtained(JNIEnv* env, jclass, jstring uuid, jstring id) {
    const std::string who = toStd(env, uuid);
    const std::string text = toStd(env, id);
    std::vector<std::string> flat;
    for (const auto& [criterion, when] :
         jni::onServer<std::map<std::string, std::int64_t>>({}, [&] { return advancements().obtained(who, text); })) {
        flat.push_back(criterion);
        flat.push_back(std::to_string(when));
    }
    return jni::stringArrayOf(env, flat);
}

jboolean JNICALL nativeAdvGrant(JNIEnv* env, jclass, jstring uuid, jstring id, jstring criterion) {
    const std::string who = toStd(env, uuid);
    const std::string text = toStd(env, id);
    const std::string name = toStd(env, criterion);
    return flag(jni::onServer<bool>(false, [&] { return advancements().grant(who, text, name); }));
}

jboolean JNICALL nativeAdvRevoke(JNIEnv* env, jclass, jstring uuid, jstring id, jstring criterion) {
    const std::string who = toStd(env, uuid);
    const std::string text = toStd(env, id);
    const std::string name = toStd(env, criterion);
    return flag(jni::onServer<bool>(false, [&] { return advancements().revoke(who, text, name); }));
}

jfloat JNICALL nativeTmRate(JNIEnv*, jclass) {
    return jni::onServer<jfloat>(20.0F, [] { return control().tickRate(); });
}

void JNICALL nativeTmSetRate(JNIEnv*, jclass, jfloat rate) {
    static_cast<void>(jni::onServer<bool>(false, [&] {
        control().setTickRate(rate);
        return true;
    }));
}

jboolean JNICALL nativeTmFrozen(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return control().frozen(); }));
}

void JNICALL nativeTmSetFrozen(JNIEnv*, jclass, jboolean frozen) {
    static_cast<void>(jni::onServer<bool>(false, [&] {
        control().setFrozen(frozen == JNI_TRUE);
        return true;
    }));
}

jboolean JNICALL nativeTmStep(JNIEnv*, jclass, jint ticks) {
    return flag(jni::onServer<bool>(false, [&] { return control().step(ticks); }));
}

jint JNICALL nativeTmStepsOwed(JNIEnv*, jclass) {
    return jni::onServer<jint>(0, [] { return control().stepsOwed(); });
}

jboolean JNICALL nativeTmStopStepping(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return control().stopStepping(); }));
}

jboolean JNICALL nativeTmSprint(JNIEnv*, jclass, jint ticks) {
    return flag(jni::onServer<bool>(false, [&] { return control().sprint(ticks); }));
}

jboolean JNICALL nativeTmSprinting(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return control().sprinting(); }));
}

jboolean JNICALL nativeTmStopSprinting(JNIEnv*, jclass) {
    return flag(jni::onServer<bool>(false, [] { return control().stopSprinting(); }));
}

jobjectArray JNICALL nativeLinkList(JNIEnv* env, jclass) {
    std::vector<std::string> flat;
    for (const plugins::PluginServerLink& link :
         jni::onServer<std::vector<plugins::PluginServerLink>>({}, [] { return control().links(); })) {
        flat.push_back(link.kind);
        flat.push_back(link.label);
        flat.push_back(link.url);
    }
    return jni::stringArrayOf(env, flat);
}

void JNICALL nativeLinkSet(JNIEnv* env, jclass, jobjectArray flat) {
    const std::vector<std::string> parts = jni::stringsOf(env, flat);
    std::vector<plugins::PluginServerLink> links;
    for (std::size_t index = 0; index + 2 < parts.size(); index += 3) {
        links.push_back(
            plugins::PluginServerLink{.kind = parts[index], .label = parts[index + 1], .url = parts[index + 2]});
    }
    static_cast<void>(jni::onServer<bool>(false, [&] {
        control().setLinks(links);
        return true;
    }));
}

jobjectArray JNICALL nativeDpList(JNIEnv* env, jclass) {
    std::vector<std::string> flat;
    for (const plugins::PluginDatapack& pack : jni::onServer<std::vector<plugins::PluginDatapack>>(
             {}, [] { return BukkitBridge::current().services().datapacks().packs(); })) {
        flat.push_back(pack.id);
        flat.push_back(pack.title);
        flat.push_back(pack.description);
        flat.push_back(pack.required ? "1" : "0");
        flat.push_back(pack.enabled ? "1" : "0");
        flat.push_back(pack.compatibility);
        flat.push_back(pack.source);
        std::string features;
        for (const std::string& feature : pack.features) {
            features += features.empty() ? feature : "\n" + feature;
        }
        flat.push_back(features);
    }
    return jni::stringArrayOf(env, flat);
}

void JNICALL nativeDpRefresh(JNIEnv*, jclass) {
    static_cast<void>(jni::onServer<bool>(false, [] {
        BukkitBridge::current().services().datapacks().refresh();
        return true;
    }));
}
} // namespace

void registerMiscNativesExtras(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("bbCreate", "([Ljava/lang/String;)Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbCreate)),
        nativeMethod("bbGet", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbGet)),
        nativeMethod("bbKept", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbKept)),
        nativeMethod("bbUpdate", "([Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeBbUpdate)),
        nativeMethod("bbRemove", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeBbRemove)),
        nativeMethod("bbAddPlayer", "(Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeBbAddPlayer)),
        nativeMethod("bbRemovePlayer", "(Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeBbRemovePlayer)),
        nativeMethod("bbClear", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeBbClear)),
        nativeMethod("bbPlayers", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbPlayers)),
        nativeMethod("advIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeAdvIds)),
        nativeMethod("advInfo", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeAdvInfo)),
        nativeMethod("advLists", "(Ljava/lang/String;I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeAdvLists)),
        nativeMethod("advObtained", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeAdvObtained)),
        nativeMethod("advGrant", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeAdvGrant)),
        nativeMethod("advRevoke", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeAdvRevoke)),
        nativeMethod("tmRate", "()F", reinterpret_cast<void*>(&nativeTmRate)),
        nativeMethod("tmSetRate", "(F)V", reinterpret_cast<void*>(&nativeTmSetRate)),
        nativeMethod("tmFrozen", "()Z", reinterpret_cast<void*>(&nativeTmFrozen)),
        nativeMethod("tmSetFrozen", "(Z)V", reinterpret_cast<void*>(&nativeTmSetFrozen)),
        nativeMethod("tmStep", "(I)Z", reinterpret_cast<void*>(&nativeTmStep)),
        nativeMethod("tmStepsOwed", "()I", reinterpret_cast<void*>(&nativeTmStepsOwed)),
        nativeMethod("tmStopStepping", "()Z", reinterpret_cast<void*>(&nativeTmStopStepping)),
        nativeMethod("tmSprint", "(I)Z", reinterpret_cast<void*>(&nativeTmSprint)),
        nativeMethod("tmSprinting", "()Z", reinterpret_cast<void*>(&nativeTmSprinting)),
        nativeMethod("tmStopSprinting", "()Z", reinterpret_cast<void*>(&nativeTmStopSprinting)),
        nativeMethod("linkList", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeLinkList)),
        nativeMethod("linkSet", "([Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeLinkSet)),
        nativeMethod("dpList", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeDpList)),
        nativeMethod("dpRefresh", "()V", reinterpret_cast<void*>(&nativeDpRefresh)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
