#pragma once

#include <basalt/wire/io/TcpSocket.hpp>

#include <expected>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#include <memory>
#include <string>

namespace basalt::server::remote {

class TlsContext {
public:
    [[nodiscard]] static std::expected<std::unique_ptr<TlsContext>, std::string> create(const std::string& certPath,
                                                                                        const std::string& keyPath);
    ~TlsContext();
    TlsContext(const TlsContext&) = delete;
    TlsContext& operator=(const TlsContext&) = delete;

    [[nodiscard]] mbedtls_ssl_config* config() noexcept { return &config_; }

private:
    TlsContext();

    mbedtls_ssl_config config_{};
    mbedtls_x509_crt certificate_{};
    mbedtls_pk_context key_{};
    mbedtls_ctr_drbg_context random_{};
    mbedtls_entropy_context entropy_{};
};

enum class IoState : std::uint8_t { Ok, WantRead, WantWrite, Closed };

/// A non-blocking byte pipe over a socket, optionally wrapped in TLS; it never moves once built.
class Transport {
public:
    Transport(wire::io::TcpConnection connection, TlsContext* tls);
    ~Transport();
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    [[nodiscard]] wire::io::TcpConnection& socket() noexcept { return connection_; }

    [[nodiscard]] bool handshaking() const noexcept { return tls_ && !established_; }

    [[nodiscard]] IoState handshake();

    /// Appends whatever can be read without blocking; Ok means it drained to would-block.
    [[nodiscard]] IoState readInto(std::string& into, std::size_t cap);

    /// Writes from the front of `pending` and erases what went out; WantWrite when some is left.
    [[nodiscard]] IoState writeFrom(std::string& pending);

private:
    static int sendCallback(void* context, const unsigned char* data, std::size_t length);
    static int receiveCallback(void* context, unsigned char* data, std::size_t length);

    wire::io::TcpConnection connection_;
    bool tls_ = false;
    bool established_ = false;
    std::size_t movingWrite_ = 0;
    mbedtls_ssl_context ssl_{};
};

} // namespace basalt::server::remote
