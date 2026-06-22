package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.Minecart;

public interface MinecartOps extends Minecart, Cored {
    default org.bukkit.block.data.BlockData getDisplayBlockData() {
        org.bukkit.block.data.BlockData stored = EntityNbt.blockData(org.basaltmc.bukkit.item.Nbt.map(
            EntityNbt.compound(core(), "").get("DisplayState")));
        return stored != null ? stored : org.bukkit.Bukkit.createBlockData("minecraft:air");
    }

    default void setDisplayBlockData(org.bukkit.block.data.BlockData data) {
        if (data == null || data.getMaterial().isAir()) {
            core().nbtRemove("DisplayState");
        } else {
            EntityNbt.put(core(), "DisplayState", EntityNbt.blockState(data));
        }
    }

    default org.bukkit.material.MaterialData getDisplayBlock() {
        return new org.bukkit.material.MaterialData(getDisplayBlockData().getMaterial());
    }

    default void setDisplayBlock(org.bukkit.material.MaterialData data) {
        setDisplayBlockData(data == null ? null : org.bukkit.Bukkit.createBlockData(data.getItemType()));
    }

    default int getDisplayBlockOffset() {
        return core().nbtInt("DisplayOffset", 6);
    }

    default void setDisplayBlockOffset(int offset) {
        core().nbtSet("DisplayOffset", "int", offset);
    }

    default org.bukkit.Material getMinecartMaterial() {
        return switch (getType()) {
            case CHEST_MINECART -> org.bukkit.Material.CHEST_MINECART;
            case FURNACE_MINECART -> org.bukkit.Material.FURNACE_MINECART;
            case TNT_MINECART -> org.bukkit.Material.TNT_MINECART;
            case HOPPER_MINECART -> org.bukkit.Material.HOPPER_MINECART;
            case COMMAND_BLOCK_MINECART -> org.bukkit.Material.COMMAND_BLOCK_MINECART;
            default -> org.bukkit.Material.MINECART;
        };
    }
}
