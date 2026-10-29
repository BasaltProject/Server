#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/bukkit/BukkitEmbedded.hpp>
#include <basalt/server/bukkit/ShimHost.hpp>

#include <doctest.h>

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
    for (const char* name : {"publicReflection", "featureKeys", "sqlite", "registryTags", "loggingBackend"}) {
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
    env->DeleteLocalRef(regression);
    CHECK(forwardedLogCount.load() > 0);
    CHECK(tagReadCount.load() == 2);
    (*host)->beginShutdown();
    (*host)->destroy();
}
