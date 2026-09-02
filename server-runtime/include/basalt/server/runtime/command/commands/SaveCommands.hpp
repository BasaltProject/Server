#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// Whether the periodic sweep is allowed to write, and how to force one. Held by the server, since
/// both answers belong to the thing that owns the tick loop.
class SaveControl {
public:
    explicit SaveControl(std::function<void()> saveNow) : saveNow_(std::move(saveNow)) {}

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

    /// Answers false when it was already in that state, which is what `/save-off` refuses on.
    bool setEnabled(bool value);

    void save() const;

private:
    std::function<void()> saveNow_;
    bool enabled_ = true;
};

/// parity: `SaveAllCommand`. `flush` means the same thing it does in vanilla: write and wait.
class SaveAllCommand final : public Command {
public:
    explicit SaveAllCommand(SaveControl& control);

    void build(const CommandNodeBuilder& root) const override;

private:
    SaveControl* control_;
};

/// parity: `SaveOffCommand`, which refuses when saving is already off rather than saying nothing.
class SaveOffCommand final : public Command {
public:
    explicit SaveOffCommand(SaveControl& control);

    void build(const CommandNodeBuilder& root) const override;

private:
    SaveControl* control_;
};

/// parity: `SaveOnCommand`.
class SaveOnCommand final : public Command {
public:
    explicit SaveOnCommand(SaveControl& control);

    void build(const CommandNodeBuilder& root) const override;

private:
    SaveControl* control_;
};

} // namespace basalt::server::runtime::command::commands
