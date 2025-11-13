#include <basalt/server/assets/EulaFile.hpp>
#include <basalt/wire/io/FileIO.hpp>

#include <chrono>
#include <format>

namespace basalt::server::assets::eulaFile {
namespace {

[[nodiscard]] std::string defaultFileContent(bool accepted) {
    // The timestamp is a comment, so its shape is ours rather than vanilla's: what it records is
    // when the operator's server first asked, which is what an audit of the file wants to know.
    return std::format("# By changing the setting below to true you are indicating your agreement to our EULA\n"
                       "# https://www.minecraft.net/en-us/eula\n"
                       "# {0:%FT%TZ}\n"
                       "eula = {1}",
                       std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()),
                       accepted ? "true" : "false");
}

} // namespace

bool isAccepted(std::string_view content) {
    std::size_t start = 0;
    while (start <= content.size()) {
        const std::size_t end = std::min(content.find('\n', start), content.size());
        std::string_view line = content.substr(start, end - start);
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t' || line.front() == '\r')) {
            line.remove_prefix(1);
        }
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.remove_suffix(1);
        }
        if (!line.starts_with('#')) {
            std::string squashed;
            for (const char character : line) {
                if (character != ' ') {
                    squashed += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                }
            }
            if (squashed == "eula=true") {
                return true;
            }
        }
        if (end == content.size()) {
            break;
        }
        start = end + 1;
    }
    return false;
}

Outcome check(bool acceptFlag, const wire::common::Logger& logger) {
    if (acceptFlag) {
        logger.info([&] { return "EULA accepted via --accept-eula."; });
        return Outcome::Accepted;
    }

    if (const std::optional<std::string> content = wire::io::fileIO::readText(std::string{kPath});
        content.has_value()) {
        if (isAccepted(*content)) {
            return Outcome::Accepted;
        }
        logger.warn([&] {
            return std::format("You must accept the Minecraft EULA (eula = true in {}) to run this server.", kPath);
        });
        return Outcome::Rejected;
    }

    const std::optional<std::string> legacy = wire::io::fileIO::readText(std::string{kLegacyPath});
    if (legacy.has_value() && isAccepted(*legacy)) {
        wire::io::fileIO::writeText(std::string{kPath}, defaultFileContent(true));
        logger.info([&] {
            return std::format("Found an accepted {0}, recording it in {1}. {0} is no longer read.", kLegacyPath,
                               kPath);
        });
        return Outcome::Accepted;
    }

    wire::io::fileIO::writeText(std::string{kPath}, defaultFileContent(false));
    logger.warn([&] {
        return std::format("{} created. You must accept the Minecraft EULA (eula = true) to run this server.", kPath);
    });
    return Outcome::JustCreated;
}

} // namespace basalt::server::assets::eulaFile
