// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core;
public interface Directional {
    default int getStepX() {
        return getStep().getX();
    }
    default int getStepY() {
        return getStep().getY();
    }
    default int getStepZ() {
        return getStep().getZ();
    }
    default Vec3i getStep() {
        return new Vec3i(getStepX(), getStepY(), getStepZ());
    }
}
