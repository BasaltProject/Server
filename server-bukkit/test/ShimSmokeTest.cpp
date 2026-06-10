#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/bukkit/BukkitEmbedded.hpp>
#include <basalt/server/bukkit/ShimHost.hpp>

#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <span>
#include <string>

namespace fs = std::filesystem;
using namespace basalt::server::bukkit;

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
    (*host)->beginShutdown();
    (*host)->destroy();
}
