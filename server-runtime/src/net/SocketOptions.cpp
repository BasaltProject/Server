#include <basalt/server/runtime/net/SocketOptions.hpp>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#endif

namespace basalt::server::runtime::net {
namespace {

#if defined(_WIN32)
using Handle = SOCKET;
using Value = const char*;
using Out = char*;
#else
using Handle = int;
using Value = const void*;
using Out = void*;
#endif

} // namespace

bool setTcpNoDelay(std::int64_t handle, bool enabled) noexcept {
    const int flag = enabled ? 1 : 0;
    return handle >= 0 && ::setsockopt(static_cast<Handle>(handle), IPPROTO_TCP, TCP_NODELAY,
                                       reinterpret_cast<Value>(&flag), sizeof(flag)) == 0;
}

bool tcpNoDelay(std::int64_t handle) noexcept {
    int flag = 0;
#if defined(_WIN32)
    int size = sizeof(flag);
#else
    socklen_t size = sizeof(flag);
#endif
    return handle >= 0 &&
           ::getsockopt(static_cast<Handle>(handle), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<Out>(&flag), &size) ==
               0 &&
           flag != 0;
}

} // namespace basalt::server::runtime::net
