#pragma once

#include <jni.h>

#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace basalt::server::bukkit {
struct ShimOptions {
    std::string libjvm;
    std::string bootClasspath;
    std::vector<std::string> classpath;
};

class ShimHost {
    struct Key {};

public:
    ShimHost(Key, void* library, JavaVM* vm) : library_(library), vm_(vm) {}
    ShimHost(const ShimHost&) = delete;
    ShimHost& operator=(const ShimHost&) = delete;
    ShimHost(ShimHost&&) = delete;
    ShimHost& operator=(ShimHost&&) = delete;
    ~ShimHost();

    [[nodiscard]] static std::expected<std::unique_ptr<ShimHost>, std::string> create(const ShimOptions& options);

    [[nodiscard]] JavaVM* vm() const noexcept { return vm_; }

    /// The environment of the calling thread, attaching it to the VM on first use.
    [[nodiscard]] JNIEnv* env() const;

private:
    void* library_;
    JavaVM* vm_;
};
} // namespace basalt::server::bukkit
