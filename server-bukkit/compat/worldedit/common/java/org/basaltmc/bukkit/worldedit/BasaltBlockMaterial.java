package org.basaltmc.bukkit.worldedit;

import com.sk89q.jnbt.CompoundTag;
import com.sk89q.worldedit.world.registry.BlockMaterial;
import org.basaltmc.bukkit.WorldNatives;

/** What FAWE asks about a block, answered from the flags and hardness the server keeps for the default state. */
public abstract class BasaltBlockMaterial implements BlockMaterial {
    private final int flags;
    private final double[] hardness;

    public BasaltBlockMaterial(String id, int defaultState) {
        flags = defaultState < 0 ? 0 : WorldNatives.stateFlags(defaultState);
        double[] facts = WorldNatives.blockHardness(id);
        hardness = facts == null ? new double[] {0, 0, 0} : facts;
    }

    private boolean flag(int bit) {
        return (flags & bit) != 0;
    }

    @Override
    public boolean isAir() {
        return flag(256);
    }

    @Override
    public boolean isFullCube() {
        return flag(4);
    }

    @Override
    public boolean isOpaque() {
        return flag(2);
    }

    @Override
    public boolean isPowerSource() {
        return false;
    }

    @Override
    public boolean isLiquid() {
        return flag(16);
    }

    @Override
    public boolean isSolid() {
        return flag(1);
    }

    @Override
    public float getHardness() {
        return (float) hardness[0];
    }

    @Override
    public float getResistance() {
        return (float) hardness[1];
    }

    @Override
    public float getSlipperiness() {
        return 0.6f;
    }

    @Override
    public int getLightValue() {
        return (flags >> 9) & 15;
    }

    @Override
    public boolean isFragileWhenPushed() {
        return false;
    }

    @Override
    public boolean isUnpushable() {
        return hardness[0] < 0;
    }

    @Override
    public boolean isTicksRandomly() {
        return flag(128);
    }

    @Override
    public boolean isMovementBlocker() {
        return flag(1);
    }

    @Override
    public boolean isBurnable() {
        return false;
    }

    @Override
    public boolean isToolRequired() {
        return hardness[2] != 0;
    }

    @Override
    public boolean isReplacedDuringPlacement() {
        return flag(8);
    }

    @Override
    public boolean isTranslucent() {
        return !flag(2);
    }

    @Override
    public boolean hasContainer() {
        return false;
    }

    public int getLightOpacity() {
        return (flags >> 13) & 31;
    }

    public boolean isTile() {
        return false;
    }

    public CompoundTag getDefaultTile() {
        return null;
    }

    public int getMapColor() {
        return 0;
    }
}
