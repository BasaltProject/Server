#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/bukkit/BukkitEmbedded.hpp>
#include <basalt/server/bukkit/ShimHost.hpp>

#include <doctest.h>
#include <basalt/wire/protocol/ProtocolBuffer.hpp>
#include <basalt/wire/protocol/play/UiPackets.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>

namespace fs = std::filesystem;
using namespace basalt::server::bukkit;

namespace {
std::atomic<int> forwardedLogCount{0};
std::atomic<int> tagReadCount{0};

void JNICALL receiveLog(JNIEnv*, jclass, jint, jstring, jstring) { ++forwardedLogCount; }

jobjectArray JNICALL readTags(JNIEnv* env, jclass, jstring) {
    ++tagReadCount;
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray result = env->NewObjectArray(4, stringClass, nullptr);
    const char* values[] = {"minecraft:first", "minecraft:stone|minecraft:dirt|minecraft:stone|minecraft:missing",
                            "minecraft:second", "minecraft:dirt|minecraft:stone|minecraft:missing"};
    for (jsize i = 0; i < 4; ++i) {
        jstring value = env->NewStringUTF(values[i]);
        env->SetObjectArrayElement(result, i, value);
        env->DeleteLocalRef(value);
    }
    env->DeleteLocalRef(stringClass);
    return result;
}
} // namespace

TEST_CASE("the embedded Shim boots and resolves the Bukkit runtime over JNI") {
    const EmbeddedRuntime embedded = embeddedRuntime();
    REQUIRE(embedded.complete());
    const fs::path scratch{BASALT_TEST_SCRATCH_DIR};
    fs::create_directories(scratch);
    const auto unpack = [&scratch](const char* name, std::span<const std::uint8_t> bytes) {
        const fs::path path = scratch / name;
        std::ofstream stream{path, std::ios::binary | std::ios::trunc};
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        stream.close();
        REQUIRE(stream.good());
        return fs::absolute(path).string();
    };
    ShimOptions options{.libjvm = unpack("libjvm", embedded.libjvm),
                        .bootClasspath = unpack("classpath.jar", embedded.bootClasspath),
                        .classpath = {unpack("basalt-bukkit.jar", embedded.runtimeJar)}};
    for (const fs::directory_entry& jar : fs::directory_iterator(fs::path{BASALT_TEST_RUNTIME_DIR} / "lib")) {
        if (jar.path().extension() == ".jar") {
            options.classpath.push_back(fs::absolute(jar.path()).string());
        }
    }
    options.classpath.push_back(BASALT_COMPAT_TEST_CLASSES);
    auto host = ShimHost::create(options);
    REQUIRE_MESSAGE(host.has_value(), (host.has_value() ? "" : host.error()));
    JNIEnv* env = (*host)->env();
    REQUIRE(env != nullptr);
    for (const char* name : {"org/basaltmc/bukkit/Natives", "org/basaltmc/bukkit/BukkitRuntime"}) {
        CAPTURE(name);
        jclass type = env->FindClass(name);
        const bool failed = env->ExceptionCheck() == JNI_TRUE;
        if (failed) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        CHECK_FALSE(failed);
        CHECK(type != nullptr);
        if (type != nullptr) {
            env->DeleteLocalRef(type);
        }
    }
    jclass natives = env->FindClass("org/basaltmc/bukkit/Natives");
    REQUIRE(natives != nullptr);
    JNINativeMethod logMethod{const_cast<char*>("log"), const_cast<char*>("(ILjava/lang/String;Ljava/lang/String;)V"),
                              reinterpret_cast<void*>(&receiveLog)};
    REQUIRE(env->RegisterNatives(natives, &logMethod, 1) == JNI_OK);
    env->DeleteLocalRef(natives);
    jclass worldNatives = env->FindClass("org/basaltmc/bukkit/WorldNatives");
    REQUIRE(worldNatives != nullptr);
    JNINativeMethod tagsMethod{const_cast<char*>("tags"), const_cast<char*>("(Ljava/lang/String;)[Ljava/lang/String;"),
                               reinterpret_cast<void*>(&readTags)};
    REQUIRE(env->RegisterNatives(worldNatives, &tagsMethod, 1) == JNI_OK);
    env->DeleteLocalRef(worldNatives);
    jclass regression = env->FindClass("compatibility/RuntimeCompatibilityTest");
    REQUIRE(regression != nullptr);
    for (const char* name : {"publicReflection", "featureKeys", "sqlite", "registryTags", "failureText", "loggingBackend"}) {
        INFO(std::string{name});
        jmethodID method = env->GetStaticMethodID(regression, name, "()V");
        REQUIRE(method != nullptr);
        env->CallStaticVoidMethod(regression, method);
        const bool failed = env->ExceptionCheck() == JNI_TRUE;
        if (failed) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }
        CHECK_FALSE(failed);
    }
    // Decode Java's real outbound frames with the native protocol implementation.
    jmethodID framesMethod = env->GetStaticMethodID(regression, "scoreboardPackets", "()[[B");
    REQUIRE(framesMethod != nullptr);
    auto frames = static_cast<jobjectArray>(env->CallStaticObjectMethod(regression, framesMethod));
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    REQUIRE(frames != nullptr);
    REQUIRE(env->GetArrayLength(frames) == 12);
    namespace play = basalt::wire::protocol::play;
    using Text = basalt::wire::text::Component::Text;
    for (jsize i = 0; i < 12; ++i) {
        INFO(i);
        auto frame = static_cast<jbyteArray>(env->GetObjectArrayElement(frames, i));
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(env->GetArrayLength(frame)));
        env->GetByteArrayRegion(frame, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<jbyte*>(bytes.data()));
        auto buffer = basalt::wire::io::ByteBuffer::wrap(std::move(bytes));
        const auto id = basalt::wire::protocol::varInt::read(buffer);
        if (i == 0 || i == 10) {
            CHECK(id == play::SetObjectivePacket::kId);
            auto packet = play::SetObjectivePacket::read(buffer); REQUIRE(packet.has_value());
            CHECK(packet->objectiveName == "compat"); CHECK(packet->method == (i == 0 ? 0 : 1));
            if (i == 0) { REQUIRE(packet->displayName.has_value()); CHECK(packet->displayName->style().bold == true);
                REQUIRE(packet->numberFormat.has_value()); CHECK(packet->numberFormat->typeId() == 0); }
        } else if (i == 1 || i == 11) {
            CHECK(id == play::SetDisplayObjectivePacket::kId);
            auto packet = play::SetDisplayObjectivePacket::read(buffer); REQUIRE(packet.has_value());
            CHECK(packet->slot == play::DisplaySlot::Sidebar); CHECK(packet->objectiveName == (i == 1 ? "compat" : ""));
        } else if (i == 2) {
            CHECK(id == play::SetScorePacket::kId);
            auto packet = play::SetScorePacket::read(buffer); REQUIRE(packet.has_value());
            CHECK(packet->score == -7); CHECK(packet->owner == "entry"); CHECK(packet->objectiveName == "compat");
            REQUIRE(packet->display.has_value()); CHECK(packet->display->style().bold == true);
            REQUIRE(packet->numberFormat.has_value()); CHECK(packet->numberFormat->typeId() == 2);
        } else if (i == 8 || i == 9) {
            CHECK(id == play::ResetScorePacket::kId);
            auto packet = play::ResetScorePacket::read(buffer); REQUIRE(packet.has_value());
            CHECK(packet->owner == "entry"); CHECK(packet->objectiveName.has_value() == (i == 8));
        } else {
            CHECK(id == play::SetPlayerTeamPacket::kId);
            auto packet = play::SetPlayerTeamPacket::read(buffer); REQUIRE(packet.has_value());
            CHECK(packet->teamName == "team");
            if (i == 3 || i == 4) {
                REQUIRE(packet->parameters.has_value()); const auto& parameters = *packet->parameters;
                CHECK(parameters.nameTagVisibility == play::NameTagVisibility::Never);
                CHECK(parameters.collisionRule == play::CollisionRule::PushOtherTeams);
                CHECK(parameters.color == play::TeamColor::Red); CHECK_FALSE(parameters.friendlyFire);
                CHECK(parameters.seeFriendlyInvisibles);
                REQUIRE(parameters.prefix.as<Text>() != nullptr);
                CHECK(parameters.prefix.as<Text>()->content == (i == 3 ? "prefix " : "changed"));
            } else { CHECK_FALSE(packet->parameters.has_value()); }
            CHECK(packet->method == (i == 3 ? 0 : i == 4 ? 2 : i == 5 ? 3 : i == 6 ? 4 : 1));
            CHECK(packet->members.size() == (i == 3 || i == 5 || i == 6 ? 1 : 0));
        }
        CHECK_FALSE(buffer.failed()); CHECK(buffer.readableBytes() == 0);
        env->DeleteLocalRef(frame);
    }
    env->DeleteLocalRef(frames);
    jmethodID metadataMethod = env->GetStaticMethodID(regression, "metadataPacket", "()[B");
    REQUIRE(metadataMethod != nullptr);
    auto metadata = static_cast<jbyteArray>(env->CallStaticObjectMethod(regression, metadataMethod));
    if (env->ExceptionCheck()) { env->ExceptionDescribe(); env->ExceptionClear(); }
    REQUIRE(metadata != nullptr);
    std::vector<std::uint8_t> metadataBytes(static_cast<std::size_t>(env->GetArrayLength(metadata)));
    env->GetByteArrayRegion(metadata,0,static_cast<jsize>(metadataBytes.size()),reinterpret_cast<jbyte*>(metadataBytes.data()));
    auto metadataBuffer = basalt::wire::io::ByteBuffer::wrap(std::move(metadataBytes));
    CHECK(basalt::wire::protocol::varInt::read(metadataBuffer) == play::SetEntityDataPacket::kId);
    auto decodedMetadata = play::SetEntityDataPacket::read(metadataBuffer);
    REQUIRE(decodedMetadata.has_value());
    CHECK(decodedMetadata->entityId == 42);
    REQUIRE(decodedMetadata->entries.size() == 3);
    CHECK(std::get<play::SetEntityDataPacket::ByteValue>(decodedMetadata->entries[0].value).value == 75);
    CHECK(std::get<play::SetEntityDataPacket::IntValue>(decodedMetadata->entries[1].value).value == 123);
    CHECK(std::get<play::SetEntityDataPacket::TextValue>(decodedMetadata->entries[2].value).value.as<Text>()->content == "name");
    CHECK_FALSE(metadataBuffer.failed()); CHECK(metadataBuffer.readableBytes() == 0);
    env->DeleteLocalRef(metadata);
    env->DeleteLocalRef(regression);
    CHECK(forwardedLogCount.load() > 0);
    CHECK(tagReadCount.load() == 2);
    (*host)->beginShutdown();
    (*host)->destroy();
}
