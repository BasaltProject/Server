package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.TNTPrimed;

public interface TNTPrimedOps extends TNTPrimed, Cored {
    default int getFuseTicks() {
        return core().integer("fuse");
    }

    default void setFuseTicks(int ticks) {
        core().set("fuse", ticks);
    }

    default org.bukkit.block.data.BlockData getBlockData() {
        org.bukkit.block.data.BlockData stored = EntityNbt.blockData(org.basaltmc.bukkit.item.Nbt.map(
            EntityNbt.compound(core(), "").get("block_state")));
        return stored != null ? stored : org.bukkit.Bukkit.createBlockData(org.bukkit.Material.TNT);
    }

    default void setBlockData(org.bukkit.block.data.BlockData data) {
        EntityNbt.put(core(), "block_state", EntityNbt.blockState(data));
    }

    default org.bukkit.entity.Entity getSource() {
        return core().entity(core().integer("owner_id")) instanceof org.bukkit.entity.LivingEntity source ? source
            : null;
    }

    default void setSource(org.bukkit.entity.Entity source) {
        core().set("owner_id", source instanceof org.bukkit.entity.LivingEntity living ? living.getEntityId() : -1);
    }
}
