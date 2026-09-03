#pragma once

#include <basalt/server/runtime/command/CommandDispatcher.hpp>

namespace basalt::server::runtime::command::commands {

/// parity: `CloneCommands`, minus its block entity copying.
///
/// `force` is what makes an overlapping copy legal, so the source is read whole before anything is
/// written: reading and writing position by position would have a moving region copy itself.
class CloneCommand final : public Command {
public:
    CloneCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `FillBiomeCommand`, which works in quarts: biomes are stored one per 4x4x4 block, so a
/// region smaller than that still changes the whole quart it falls in, exactly as vanilla does.
///
/// The change reaches clients through `chunks_biomes` rather than a chunk resend, which is what
/// vanilla sends here too: only the biome containers are replaced, so the column does not blink.
class FillBiomeCommand final : public Command {
public:
    FillBiomeCommand();

    void build(const CommandNodeBuilder& root) const override;
};

/// parity: `ForceLoadCommand`, whose coordinates are block coordinates that name a chunk, which is
/// why `/forceload add 0 0` forces chunk 0,0 and `/forceload add 16 16` forces chunk 1,1.
class ForceLoadCommand final : public Command {
public:
    ForceLoadCommand();

    void build(const CommandNodeBuilder& root) const override;
};

} // namespace basalt::server::runtime::command::commands
