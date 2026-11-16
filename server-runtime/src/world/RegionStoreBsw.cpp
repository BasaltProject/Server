#include <basalt/server/runtime/world/RegionStore.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace basalt::server::runtime::world {
namespace {

namespace fs = std::filesystem;
using metamorph::region::RegionFormat;

void removeInterruptedFiles(const fs::path& folder) {
    std::error_code error;
    for (const fs::directory_entry& entry : fs::directory_iterator(folder, error)) {
        const std::string name = entry.path().filename().string();
        if (name.starts_with("r.") && (name.ends_with(".bsw.new") || name.ends_with(".bsw.tmp"))) {
            fs::remove(entry.path(), error);
        }
    }
}

[[nodiscard]] fs::path asideName(const fs::path& root, RegionFormat from) {
    const std::string base = "region." + std::string{nameOf(from)} + "-original";
    fs::path candidate = root / base;
    for (int attempt = 2; fs::exists(candidate); ++attempt) {
        candidate = root / (base + "-" + std::to_string(attempt));
    }
    return candidate;
}

} // namespace

void RegionStore::chooseFormat(config::RegionFormatSetting setting) {
    const fs::path root{worldRoot_};
    const fs::path folder = root / "region";
    removeInterruptedFiles(folder);
    const RegionFormat found = metamorph::region::detectRegionFormat(folder.string());
    const bool bswFiles = metamorph::region::hasRegionFiles(folder.string(), RegionFormat::Bsw);
    if (found == RegionFormat::Mixed && bswFiles) {
        throw std::runtime_error("World regions in " + folder.string() +
                                 " mix .bsw files with another format; refusing to guess. Convert the folder "
                                 "with the metamorph tool or move the stray files away.");
    }
    using Setting = config::RegionFormatSetting;
    if (setting == Setting::Anvil) {
        if (found == RegionFormat::Mixed) {
            throw std::runtime_error("World regions in " + folder.string() +
                                     " mix several formats; convert them to one format before using anvil.");
        }
        if (found == RegionFormat::Bsw || found == RegionFormat::Linear) {
            convertRegions(found, RegionFormat::Anvil);
        }
        anvil_ = true;
        return;
    }
    if (setting == Setting::Linear) {
        if (found == RegionFormat::Bsw) {
            convertRegions(RegionFormat::Bsw, RegionFormat::Linear);
        }
        return;
    }
    if (setting == Setting::Bsw && (found == RegionFormat::Linear || found == RegionFormat::Anvil)) {
        convertRegions(found, RegionFormat::Bsw);
    } else if (setting == Setting::Bsw && found == RegionFormat::Mixed) {
        throw std::runtime_error("World regions in " + folder.string() +
                                 " mix linear and anvil files; convert them to one format before using bsw.");
    } else if (setting == Setting::Auto && found != RegionFormat::Bsw && found != RegionFormat::Empty) {
        return;
    }
    bsw_ = std::make_unique<BswChunkStore>(folder.string());
}

void RegionStore::convertRegions(RegionFormat from, RegionFormat to) {
    namespace region = metamorph::region;
    const fs::path root{worldRoot_};
    const fs::path source = root / "region";
    const fs::path temporary = root / "region.convert-tmp";
    std::error_code error;
    fs::remove_all(temporary, error);
    logger_.info([&] {
        return "Converting " + source.string() + " from " + std::string{nameOf(from)} + " to " +
               std::string{nameOf(to)} + ". The old files are kept next to it.";
    });
    region::ConvertOptions options;
    std::size_t lastTenth = 0;
    options.progress = [this, &lastTenth](std::size_t done, std::size_t total) {
        const std::size_t tenth = total == 0 ? 10 : done * 10 / total;
        if (tenth > lastTenth) {
            lastTenth = tenth;
            logger_.info([&] { return "Region conversion " + std::to_string(done) + "/" + std::to_string(total); });
        }
    };
    if (from == RegionFormat::Anvil) {
        options.prepare = [this](std::int32_t regionX, std::int32_t regionZ, metamorph::region::LinearRegion& held) {
            mergeVanillaEntities(held, regionX, regionZ);
        };
    }
    const region::ConvertReport report = region::convertRegionFolder(source.string(), temporary.string(), to, options);
    if (!report.ok()) {
        const fs::path failed = asideName(root, from).string() + "-failed";
        fs::rename(temporary, failed, error);
        throw std::runtime_error("Converting " + source.string() + " failed (" + std::to_string(report.failed) +
                                 " region(s) failed, " + std::to_string(report.mismatches) + " chunk(s) differ). " +
                                 report.error +
                                 " The original files are untouched; the "
                                 "partial output is in " +
                                 failed.string());
    }
    const fs::path aside = asideName(root, from);
    fs::rename(source, aside, error);
    if (error) {
        throw std::runtime_error("Cannot move " + source.string() + " aside: " + error.message());
    }
    fs::rename(temporary, source, error);
    if (error) {
        fs::rename(aside, source, error);
        throw std::runtime_error("Cannot put the converted regions in place of " + source.string());
    }
    logger_.info([&] {
        return "Converted " + std::to_string(report.regions) + " region(s) and " + std::to_string(report.chunks) +
               " chunk(s), every chunk verified. Original files kept in " + aside.string() + ".";
    });
}

} // namespace basalt::server::runtime::world
