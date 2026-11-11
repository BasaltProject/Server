#include <basalt/server/plugins/PluginFiles.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace basalt::server::plugins::fileService {
namespace {
namespace fs = std::filesystem;

[[nodiscard]] fs::path pathOf(std::string_view text) { return fs::path{std::string{text}}; }

[[nodiscard]] fs::path sibling(const fs::path& target) {
    static std::atomic<std::uint64_t> counter{0};
    return target.parent_path() / (target.filename().string() + ".tmp" + std::to_string(counter.fetch_add(1)));
}

[[nodiscard]] std::int64_t millisOf(fs::file_time_type time) {
    const auto sinceEpoch = time - fs::file_time_type::clock::now() + std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::time_point_cast<std::chrono::milliseconds>(sinceEpoch).time_since_epoch())
        .count();
}
} // namespace

std::optional<Stat> stat(std::string_view path) {
    std::error_code error;
    const fs::path target = pathOf(path);
    const fs::file_status status = fs::symlink_status(target, error);
    if (error || status.type() == fs::file_type::not_found) {
        return std::nullopt;
    }
    Stat result;
    if (status.type() == fs::file_type::regular) {
        result.kind = Kind::File;
        result.size = static_cast<std::int64_t>(fs::file_size(target, error));
    } else if (status.type() == fs::file_type::directory) {
        result.kind = Kind::Directory;
    } else {
        result.kind = Kind::Other;
    }
    result.modifiedMillis = millisOf(fs::last_write_time(target, error));
    return result;
}

std::vector<std::string> walk(std::string_view directory) {
    std::vector<std::string> entries;
    std::error_code error;
    const fs::path root = pathOf(directory);
    for (fs::recursive_directory_iterator at{root, fs::directory_options::none, error}, end; !error && at != end;
         at.increment(error)) {
        const fs::file_status status = at->symlink_status(error);
        const char tag = status.type() == fs::file_type::regular     ? 'f'
                         : status.type() == fs::file_type::directory ? 'd'
                                                                     : 'o';
        entries.push_back(std::string{tag} + ':' + fs::relative(at->path(), root, error).generic_string());
    }
    std::sort(entries.begin(), entries.end(),
              [](const std::string& a, const std::string& b) { return a.compare(2, std::string::npos, b, 2) < 0; });
    return entries;
}

std::optional<std::vector<std::string>> list(std::string_view directory) {
    std::error_code error;
    std::vector<std::string> names;
    for (fs::directory_iterator at{pathOf(directory), error}, end; !error && at != end; at.increment(error)) {
        names.push_back(at->path().filename().string());
    }
    if (error) {
        return std::nullopt;
    }
    std::sort(names.begin(), names.end());
    return names;
}

std::optional<std::vector<std::uint8_t>> read(std::string_view path) {
    std::ifstream in{pathOf(path), std::ios::binary};
    if (!in) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

bool write(std::string_view path, const std::vector<std::uint8_t>& bytes) {
    const fs::path target = pathOf(path);
    const fs::path temporary = sibling(target);
    {
        std::ofstream out{temporary, std::ios::binary | std::ios::trunc};
        if (!out) {
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out.good()) {
            return false;
        }
    }
    std::error_code error;
    fs::rename(temporary, target, error);
    if (error) {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

bool makeDirectories(std::string_view path) {
    std::error_code error;
    fs::create_directories(pathOf(path), error);
    return !error;
}

bool copyFile(std::string_view from, std::string_view to) {
    std::error_code error;
    fs::copy_file(pathOf(from), pathOf(to), fs::copy_options::none, error);
    if (error) {
        return false;
    }
    fs::permissions(pathOf(to), fs::status(pathOf(from), error).permissions(), error);
    fs::last_write_time(pathOf(to), fs::last_write_time(pathOf(from), error), error);
    return true;
}

bool hardLink(std::string_view from, std::string_view to) {
    std::error_code error;
    fs::create_hard_link(pathOf(from), pathOf(to), error);
    return !error;
}

bool move(std::string_view from, std::string_view to, bool atomic) {
    std::error_code error;
    fs::rename(pathOf(from), pathOf(to), error);
    if (!error) {
        return true;
    }
    if (atomic) {
        return false;
    }
    if (!copyFile(from, to)) {
        return false;
    }
    fs::remove(pathOf(from), error);
    return !error;
}

bool remove(std::string_view path) {
    std::error_code error;
    return fs::remove(pathOf(path), error) && !error;
}

bool removeTree(std::string_view path) {
    std::error_code error;
    fs::remove_all(pathOf(path), error);
    return !error;
}

Capabilities capabilities(std::string_view directory) {
    static std::atomic<std::uint64_t> counter{0};
    const fs::path root = pathOf(directory);
    const std::string tag = ".capability-" + std::to_string(counter.fetch_add(1)) + '-' +
                            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path probe = root / tag;
    const fs::path moved = root / (tag + ".moved");
    const fs::path link = root / (tag + ".link");
    Capabilities result;
    std::error_code error;
    {
        std::ofstream out{probe, std::ios::binary};
        if (!out) {
            return result;
        }
    }
    fs::path current = probe;
    fs::rename(probe, moved, error);
    if (!error) {
        result.atomicMove = true;
        current = moved;
    }
    fs::create_hard_link(current, link, error);
    result.hardLinks = !error;
    fs::remove(link, error);
    fs::remove(current, error);
    return result;
}

} // namespace basalt::server::plugins::fileService
