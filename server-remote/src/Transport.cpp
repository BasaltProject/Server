#include "Transport.hpp"

#include <algorithm>
#include <array>
#include <basalt_mbedtls_threads.h>

namespace basalt::server::remote {
namespace {

[[nodiscard]] std::string describe(int code) {
    std::array<char, 128> written{};
    mbedtls_strerror(code, written.data(), written.size());
    return std::string{written.data()};
}

} // namespace

TlsContext::TlsContext() {
    basalt_mbedtls_threading_init();
    mbedtls_ssl_config_init(&config_);
    mbedtls_x509_crt_init(&certificate_);
    mbedtls_pk_init(&key_);
    mbedtls_ctr_drbg_init(&random_);
    mbedtls_entropy_init(&entropy_);
}

TlsContext::~TlsContext() {
    mbedtls_entropy_free(&entropy_);
    mbedtls_ctr_drbg_free(&random_);
    mbedtls_pk_free(&key_);
    mbedtls_x509_crt_free(&certificate_);
    mbedtls_ssl_config_free(&config_);
}

std::expected<std::unique_ptr<TlsContext>, std::string> TlsContext::create(const std::string& certPath,
                                                                           const std::string& keyPath) {
    std::unique_ptr<TlsContext> context{new TlsContext()};
    int code = mbedtls_ctr_drbg_seed(&context->random_, mbedtls_entropy_func, &context->entropy_,
                                     reinterpret_cast<const unsigned char*>("basalt-remote"), 13);
    if (code != 0) {
        return std::unexpected("could not seed the TLS random generator: " + describe(code));
    }
    code = mbedtls_x509_crt_parse_file(&context->certificate_, certPath.c_str());
    if (code != 0) {
        return std::unexpected("could not read the certificate " + certPath + ": " + describe(code));
    }
    code =
        mbedtls_pk_parse_keyfile(&context->key_, keyPath.c_str(), nullptr, mbedtls_ctr_drbg_random, &context->random_);
    if (code != 0) {
        return std::unexpected("could not read the private key " + keyPath + ": " + describe(code));
    }
    code = mbedtls_ssl_config_defaults(&context->config_, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
                                       MBEDTLS_SSL_PRESET_DEFAULT);
    if (code != 0) {
        return std::unexpected("could not set up TLS: " + describe(code));
    }
    mbedtls_ssl_conf_rng(&context->config_, mbedtls_ctr_drbg_random, &context->random_);
    mbedtls_ssl_conf_min_tls_version(&context->config_, MBEDTLS_SSL_VERSION_TLS1_2);
    code = mbedtls_ssl_conf_own_cert(&context->config_, &context->certificate_, &context->key_);
    if (code != 0) {
        return std::unexpected("the certificate and key do not match: " + describe(code));
    }
    return context;
}

Transport::Transport(wire::io::TcpConnection connection, TlsContext* tls)
    : connection_(std::move(connection)), tls_(tls != nullptr) {
    mbedtls_ssl_init(&ssl_);
    if (tls_) {
        if (mbedtls_ssl_setup(&ssl_, tls->config()) != 0) {
            tls_ = false;
            connection_.close();
            return;
        }
        mbedtls_ssl_set_bio(&ssl_, this, &Transport::sendCallback, &Transport::receiveCallback, nullptr);
    }
}

Transport::~Transport() { mbedtls_ssl_free(&ssl_); }

int Transport::sendCallback(void* context, const unsigned char* data, std::size_t length) {
    auto* self = static_cast<Transport*>(context);
    const auto sent = self->connection_.writeSome({data, length});
    if (sent.has_value()) {
        return static_cast<int>(*sent);
    }
    return sent.error().kind == wire::io::SocketError::Kind::WouldBlock ? MBEDTLS_ERR_SSL_WANT_WRITE
                                                                        : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

int Transport::receiveCallback(void* context, unsigned char* data, std::size_t length) {
    auto* self = static_cast<Transport*>(context);
    const auto received = self->connection_.read({data, length});
    if (received.has_value()) {
        return static_cast<int>(*received);
    }
    return received.error().kind == wire::io::SocketError::Kind::WouldBlock ? MBEDTLS_ERR_SSL_WANT_READ
                                                                            : MBEDTLS_ERR_SSL_INTERNAL_ERROR;
}

IoState Transport::handshake() {
    if (!tls_ || established_) {
        return IoState::Ok;
    }
    const int code = mbedtls_ssl_handshake(&ssl_);
    if (code == 0) {
        established_ = true;
        return IoState::Ok;
    }
    if (code == MBEDTLS_ERR_SSL_WANT_READ) {
        return IoState::WantRead;
    }
    return code == MBEDTLS_ERR_SSL_WANT_WRITE ? IoState::WantWrite : IoState::Closed;
}

IoState Transport::readInto(std::string& into, std::size_t cap) {
    std::array<std::uint8_t, 4096> chunk{};
    while (into.size() < cap) {
        if (tls_) {
            const int count = mbedtls_ssl_read(&ssl_, chunk.data(), chunk.size());
            if (count > 0) {
                into.append(reinterpret_cast<const char*>(chunk.data()), static_cast<std::size_t>(count));
                continue;
            }
            if (count == MBEDTLS_ERR_SSL_WANT_READ || count == MBEDTLS_ERR_SSL_WANT_WRITE) {
                return IoState::Ok;
            }
            return IoState::Closed;
        }
        const auto count = connection_.read(chunk);
        if (!count.has_value()) {
            return count.error().kind == wire::io::SocketError::Kind::WouldBlock ? IoState::Ok : IoState::Closed;
        }
        if (*count == 0) {
            return IoState::Closed;
        }
        into.append(reinterpret_cast<const char*>(chunk.data()), *count);
    }
    return IoState::Ok;
}

IoState Transport::writeFrom(std::string& pending) {
    while (!pending.empty()) {
        if (tls_) {
            const std::size_t length = movingWrite_ != 0 ? movingWrite_ : std::min<std::size_t>(pending.size(), 16384);
            const int count = mbedtls_ssl_write(&ssl_, reinterpret_cast<const unsigned char*>(pending.data()), length);
            if (count > 0) {
                movingWrite_ = 0;
                pending.erase(0, static_cast<std::size_t>(count));
                continue;
            }
            if (count == MBEDTLS_ERR_SSL_WANT_WRITE || count == MBEDTLS_ERR_SSL_WANT_READ) {
                movingWrite_ = length;
                return IoState::WantWrite;
            }
            return IoState::Closed;
        }
        const auto count =
            connection_.writeSome({reinterpret_cast<const std::uint8_t*>(pending.data()), pending.size()});
        if (!count.has_value()) {
            return count.error().kind == wire::io::SocketError::Kind::WouldBlock ? IoState::WantWrite : IoState::Closed;
        }
        pending.erase(0, *count);
    }
    return IoState::Ok;
}

} // namespace basalt::server::remote
