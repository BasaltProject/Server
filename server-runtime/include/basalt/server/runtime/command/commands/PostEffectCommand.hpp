#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: 26.3 `PostEffectCommand`: add, clear, list and remove the post-processing shaders a
/// player's client runs. The list is sent whole whenever it changes, as `ServerPlayer` does.
class PostEffectCommand final : public Command {
public:
    PostEffectCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
