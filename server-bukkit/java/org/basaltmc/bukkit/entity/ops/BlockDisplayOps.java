package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.item.Nbt;
import org.bukkit.Bukkit;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.BlockDisplay;

public interface BlockDisplayOps extends BlockDisplay, Cored {
    default BlockData getBlock() {
        BlockData stored = EntityNbt.blockData(Nbt.map(EntityNbt.compound(core(), "").get("block_state")));
        return stored == null ? Bukkit.createBlockData("minecraft:air") : stored;
    }

    default void setBlock(BlockData block) {
        if (block == null) {
            throw new IllegalArgumentException("Block cannot be null");
        }
        EntityNbt.put(core(), "block_state", EntityNbt.blockState(block));
    }
}
