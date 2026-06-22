package org.basaltmc.bukkit.entity.ops;

import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.entity.Cored;
import org.basaltmc.bukkit.facade.NbtCodec;
import org.bukkit.block.data.BlockData;
import org.bukkit.entity.Enderman;

public interface EndermanOps extends Enderman, Cored {
    default BlockData getCarriedBlock() {
        byte[] stored = core().nbt("carriedBlockState");
        CompoundTag state = stored == null ? null : NbtCodec.fromBytes(stored);
        if (state == null || !state.contains("Name")) {
            return null;
        }
        StringBuilder text = new StringBuilder(state.getStringOr("Name", "minecraft:air"));
        CompoundTag properties = state.getCompoundOrEmpty("Properties");
        if (!properties.isEmpty()) {
            text.append('[');
            boolean first = true;
            for (String key : properties.keySet()) {
                text.append(first ? "" : ",").append(key).append('=').append(properties.getStringOr(key, ""));
                first = false;
            }
            text.append(']');
        }
        return org.bukkit.Bukkit.createBlockData(text.toString());
    }

    default void setCarriedBlock(BlockData block) {
        CompoundTag patch = new CompoundTag();
        if (block != null && !block.getMaterial().isAir()) {
            String text = block.getAsString();
            int open = text.indexOf('[');
            CompoundTag state = new CompoundTag();
            state.putString("Name", open < 0 ? text : text.substring(0, open));
            if (open >= 0) {
                CompoundTag properties = new CompoundTag();
                for (String pair : text.substring(open + 1, text.length() - 1).split(",")) {
                    String[] parts = pair.split("=", 2);
                    properties.putString(parts[0], parts[1]);
                }
                state.put("Properties", properties);
            }
            patch.put("carriedBlockState", state);
            core().mergeNbt(NbtCodec.toBytes(patch));
        } else {
            core().nbtSet("carriedBlockState", "string", "");
        }
    }

    default org.bukkit.material.MaterialData getCarriedMaterial() {
        BlockData carried = getCarriedBlock();
        return carried == null ? null : new org.bukkit.material.MaterialData(carried.getMaterial());
    }

    default void setCarriedMaterial(org.bukkit.material.MaterialData data) {
        setCarriedBlock(data == null ? null : org.bukkit.Bukkit.createBlockData(data.getItemType()));
    }
}
