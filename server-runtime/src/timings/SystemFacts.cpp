#include <basalt/server/runtime/timings/SystemFacts.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#if defined(__linux__)
#include <sys/utsname.h>
#endif

namespace basalt::server::runtime::timings {
namespace {

#if defined(__APPLE__)
[[nodiscard]] std::optional<std::string> sysctlString(const char* name) {
    std::size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) {
        return std::nullopt;
    }
    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) {
        return std::nullopt;
    }
    value.resize(size > 0 ? size - 1 : 0);
    return value;
}

[[nodiscard]] std::optional<std::int64_t> sysctlInt(const char* name) {
    std::int64_t value = 0;
    std::size_t size = sizeof value;
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0) {
        return std::nullopt;
    }
    return value;
}
#endif

#if defined(__linux__)
[[nodiscard]] std::optional<std::int64_t> meminfoKb(const std::string& text, const std::string& key) {
    const std::size_t at = text.find(key + ":");
    if (at == std::string::npos) {
        return std::nullopt;
    }
    std::istringstream line{text.substr(at + key.size() + 1)};
    std::int64_t kilobytes = 0;
    line >> kilobytes;
    return kilobytes * 1024;
}
#endif

} // namespace

SystemFacts readSystemFacts(const std::string& diskPath) {
    SystemFacts facts;
    facts.cpuThreads = static_cast<std::int32_t>(std::thread::hardware_concurrency());
#if defined(__aarch64__) || defined(_M_ARM64)
    facts.arch = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    facts.arch = "x64";
#else
    facts.arch = "unknown";
#endif
#if defined(__APPLE__)
    facts.osName = "macOS";
    facts.osVersion = sysctlString("kern.osproductversion").value_or("");
    facts.cpuModel = sysctlString("machdep.cpu.brand_string").value_or("");
    facts.memoryTotal = sysctlInt("hw.memsize");
    vm_size_t pageSize = 0;
    host_page_size(mach_host_self(), &pageSize);
    vm_statistics64_data_t stats{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&stats), &count) ==
        KERN_SUCCESS) {
        const std::int64_t pages =
            static_cast<std::int64_t>(stats.active_count) + stats.wire_count + stats.compressor_page_count;
        facts.memoryUsed = pages * static_cast<std::int64_t>(pageSize);
    }
    xsw_usage swap{};
    std::size_t swapSize = sizeof swap;
    if (sysctlbyname("vm.swapusage", &swap, &swapSize, nullptr, 0) == 0) {
        facts.swapTotal = static_cast<std::int64_t>(swap.xsu_total);
        facts.swapUsed = static_cast<std::int64_t>(swap.xsu_used);
    }
#elif defined(__linux__)
    facts.osName = "Linux";
    utsname name{};
    if (uname(&name) == 0) {
        facts.osVersion = name.release;
    }
    {
        std::ifstream cpuinfo{"/proc/cpuinfo"};
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.starts_with("model name")) {
                const std::size_t colon = line.find(':');
                facts.cpuModel = colon == std::string::npos ? "" : line.substr(colon + 2);
                break;
            }
        }
    }
    {
        std::ifstream file{"/proc/meminfo"};
        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string text = buffer.str();
        facts.memoryTotal = meminfoKb(text, "MemTotal");
        const auto available = meminfoKb(text, "MemAvailable");
        if (facts.memoryTotal && available) {
            facts.memoryUsed = *facts.memoryTotal - *available;
        }
        facts.swapTotal = meminfoKb(text, "SwapTotal");
        const auto swapFree = meminfoKb(text, "SwapFree");
        if (facts.swapTotal && swapFree) {
            facts.swapUsed = *facts.swapTotal - *swapFree;
        }
    }
#elif defined(_WIN32)
    facts.osName = "Windows";
#endif
    std::error_code error;
    const std::filesystem::space_info space = std::filesystem::space(diskPath.empty() ? "." : diskPath, error);
    if (!error && space.capacity > 0) {
        facts.diskTotal = static_cast<std::int64_t>(space.capacity);
        facts.diskUsed = static_cast<std::int64_t>(space.capacity - space.free);
    }
    return facts;
}

std::optional<double> SystemCpuMeter::percent() {
    std::uint64_t busy = 0;
    std::uint64_t total = 0;
#if defined(__APPLE__)
    host_cpu_load_info_data_t load{};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, reinterpret_cast<host_info_t>(&load), &count) !=
        KERN_SUCCESS) {
        return std::nullopt;
    }
    for (int state = 0; state < CPU_STATE_MAX; ++state) {
        total += load.cpu_ticks[state];
    }
    busy = total - load.cpu_ticks[CPU_STATE_IDLE];
#elif defined(__linux__)
    std::ifstream stat{"/proc/stat"};
    std::string label;
    std::uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    if (!(stat >> label >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal)) {
        return std::nullopt;
    }
    total = user + nice + system + idle + iowait + irq + softirq + steal;
    busy = total - idle - iowait;
#else
    return std::nullopt;
#endif
    const std::scoped_lock lock{mutex_};
    const bool hadBaseline = primed_;
    const std::uint64_t busyDelta = busy - busy_;
    const std::uint64_t totalDelta = total - total_;
    busy_ = busy;
    total_ = total;
    primed_ = true;
    if (!hadBaseline || totalDelta == 0) {
        return std::nullopt;
    }
    return std::clamp(static_cast<double>(busyDelta) * 100.0 / static_cast<double>(totalDelta), 0.0, 100.0);
}

} // namespace basalt::server::runtime::timings
