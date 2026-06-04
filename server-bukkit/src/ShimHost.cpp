#include <basalt/server/bukkit/ShimHost.hpp>

#include <array>
#include <dlfcn.h>

namespace basalt::server::bukkit {
namespace {
using CreateVm = jint (*)(JavaVM**, void**, void*);

[[nodiscard]] std::string joined(const std::vector<std::string>& paths) {
    std::string out;
    for (const std::string& path : paths) {
        if (!out.empty()) {
            out += ':';
        }
        out += path;
    }
    return out;
}
} // namespace

ShimHost::~ShimHost() = default;

std::expected<std::unique_ptr<ShimHost>, std::string> ShimHost::create(const ShimOptions& options) {
    void* library = dlopen(options.libjvm.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (library == nullptr) {
        return std::unexpected(std::string{"cannot load "} + options.libjvm + ": " + dlerror());
    }
    void* symbol = dlsym(library, "JNI_CreateJavaVM");
    if (symbol == nullptr) {
        return std::unexpected(options.libjvm + " has no JNI_CreateJavaVM");
    }
    CreateVm create = nullptr;
    static_assert(sizeof(create) == sizeof(symbol));
    __builtin_memcpy(&create, &symbol, sizeof(create));

    const std::string classpath = "-Djava.class.path=" + joined(options.classpath);
    const std::string boot = "-Xbootclasspath:" + options.bootClasspath;
    const std::string bootstrap = "-Dshim.bootstrap=" + options.libjvm;
    std::array<JavaVMOption, 3> vmOptions{};
    vmOptions[0].optionString = const_cast<char*>(classpath.c_str());
    vmOptions[1].optionString = const_cast<char*>(boot.c_str());
    vmOptions[2].optionString = const_cast<char*>(bootstrap.c_str());
    JavaVMInitArgs args{};
    args.version = JNI_VERSION_1_8;
    args.nOptions = static_cast<jint>(vmOptions.size());
    args.options = vmOptions.data();

    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    if (create(&vm, reinterpret_cast<void**>(&env), &args) != JNI_OK) {
        return std::unexpected(std::string{"JNI_CreateJavaVM failed"});
    }
    return std::make_unique<ShimHost>(Key{}, library, vm);
}

JNIEnv* ShimHost::env() const {
    JNIEnv* env = nullptr;
    if (vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) == JNI_OK) {
        return env;
    }
    if (vm_->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
        return nullptr;
    }
    return env;
}
} // namespace basalt::server::bukkit
