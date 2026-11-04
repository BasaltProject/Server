package org.basaltmc.bukkit.facade;

import net.minecraft.world.level.block.entity.BlockEntity;
import org.bukkit.block.Lidded;

public final class LidControl {
    private LidControl() {}

    private static Lidded lidOf(BlockEntity entity) {
        if (entity.getLevel() == null || entity.getLevel().getWorld() == null) {
            return null;
        }
        var pos = entity.getBlockPos();
        return entity.getLevel().getWorld().getBlockAt(pos.getX(), pos.getY(), pos.getZ()).getState() instanceof Lidded lidded
            ? lidded : null;
    }

    public static void set(BlockEntity entity, boolean open) {
        Lidded lidded = lidOf(entity);
        if (lidded == null) {
            return;
        }
        if (open) {
            lidded.open();
        } else {
            lidded.close();
        }
    }

    public static float openness(BlockEntity entity) {
        Lidded lidded = lidOf(entity);
        return lidded != null && lidded.isOpen() ? 1.0F : 0.0F;
    }
}
