#pragma once

#include <jni.h>

namespace basalt::server::bukkit {
// Attached native threads do not return through a Java native-method frame.
// Bound every host callback explicitly: deleting individual references alone
// does not reclaim the interpreter's local-reference stack slots.
class JniLocalFrame {
public:
    explicit JniLocalFrame(JNIEnv* env, jint capacity = 64)
        : env_(env), valid_(env != nullptr && env->PushLocalFrame(capacity) == JNI_OK) {}
    ~JniLocalFrame() { if (valid_) env_->PopLocalFrame(nullptr); }
    JniLocalFrame(const JniLocalFrame&) = delete;
    JniLocalFrame& operator=(const JniLocalFrame&) = delete;
    explicit operator bool() const noexcept { return valid_; }
private:
    JNIEnv* env_;
    bool valid_;
};
} // namespace basalt::server::bukkit
