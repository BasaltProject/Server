#include "JniSupport.hpp"

namespace basalt::server::bukkit {
namespace {
using plugins::PluginWorldRef;

[[nodiscard]] std::shared_ptr<PluginWorldRef> worldOf(JNIEnv* env, jstring name) {
    return BukkitBridge::current().services().worlds().byName(toStd(env, name));
}

[[nodiscard]] plugins::PluginStorage& storage() { return BukkitBridge::current().services().storage(); }

using Blob = std::optional<std::vector<std::uint8_t>>;

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

[[nodiscard]] plugins::PluginAccess& access() { return BukkitBridge::current().services().access(); }

template <typename Result, typename Body>
[[nodiscard]] Result accessCall(Result fallback, Body&& body) {
    return jni::onServer<Result>(std::move(fallback), std::forward<Body>(body));
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

[[nodiscard]] plugins::PluginScoreboards& boards() { return BukkitBridge::current().services().scoreboards(); }

template <typename Result, typename Body>
[[nodiscard]] Result boardCall(Result fallback, Body&& body) {
    return jni::onServer<Result>(std::move(fallback), std::forward<Body>(body));
}

[[nodiscard]] jboolean flag(bool value) { return value ? JNI_TRUE : JNI_FALSE; }

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

[[nodiscard]] plugins::PluginServerControl& control() { return BukkitBridge::current().services().serverControl(); }

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
    return jni::stringArrayOf(env, {facts->previouslyKilled ? "1" : "0", portal, facts->respawnStage,
                                    std::to_string(facts->gatewaysLeft), facts->dragonUuid,
                                    std::to_string(facts->barProgress), facts->barVisible ? "1" : "0",
                                    joined(facts->barPlayers, [](std::int32_t id) { return std::to_string(id); }),
                                    joined(facts->respawnCrystals, [](const std::string& id) { return id; }),
                                    joined(facts->healingCrystals, [](const std::string& id) { return id; })});
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

void registerMiscNatives(JNIEnv* env, jclass natives) {
    const auto method = [](const char* name, const char* signature, void* function) {
        return JNINativeMethod{const_cast<char*>(name), const_cast<char*>(signature), function};
    };
    const std::array methods{
        method("worldData", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativeWorldData)),
        method("setWorldData", "(Ljava/lang/String;[B)V", reinterpret_cast<void*>(&nativeSetWorldData)),
        method("chunkData", "(Ljava/lang/String;II)[B", reinterpret_cast<void*>(&nativeChunkData)),
        method("setChunkData", "(Ljava/lang/String;II[B)V", reinterpret_cast<void*>(&nativeSetChunkData)),
        method("blockEntityData", "(Ljava/lang/String;III)[B", reinterpret_cast<void*>(&nativeBlockEntityData)),
        method("setBlockEntityData", "(Ljava/lang/String;III[B)V", reinterpret_cast<void*>(&nativeSetBlockEntityData)),
        method("entityData", "(Ljava/lang/String;I)[B", reinterpret_cast<void*>(&nativeEntityData)),
        method("setEntityData", "(Ljava/lang/String;I[B)V", reinterpret_cast<void*>(&nativeSetEntityData)),
        method("playerData", "(Ljava/lang/String;)[B", reinterpret_cast<void*>(&nativePlayerData)),
        method("setPlayerData", "(Ljava/lang/String;[B)V", reinterpret_cast<void*>(&nativeSetPlayerData)),
        method("knownPlayer", "(Ljava/lang/String;Z)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeKnownPlayer)),
        method("knownPlayerNumbers", "(Ljava/lang/String;)[D", reinterpret_cast<void*>(&nativeKnownPlayerNumbers)),
        method("knownPlayers", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeKnownPlayers)),
        method("spawnPoint", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSpawnPoint)),
        method("isOperator", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeIsOperator)),
        method("setOperator", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativeSetOperator)),
        method("operators", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeOperators)),
        method("isWhitelisted", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeIsWhitelisted)),
        method("setWhitelisted", "(Ljava/lang/String;Ljava/lang/String;Z)V",
               reinterpret_cast<void*>(&nativeSetWhitelisted)),
        method("whitelisted", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeWhitelisted)),
        method("whitelistEnforced", "()Z", reinterpret_cast<void*>(&nativeWhitelistEnforced)),
        method("setWhitelistEnforced", "(Z)V", reinterpret_cast<void*>(&nativeSetWhitelistEnforced)),
        method("banOf", "(Ljava/lang/String;Z)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBanOf)),
        method("banTimes", "(Ljava/lang/String;Z)[J", reinterpret_cast<void*>(&nativeBanTimes)),
        method("bans", "(Z)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBans)),
        method("ban", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;JJZ)V",
               reinterpret_cast<void*>(&nativeBan)),
        method("pardon", "(Ljava/lang/String;Z)Z", reinterpret_cast<void*>(&nativePardon)),
        method("sbCreate", "()I", reinterpret_cast<void*>(&nativeSbCreate)),
        method("sbRelease", "(I)V", reinterpret_cast<void*>(&nativeSbRelease)),
        method("sbObjectives", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbObjectives)),
        method("sbObjective", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbObjective)),
        method("sbAddObjective", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbAddObjective)),
        method("sbUpdateObjective", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbUpdateObjective)),
        method("sbRemoveObjective", "(ILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbRemoveObjective)),
        method("sbDisplayed", "(II)Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbDisplayed)),
        method("sbDisplay", "(IILjava/lang/String;)V", reinterpret_cast<void*>(&nativeSbDisplay)),
        method("sbScore", "(ILjava/lang/String;Ljava/lang/String;)J", reinterpret_cast<void*>(&nativeSbScore)),
        method("sbSetScore", "(ILjava/lang/String;Ljava/lang/String;I)Z", reinterpret_cast<void*>(&nativeSbSetScore)),
        method("sbResetScore", "(ILjava/lang/String;Ljava/lang/String;)V",
               reinterpret_cast<void*>(&nativeSbResetScore)),
        method("sbEntries", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbEntries)),
        method("sbEntriesIn", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbEntriesIn)),
        method("sbScoresOf", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbScoresOf)),
        method("sbScoreDisplay", "(ILjava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeSbScoreDisplay)),
        method("sbSetScoreDisplay", "(ILjava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V",
               reinterpret_cast<void*>(&nativeSbSetScoreDisplay)),
        method("sbTriggerable", "(ILjava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeSbTriggerable)),
        method("sbSetTriggerable", "(ILjava/lang/String;Ljava/lang/String;Z)V",
               reinterpret_cast<void*>(&nativeSbSetTriggerable)),
        method("sbTeams", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbTeams)),
        method("sbTeam", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbTeam)),
        method("sbTeamOf", "(ILjava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeSbTeamOf)),
        method("sbAddTeam", "(ILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbAddTeam)),
        method("sbUpdateTeam", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbUpdateTeam)),
        method("sbRemoveTeam", "(ILjava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbRemoveTeam)),
        method("sbAddMember", "(ILjava/lang/String;Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeSbAddMember)),
        method("sbRemoveMember", "(ILjava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeSbRemoveMember)),
        method("sbShow", "(Ljava/lang/String;I)V", reinterpret_cast<void*>(&nativeSbShow)),
        method("sbShown", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&nativeSbShown)),
        method("bbCreate", "([Ljava/lang/String;)Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbCreate)),
        method("bbGet", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbGet)),
        method("bbKept", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbKept)),
        method("bbUpdate", "([Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeBbUpdate)),
        method("bbRemove", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeBbRemove)),
        method("bbAddPlayer", "(Ljava/lang/String;Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeBbAddPlayer)),
        method("bbRemovePlayer", "(Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeBbRemovePlayer)),
        method("bbClear", "(Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeBbClear)),
        method("bbPlayers", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeBbPlayers)),
        method("advIds", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeAdvIds)),
        method("advInfo", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeAdvInfo)),
        method("advLists", "(Ljava/lang/String;I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeAdvLists)),
        method("advObtained", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeAdvObtained)),
        method("advGrant", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeAdvGrant)),
        method("advRevoke", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z",
               reinterpret_cast<void*>(&nativeAdvRevoke)),
        method("tmRate", "()F", reinterpret_cast<void*>(&nativeTmRate)),
        method("tmSetRate", "(F)V", reinterpret_cast<void*>(&nativeTmSetRate)),
        method("tmFrozen", "()Z", reinterpret_cast<void*>(&nativeTmFrozen)),
        method("tmSetFrozen", "(Z)V", reinterpret_cast<void*>(&nativeTmSetFrozen)),
        method("tmStep", "(I)Z", reinterpret_cast<void*>(&nativeTmStep)),
        method("tmStepsOwed", "()I", reinterpret_cast<void*>(&nativeTmStepsOwed)),
        method("tmStopStepping", "()Z", reinterpret_cast<void*>(&nativeTmStopStepping)),
        method("tmSprint", "(I)Z", reinterpret_cast<void*>(&nativeTmSprint)),
        method("tmSprinting", "()Z", reinterpret_cast<void*>(&nativeTmSprinting)),
        method("tmStopSprinting", "()Z", reinterpret_cast<void*>(&nativeTmStopSprinting)),
        method("linkList", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeLinkList)),
        method("linkSet", "([Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeLinkSet)),
        method("dpList", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeDpList)),
        method("dpRefresh", "()V", reinterpret_cast<void*>(&nativeDpRefresh)),
        method("mapInfo", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeMapInfo)),
        method("mapCreate", "(Ljava/lang/String;III)I", reinterpret_cast<void*>(&nativeMapCreate)),
        method("mapUpdate", "(I[Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeMapUpdate)),
        method("mapColors", "(I)[B", reinterpret_cast<void*>(&nativeMapColors)),
        method("mapCarriers", "(I)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeMapCarriers)),
        method("mapShow", "(Ljava/lang/String;I[B[I[Ljava/lang/String;)V", reinterpret_cast<void*>(&nativeMapShow)),
        method("tmPace", "()[D", reinterpret_cast<void*>(&nativeTmPace)),
        method("resourcePack", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeResourcePack)),
        method("lootHas", "(Ljava/lang/String;)Z", reinterpret_cast<void*>(&nativeLootHas)),
        method("lootTables", "()[Ljava/lang/String;", reinterpret_cast<void*>(&nativeLootTables)),
        method("lootRoll", "(Ljava/lang/String;Ljava/lang/String;DDDFJLjava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeLootRoll)),
        method("locateStructure", "(Ljava/lang/String;Ljava/lang/String;III)[I",
               reinterpret_cast<void*>(&nativeLocateStructure)),
        method("locateBiome", "(Ljava/lang/String;[Ljava/lang/String;IIIII)[I",
               reinterpret_cast<void*>(&nativeLocateBiome)),
        method("shapeBoxes", "(I)[I", reinterpret_cast<void*>(&nativeShapeBoxes)),
        method("dragonFight", "(Ljava/lang/String;)[Ljava/lang/String;", reinterpret_cast<void*>(&nativeDragonFight)),
        method("setDragonPreviouslyKilled", "(Ljava/lang/String;Z)V",
               reinterpret_cast<void*>(&nativeSetDragonPreviouslyKilled)),
        method("playerProfile", "(Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativePlayerProfile)),
        method("select", "(Ljava/lang/String;Ljava/lang/String;)[Ljava/lang/String;",
               reinterpret_cast<void*>(&nativeSelect)),
        method("playerPermissions", "(Ljava/lang/String;[Ljava/lang/String;[Z)V",
               reinterpret_cast<void*>(&nativePlayerPermissions)),
    };
    if (env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size())) != 0) {
        BukkitBridge::current().log().error(
            [] { return std::string{"MiscNatives has a native the runtime does not declare"}; });
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
}
} // namespace basalt::server::bukkit
