#include <basalt/server/lua/LuaFailure.hpp>

namespace basalt::server::lua {
bool LuaFailure::outOfBudget() const {
    return message.find(failureMessages::kBudget) != std::string::npos ||
           message.find(failureMessages::kInterrupted) != std::string::npos;
}

bool LuaFailure::outOfMemory() const { return message.find(failureMessages::kMemory) != std::string::npos; }
} // namespace basalt::server::lua
