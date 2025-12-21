#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `LocateCommand`, all three forms. `biome` is searched the way vanilla searches, a
/// widening square scan at quart resolution around the runner; `structure` walks the structure
/// set's placement grid outward and tests each candidate exactly as generation would; `poi` looks
/// for the blocks the type is made of.
///
/// The difference in `poi` is where it looks. Vanilla keeps a point-of-interest index per chunk,
/// written as chunks generate, and asks that; this build has no such index, so it reads the chunks
/// themselves, which is why it only finds what is loaded. What makes that affordable is the
/// palette: a section that does not hold the block at all is skipped without reading a single one.
class LocateCommand final : public Command {
public:
    LocateCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
