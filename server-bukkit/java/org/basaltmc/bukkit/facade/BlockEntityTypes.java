package org.basaltmc.bukkit.facade;

import java.util.HashMap;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.block.entity.BlockEntity;
import org.basaltmc.bukkit.WorldNatives;

public final class BlockEntityTypes {
    private static volatile Map<String, String> kTypes;

    private BlockEntityTypes() {}

    private static Map<String, String> types() {
        Map<String, String> found = kTypes;
        if (found == null) {
            String[] pairs = WorldNatives.blockEntityTypes();
            found = new HashMap<>();
            for (int i = 0; i + 1 < pairs.length; i += 2) {
                found.put(pairs[i], pairs[i + 1]);
            }
            kTypes = found;
        }
        return found;
    }

    public static BlockEntity detached(Identifier block, BlockPos pos) {
        String type = types().get(block.toString());
        if (type == null) {
            return null;
        }
        CompoundTag tag = new CompoundTag();
        tag.putString("id", type);
        tag.putInt("x", pos.getX());
        tag.putInt("y", pos.getY());
        tag.putInt("z", pos.getZ());
        return new BlockEntity(null, pos, tag);
    }
}
