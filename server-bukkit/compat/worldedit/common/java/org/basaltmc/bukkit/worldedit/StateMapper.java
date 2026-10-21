package org.basaltmc.bukkit.worldedit;

import com.sk89q.worldedit.world.block.BlockState;

/** Translates between a WorldEdit block state and a Basalt block state id. */
public interface StateMapper {
    int toState(BlockState state);

    BlockState toBlock(int state);
}
