#include "BridgeInternal.hpp"

#include <basalt/server/plugins/PluginPackets.hpp>

#include <array>
#include <map>
#include <memory>
#include <mutex>

namespace basalt::server::bukkit {
namespace {
using plugins::PacketEndpoint;
using plugins::PluginPacket;

constexpr std::int32_t kLastPriority = 1000;
constexpr std::int32_t kLoginHello = 1;
constexpr std::int32_t kLoginCompression = 3;

// Hands every packet to the Java channel layer while a plugin hooked netty; one instance for the process.
class PacketTap {
public:
    explicit PacketTap(BukkitBridge& bridge) : bridge_(&bridge) {}

    void start(JNIEnv* env) {
        const std::lock_guard<std::mutex> guard{mutex_};
        if (started_) {
            return;
        }
        jclass local = env->FindClass("org/basaltmc/bukkit/net/ChannelBridge");
        if (local == nullptr) {
            env->ExceptionClear();
            return;
        }
        bridge_class_ = static_cast<jclass>(env->NewGlobalRef(local));
        inbound_ = env->GetStaticMethodID(bridge_class_, "inbound", "(JII[BLjava/lang/String;I)[B");
        outbound_ = env->GetStaticMethodID(bridge_class_, "outbound", "(JII[B)Z");
        closed_ = env->GetStaticMethodID(bridge_class_, "closed", "(J)V");
        if (inbound_ == nullptr || outbound_ == nullptr || closed_ == nullptr) {
            env->ExceptionClear();
            return;
        }
        listener_ = bridge_->plugins().packets().addNative(
            plugins::PacketScope{}, kLastPriority, false, [this](PluginPacket& packet) { onPacket(packet); });
        closer_ = bridge_->plugins().packets().addCloseListener([this](std::uint64_t connection) { onClosed(connection); });
        started_ = true;
    }

    void stop() {
        const std::lock_guard<std::mutex> guard{mutex_};
        listener_.remove();
        closer_.remove();
        started_ = false;
    }

    [[nodiscard]] std::shared_ptr<PacketEndpoint> endpoint(std::uint64_t connection) {
        const std::lock_guard<std::mutex> guard{mutex_};
        const auto found = endpoints_.find(connection);
        return found == endpoints_.end() ? nullptr : found->second.lock();
    }

    void rememberEndpoint(const std::shared_ptr<PacketEndpoint>& endpoint) {
        const std::lock_guard<std::mutex> guard{mutex_};
        endpoints_.insert_or_assign(endpoint->connectionId(), endpoint);
    }

private:
    void onPacket(PluginPacket& packet) {
        if (packet.endpoint == nullptr) {
            return;
        }
        // These two change how the next frames are written, so they cannot wait in a queue behind the plugin.
        if (packet.address.phase == plugins::PacketPhase::Login && !packet.serverbound() &&
            (packet.address.id == kLoginHello || packet.address.id == kLoginCompression)) {
            return;
        }
        remember(packet);
        JNIEnv* env = bridge_->env();
        if (env == nullptr) {
            return;
        }
        const jsize size = static_cast<jsize>(packet.bytes.size());
        jbyteArray body = env->NewByteArray(size);
        env->SetByteArrayRegion(body, 0, size, reinterpret_cast<const jbyte*>(packet.bytes.data()));
        const auto phase = static_cast<jint>(packet.address.phase);
        if (packet.serverbound()) {
            const bool first = packet.address.phase == plugins::PacketPhase::Handshake;
            jstring address = first ? toJava(env, packet.endpoint->address()) : nullptr;
            const jint port = first ? static_cast<jint>(packet.endpoint->port()) : 0;
            jobject result = env->CallStaticObjectMethod(bridge_class_, inbound_, static_cast<jlong>(packet.connection),
                                                         phase, static_cast<jint>(packet.address.id), body, address,
                                                         port);
            if (env->ExceptionCheck() == JNI_TRUE) {
                bridge_->reportFailure(env, "a serverbound packet listener");
            } else if (result == nullptr) {
                packet.cancelled = true;
            } else if (env->IsSameObject(result, body) == JNI_FALSE) {
                const auto fresh = static_cast<jbyteArray>(result);
                std::vector<std::uint8_t> bytes(static_cast<std::size_t>(env->GetArrayLength(fresh)));
                env->GetByteArrayRegion(fresh, 0, static_cast<jsize>(bytes.size()),
                                        reinterpret_cast<jbyte*>(bytes.data()));
                packet.replacement = std::move(bytes);
            }
            if (result != nullptr) {
                env->DeleteLocalRef(result);
            }
            if (address != nullptr) {
                env->DeleteLocalRef(address);
            }
        } else {
            const jboolean taken = env->CallStaticBooleanMethod(bridge_class_, outbound_,
                                                                static_cast<jlong>(packet.connection), phase,
                                                                static_cast<jint>(packet.address.id), body);
            if (env->ExceptionCheck() == JNI_TRUE) {
                bridge_->reportFailure(env, "a clientbound packet listener");
            } else if (taken == JNI_TRUE) {
                packet.cancelled = true;
            }
        }
        env->DeleteLocalRef(body);
    }

    void remember(const PluginPacket& packet) {
        const std::lock_guard<std::mutex> guard{mutex_};
        endpoints_.try_emplace(packet.connection, packet.endpoint);
    }

    void onClosed(std::uint64_t connection) {
        {
            const std::lock_guard<std::mutex> guard{mutex_};
            endpoints_.erase(connection);
        }
        JNIEnv* env = bridge_->env();
        if (env == nullptr) {
            return;
        }
        env->CallStaticVoidMethod(bridge_class_, closed_, static_cast<jlong>(connection));
        if (env->ExceptionCheck() == JNI_TRUE) {
            bridge_->reportFailure(env, "closing a channel");
        }
    }

    BukkitBridge* bridge_;
    std::mutex mutex_;
    bool started_ = false;
    jclass bridge_class_ = nullptr;
    jmethodID inbound_ = nullptr;
    jmethodID outbound_ = nullptr;
    jmethodID closed_ = nullptr;
    plugins::PluginHandle listener_;
    plugins::PluginHandle closer_;
    std::map<std::uint64_t, std::weak_ptr<PacketEndpoint>> endpoints_;
};

[[nodiscard]] PacketTap& tap() {
    static PacketTap kTap{BukkitBridge::current()};
    return kTap;
}

void JNICALL nativeTapSwitch(JNIEnv* env, jclass, jboolean on) {
    if (on == JNI_TRUE) {
        tap().start(env);
    } else {
        tap().stop();
    }
}

[[nodiscard]] std::vector<std::uint8_t> bytesOf(JNIEnv* env, jbyteArray array) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(env->GetArrayLength(array)));
    env->GetByteArrayRegion(array, 0, static_cast<jsize>(bytes.size()), reinterpret_cast<jbyte*>(bytes.data()));
    return bytes;
}

void JNICALL nativeSend(JNIEnv* env, jclass, jlong connection, jint id, jbyteArray body) {
    const std::shared_ptr<PacketEndpoint> endpoint = tap().endpoint(static_cast<std::uint64_t>(connection));
    if (endpoint != nullptr) {
        endpoint->sendClientbound(id, bytesOf(env, body), false);
    }
}

void JNICALL nativeReceive(JNIEnv* env, jclass, jlong connection, jint id, jbyteArray body) {
    const std::shared_ptr<PacketEndpoint> endpoint = tap().endpoint(static_cast<std::uint64_t>(connection));
    if (endpoint != nullptr) {
        endpoint->receiveServerbound(id, bytesOf(env, body), false);
    }
}

void JNICALL nativeClose(JNIEnv*, jclass, jlong connection) {
    const std::shared_ptr<PacketEndpoint> endpoint = tap().endpoint(static_cast<std::uint64_t>(connection));
    if (endpoint != nullptr) {
        endpoint->close();
    }
}

jlong JNICALL nativeConnectionOf(JNIEnv* env, jclass, jstring uuid) {
    const auto player = BukkitBridge::current().services().players().byUuid(toStd(env, uuid));
    if (player == nullptr) {
        return 0;
    }
    const std::shared_ptr<PacketEndpoint> endpoint = BukkitBridge::current().services().packets().endpointOf(*player);
    if (endpoint == nullptr) {
        return 0;
    }
    // A plugin may first request a channel after login, before any packet has
    // traversed the lazy tap. Retain the real endpoint before opening its peer,
    // so even the first outbound write has somewhere to go.
    tap().rememberEndpoint(endpoint);
    jclass bridge = env->FindClass("org/basaltmc/bukkit/net/ChannelBridge");
    if (bridge != nullptr) {
        jmethodID open = env->GetStaticMethodID(bridge, "ensureOpen", "(JLjava/lang/String;I)V");
        if (open != nullptr) {
            jstring address = toJava(env, endpoint->address());
            env->CallStaticVoidMethod(bridge, open, static_cast<jlong>(endpoint->connectionId()), address,
                                      static_cast<jint>(endpoint->port()));
            env->DeleteLocalRef(address);
        }
        env->DeleteLocalRef(bridge);
    }
    return static_cast<jlong>(endpoint->connectionId());
}
} // namespace

void registerPacketNatives(JNIEnv* env, jclass natives) {
    const std::array methods{
        JNINativeMethod{const_cast<char*>("packetTap"), const_cast<char*>("(Z)V"),
                        reinterpret_cast<void*>(&nativeTapSwitch)},
        JNINativeMethod{const_cast<char*>("packetSend"), const_cast<char*>("(JI[B)V"),
                        reinterpret_cast<void*>(&nativeSend)},
        JNINativeMethod{const_cast<char*>("packetReceive"), const_cast<char*>("(JI[B)V"),
                        reinterpret_cast<void*>(&nativeReceive)},
        JNINativeMethod{const_cast<char*>("packetClose"), const_cast<char*>("(J)V"),
                        reinterpret_cast<void*>(&nativeClose)},
        JNINativeMethod{const_cast<char*>("packetConnectionOf"), const_cast<char*>("(Ljava/lang/String;)J"),
                        reinterpret_cast<void*>(&nativeConnectionOf)},
    };
    env->RegisterNatives(natives, methods.data(), static_cast<jint>(methods.size()));
}
} // namespace basalt::server::bukkit
