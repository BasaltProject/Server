#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

class SaveControl {
public:
    explicit SaveControl(std::function<void()> saveNow) : saveNow_(std::move(saveNow)) {}

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }

    bool setEnabled(bool value);

    void save() const;

private:
    std::function<void()> saveNow_;
    bool enabled_ = true;
};

/// parity: `SaveAllCommand`.
class SaveAllCommand final : public Command {
public:
    explicit SaveAllCommand(SaveControl& control);

    void build(const CommandNodeBuilder& root) const override;

private:
    SaveControl* control_;
};

/// parity: `SaveOffCommand`.
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
