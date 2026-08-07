#include <basalt/server/runtime/ServerWatchdog.hpp>

#include <utility>

namespace basalt::server::runtime {

ServerWatchdog::ServerWatchdog(std::chrono::milliseconds limit, std::function<std::int64_t()> ticksRun,
                               std::function<void(double stalledSeconds)> onStall, std::chrono::milliseconds poll)
    : thread_([limit, poll, ticksRun = std::move(ticksRun), onStall = std::move(onStall)](std::stop_token stop) {
          using Clock = std::chrono::steady_clock;
          std::int64_t seen = ticksRun();
          Clock::time_point since = Clock::now();
          while (!stop.stop_requested()) {
              std::this_thread::sleep_for(poll);
              const std::int64_t now = ticksRun();
              if (now != seen) {
                  seen = now;
                  since = Clock::now();
                  continue;
              }
              const auto stalled = Clock::now() - since;
              if (stalled > limit) {
                  onStall(std::chrono::duration<double>(stalled).count());
                  return;
              }
          }
      }) {}

} // namespace basalt::server::runtime
