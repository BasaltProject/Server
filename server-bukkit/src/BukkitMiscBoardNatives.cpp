#include "BukkitMiscNativesShared.hpp"

namespace basalt::server::bukkit {
namespace {
using namespace shared;

[[nodiscard]] plugins::PluginScoreboards& boards() { return BukkitBridge::current().services().scoreboards(); }

template <typename Result, typename Body>
[[nodiscard]] Result boardCall(Result fallback, Body&& body) {
    return jni::onServer<Result>(std::move(fallback), std::forward<Body>(body));
}

[[nodiscard]] std::vector<std::string> objectiveFields(const plugins::PluginObjective& objective) {
    return {objective.name,
            objective.criterion,
            objective.displayName,
            objective.hearts ? "1" : "0",
            objective.autoUpdate ? "1" : "0",
            objective.numberFormat,
            objective.numberFormatText,
            objective.readOnly ? "1" : "0"};
}

[[nodiscard]] plugins::PluginObjective objectiveOf(const std::vector<std::string>& fields) {
    plugins::PluginObjective objective;
    if (fields.size() < 7) {
        return objective;
    }
    objective.name = fields[0];
    objective.criterion = fields[1];
    objective.displayName = fields[2];
    objective.hearts = fields[3] == "1";
    objective.autoUpdate = fields[4] == "1";
    objective.numberFormat = fields[5];
    objective.numberFormatText = fields[6];
    return objective;
}

[[nodiscard]] std::vector<std::string> teamFields(const plugins::PluginTeam& team) {
    std::vector<std::string> fields{team.name,
                                    team.displayName,
                                    team.prefix,
                                    team.suffix,
                                    team.color,
                                    team.friendlyFire ? "1" : "0",
                                    team.seeFriendlyInvisibles ? "1" : "0",
                                    team.nameTagVisibility,
                                    team.deathMessageVisibility,
                                    team.collisionRule};
    fields.insert(fields.end(), team.members.begin(), team.members.end());
    return fields;
}

[[nodiscard]] plugins::PluginTeam teamOf(const std::vector<std::string>& fields) {
    plugins::PluginTeam team;
    if (fields.size() < 10) {
        return team;
    }
    team.name = fields[0];
    team.displayName = fields[1];
    team.prefix = fields[2];
    team.suffix = fields[3];
    team.color = fields[4];
    team.friendlyFire = fields[5] == "1";
    team.seeFriendlyInvisibles = fields[6] == "1";
    team.nameTagVisibility = fields[7];
    team.deathMessageVisibility = fields[8];
    team.collisionRule = fields[9];
    team.members.assign(fields.begin() + 10, fields.end());
    return team;
}

jint JNICALL nativeSbCreate(JNIEnv*, jclass) {
    return boardCall<jint>(-1, [] { return boards().create(); });
}

void JNICALL nativeSbRelease(JNIEnv*, jclass, jint board) {
    static_cast<void>(boardCall<bool>(false, [&] {
        boards().release(board);
        return true;
    }));
}

jobjectArray JNICALL nativeSbObjectives(JNIEnv* env, jclass, jint board) {
    std::vector<std::string> names;
    for (const plugins::PluginObjective& objective :
         boardCall<std::vector<plugins::PluginObjective>>({}, [&] { return boards().objectives(board); })) {
        names.push_back(objective.name);
    }
    return jni::stringArrayOf(env, names);
}

jobjectArray JNICALL nativeSbObjective(JNIEnv* env, jclass, jint board, jstring name) {
    const std::string text = toStd(env, name);
    const auto found = boardCall<std::optional<plugins::PluginObjective>>(
        std::nullopt, [&] { return boards().objective(board, text); });
    return found.has_value() ? jni::stringArrayOf(env, objectiveFields(*found)) : nullptr;
}

jboolean JNICALL nativeSbAddObjective(JNIEnv* env, jclass, jint board, jobjectArray fields) {
    const plugins::PluginObjective objective = objectiveOf(jni::stringsOf(env, fields));
    return flag(boardCall<bool>(false, [&] { return boards().addObjective(board, objective); }));
}

jboolean JNICALL nativeSbUpdateObjective(JNIEnv* env, jclass, jint board, jobjectArray fields) {
    const plugins::PluginObjective objective = objectiveOf(jni::stringsOf(env, fields));
    return flag(boardCall<bool>(false, [&] { return boards().updateObjective(board, objective); }));
}

jboolean JNICALL nativeSbRemoveObjective(JNIEnv* env, jclass, jint board, jstring name) {
    const std::string text = toStd(env, name);
    return flag(boardCall<bool>(false, [&] { return boards().removeObjective(board, text); }));
}

jstring JNICALL nativeSbDisplayed(JNIEnv* env, jclass, jint board, jint slot) {
    const auto found =
        boardCall<std::optional<std::string>>(std::nullopt, [&] { return boards().displayed(board, slot); });
    return found.has_value() ? toJava(env, *found) : nullptr;
}

void JNICALL nativeSbDisplay(JNIEnv* env, jclass, jint board, jint slot, jstring objective) {
    const std::optional<std::string> name = objective == nullptr ? std::nullopt : std::optional{toStd(env, objective)};
    static_cast<void>(boardCall<bool>(false, [&] {
        boards().display(board, slot, name);
        return true;
    }));
}

jlong JNICALL nativeSbScore(JNIEnv* env, jclass, jint board, jstring entry, jstring objective) {
    const std::string who = toStd(env, entry);
    const std::string what = toStd(env, objective);
    const auto found =
        boardCall<std::optional<std::int32_t>>(std::nullopt, [&] { return boards().score(board, who, what); });
    return found.has_value() ? static_cast<jlong>(*found) : INT64_MIN;
}

jboolean JNICALL nativeSbSetScore(JNIEnv* env, jclass, jint board, jstring entry, jstring objective, jint value) {
    const std::string who = toStd(env, entry);
    const std::string what = toStd(env, objective);
    return flag(boardCall<bool>(false, [&] { return boards().setScore(board, who, what, value); }));
}

void JNICALL nativeSbResetScore(JNIEnv* env, jclass, jint board, jstring entry, jstring objective) {
    const std::string who = toStd(env, entry);
    const std::optional<std::string> what = objective == nullptr ? std::nullopt : std::optional{toStd(env, objective)};
    static_cast<void>(boardCall<bool>(false, [&] {
        boards().resetScore(board, who, what);
        return true;
    }));
}

jobjectArray JNICALL nativeSbEntries(JNIEnv* env, jclass, jint board) {
    return jni::stringArrayOf(env, boardCall<std::vector<std::string>>({}, [&] { return boards().entries(board); }));
}

jobjectArray JNICALL nativeSbEntriesIn(JNIEnv* env, jclass, jint board, jstring objective) {
    const std::string what = toStd(env, objective);
    return jni::stringArrayOf(env,
                              boardCall<std::vector<std::string>>({}, [&] { return boards().entriesIn(board, what); }));
}

jobjectArray JNICALL nativeSbScoresOf(JNIEnv* env, jclass, jint board, jstring entry) {
    const std::string who = toStd(env, entry);
    std::vector<std::string> flat;
    for (const auto& [objective, value] : boardCall<std::vector<std::pair<std::string, std::int32_t>>>(
             {}, [&] { return boards().scoresOf(board, who); })) {
        flat.push_back(objective);
        flat.push_back(std::to_string(value));
    }
    return jni::stringArrayOf(env, flat);
}

jobjectArray JNICALL nativeSbScoreDisplay(JNIEnv* env, jclass, jint board, jstring entry, jstring objective) {
    const std::string who = toStd(env, entry);
    const std::string what = toStd(env, objective);
    const plugins::PluginScoreDisplay shown =
        boardCall<plugins::PluginScoreDisplay>({}, [&] { return boards().scoreDisplay(board, who, what); });
    jobjectArray array = jni::stringArrayOf(env, {"", shown.numberFormat, shown.numberFormatText});
    if (shown.customName.has_value()) {
        jstring custom = toJava(env, *shown.customName);
        env->SetObjectArrayElement(array, 0, custom);
        env->DeleteLocalRef(custom);
    } else {
        env->SetObjectArrayElement(array, 0, nullptr);
    }
    return array;
}

void JNICALL nativeSbSetScoreDisplay(JNIEnv* env, jclass, jint board, jstring entry, jstring objective,
                                     jobjectArray fields) {
    const std::string who = toStd(env, entry);
    const std::string what = toStd(env, objective);
    plugins::PluginScoreDisplay shown;
    if (fields != nullptr && env->GetArrayLength(fields) >= 3) {
        auto custom = static_cast<jstring>(env->GetObjectArrayElement(fields, 0));
        if (custom != nullptr) {
            shown.customName = toStd(env, custom);
            env->DeleteLocalRef(custom);
        }
        const std::vector<std::string> rest = jni::stringsOf(env, fields);
        shown.numberFormat = rest[1];
        shown.numberFormatText = rest[2];
    }
    static_cast<void>(boardCall<bool>(false, [&] {
        boards().setScoreDisplay(board, who, what, shown);
        return true;
    }));
}

jboolean JNICALL nativeSbTriggerable(JNIEnv* env, jclass, jint board, jstring entry, jstring objective) {
    const std::string who = toStd(env, entry);
    const std::string what = toStd(env, objective);
    return flag(boardCall<bool>(false, [&] { return boards().triggerable(board, who, what); }));
}

void JNICALL nativeSbSetTriggerable(JNIEnv* env, jclass, jint board, jstring entry, jstring objective, jboolean value) {
    const std::string who = toStd(env, entry);
    const std::string what = toStd(env, objective);
    static_cast<void>(boardCall<bool>(false, [&] {
        boards().setTriggerable(board, who, what, value == JNI_TRUE);
        return true;
    }));
}

jobjectArray JNICALL nativeSbTeams(JNIEnv* env, jclass, jint board) {
    std::vector<std::string> names;
    for (const plugins::PluginTeam& team :
         boardCall<std::vector<plugins::PluginTeam>>({}, [&] { return boards().teams(board); })) {
        names.push_back(team.name);
    }
    return jni::stringArrayOf(env, names);
}

jobjectArray JNICALL nativeSbTeam(JNIEnv* env, jclass, jint board, jstring name) {
    const std::string text = toStd(env, name);
    const auto found =
        boardCall<std::optional<plugins::PluginTeam>>(std::nullopt, [&] { return boards().team(board, text); });
    return found.has_value() ? jni::stringArrayOf(env, teamFields(*found)) : nullptr;
}

jobjectArray JNICALL nativeSbTeamOf(JNIEnv* env, jclass, jint board, jstring entry) {
    const std::string text = toStd(env, entry);
    const auto found =
        boardCall<std::optional<plugins::PluginTeam>>(std::nullopt, [&] { return boards().teamOf(board, text); });
    return found.has_value() ? jni::stringArrayOf(env, teamFields(*found)) : nullptr;
}

jboolean JNICALL nativeSbAddTeam(JNIEnv* env, jclass, jint board, jstring name) {
    const std::string text = toStd(env, name);
    return flag(boardCall<bool>(false, [&] { return boards().addTeam(board, text); }));
}

jboolean JNICALL nativeSbUpdateTeam(JNIEnv* env, jclass, jint board, jobjectArray fields) {
    const plugins::PluginTeam team = teamOf(jni::stringsOf(env, fields));
    return flag(boardCall<bool>(false, [&] { return boards().updateTeam(board, team); }));
}

jboolean JNICALL nativeSbRemoveTeam(JNIEnv* env, jclass, jint board, jstring name) {
    const std::string text = toStd(env, name);
    return flag(boardCall<bool>(false, [&] { return boards().removeTeam(board, text); }));
}

jboolean JNICALL nativeSbAddMember(JNIEnv* env, jclass, jint board, jstring team, jstring entry) {
    const std::string which = toStd(env, team);
    const std::string who = toStd(env, entry);
    return flag(boardCall<bool>(false, [&] { return boards().addMember(board, which, who); }));
}

jboolean JNICALL nativeSbRemoveMember(JNIEnv* env, jclass, jint board, jstring team, jstring entry) {
    const std::string which = toStd(env, team);
    const std::string who = toStd(env, entry);
    return flag(boardCall<bool>(false, [&] { return boards().removeMember(board, which, who); }));
}

void JNICALL nativeSbShow(JNIEnv* env, jclass, jstring uuid, jint board) {
    const std::string id = toStd(env, uuid);
    static_cast<void>(boardCall<bool>(false, [&] {
        boards().show(id, board);
        return true;
    }));
}

jint JNICALL nativeSbShown(JNIEnv* env, jclass, jstring uuid) {
    const std::string id = toStd(env, uuid);
    return boardCall<jint>(0, [&] { return boards().shown(id); });
}
} // namespace

void registerMiscNativesBoard(JNIEnv* env, jclass natives) {
    const std::array methods{
        nativeMethod("sbCreate", "()I", reinterpret_cast<void*>(&nativeSbCreate)),
        nativeMethod("sbRelease", "(I)V", reinterpret_cast<void*>(&nativeSbRelease)),
        nativeMethod("sbObjectives", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbObjectives)),
        nativeMethod("sbObjective", "(ILjava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeSbObjective)),
        nativeMethod("sbAddObjective", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbAddObjective)),
        nativeMethod("sbUpdateObjective", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbUpdateObjective)),
        nativeMethod("sbRemoveObjective", "(ILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbRemoveObjective)),
        nativeMethod("sbDisplayed", "(II)Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbDisplayed)),
        nativeMethod("sbDisplay", "(IILjava/lang/String;)V", reinterpret_cast<void*>(&nativeSbDisplay)),
        nativeMethod("sbScore", "(ILjava/lang/String;Ljava/lang/String;)J", reinterpret_cast<void*>(&nativeSbScore)),
        nativeMethod("sbSetScore", "(ILjava/lang/String;Ljava/lang/String;I)Z",
                     reinterpret_cast<void*>(&nativeSbSetScore)),
        nativeMethod("sbResetScore", "(ILjava/lang/String;Ljava/lang/String;)V",
                     reinterpret_cast<void*>(&nativeSbResetScore)),
        nativeMethod("sbEntries", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbEntries)),
        nativeMethod("sbEntriesIn", "(ILjava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeSbEntriesIn)),
        nativeMethod("sbScoresOf", "(ILjava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeSbScoresOf)),
        nativeMethod("sbScoreDisplay", "(ILjava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
                     reinterpret_cast<void*>(&nativeSbScoreDisplay)),
        nativeMethod("sbSetScoreDisplay", "(ILjava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V",
                     reinterpret_cast<void*>(&nativeSbSetScoreDisplay)),
        nativeMethod("sbTriggerable", "(ILjava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeSbTriggerable)),
        nativeMethod("sbSetTriggerable", "(ILjava/lang/String;Ljava/lang/String;Z)V",
                     reinterpret_cast<void*>(&nativeSbSetTriggerable)),
        nativeMethod("sbTeams", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbTeams)),
        nativeMethod("sbTeam", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbTeam)),
        nativeMethod("sbTeamOf", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbTeamOf)),
        nativeMethod("sbAddTeam", "(ILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbAddTeam)),
        nativeMethod("sbUpdateTeam", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbUpdateTeam)),
        nativeMethod("sbRemoveTeam", "(ILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbRemoveTeam)),
        nativeMethod("sbAddMember", "(ILjava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeSbAddMember)),
        nativeMethod("sbRemoveMember", "(ILjava/lang/String;Ljava/lang/String;)Z",
                     reinterpret_cast<void*>(&nativeSbRemoveMember)),
        nativeMethod("sbShow", "(Ljava/lang/String;I)V", reinterpret_cast<void*>(&nativeSbShow)),
        nativeMethod("sbShown", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeSbShown)),
    };
    registerMethods(env, natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
