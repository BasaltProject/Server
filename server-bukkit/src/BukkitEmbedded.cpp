#include <basalt/server/bukkit/BukkitEmbedded.hpp>

#ifdef __APPLE__
#define BASALT_EMBED_SECTION ".section __DATA,__const\n"
#define BASALT_EMBED_SYMBOL(name) "_" #name
#else
#define BASALT_EMBED_SECTION ".section .rodata\n"
#define BASALT_EMBED_SYMBOL(name) #name
#endif

#define BASALT_EMBED(name, file)                                                                                       \
    asm(BASALT_EMBED_SECTION ".balign 16\n.globl " BASALT_EMBED_SYMBOL(name) "\n" BASALT_EMBED_SYMBOL(                 \
        name) ":\n"                                                                                                    \
              ".incbin \"" BASALT_BUKKIT_EMBED_DIR "/" file                                                            \
              "\"\n.globl " BASALT_EMBED_SYMBOL(name##End) "\n" BASALT_EMBED_SYMBOL(name##End) ":\n.byte 0\n.text\n"); \
    extern "C" const std::uint8_t name[];                                                                              \
    extern "C" const std::uint8_t name##End[]

BASALT_EMBED(basaltEmbeddedLibjvm, "libjvm");
BASALT_EMBED(basaltEmbeddedBoot, "boot.jar");
BASALT_EMBED(basaltEmbeddedRuntimeJar, "runtime.jar");
BASALT_EMBED(basaltEmbeddedLibraries, "libs.txt");

namespace basalt::server::bukkit {
namespace {
std::span<const std::uint8_t> between(const std::uint8_t* begin, const std::uint8_t* end) {
    return {begin, static_cast<std::size_t>(end - begin)};
}
} // namespace

EmbeddedRuntime embeddedRuntime() {
    const std::span<const std::uint8_t> libraries = between(basaltEmbeddedLibraries, basaltEmbeddedLibrariesEnd);
    return {between(basaltEmbeddedLibjvm, basaltEmbeddedLibjvmEnd),
            between(basaltEmbeddedBoot, basaltEmbeddedBootEnd),
            between(basaltEmbeddedRuntimeJar, basaltEmbeddedRuntimeJarEnd),
            {reinterpret_cast<const char*>(libraries.data()), libraries.size()}};
}
} // namespace basalt::server::bukkit
